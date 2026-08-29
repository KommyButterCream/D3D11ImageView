#include "pch.h"
#include "ValueLabelAtlas.h"

#include "../../../Module/D3D11Engine/Font/FontManager.h"

#include <stdio.h>
#include <math.h>

namespace
{
	// 단계별로 구워 둘 폰트 크기. 맨 위 20 에서 1.163배씩 내려온다.
	//
	// 실사용 하한이 10.9 라 맨 아래 단계에서도 축소는 14% 를 넘지 않는다.
	constexpr float kLodFontSize[ValueLabelAtlas::kLodCount] =
	{
		12.7f, 14.8f, 17.2f, 20.0f
	};

	// 8bit 는 값이 256가지뿐이라 전부 담는다.
	constexpr int32_t k8BitCapacity = 256;

	// 16bit 는 화면에 나온 값만 담는다.
	//
	// 임계가 42.5px/셀이라 2560x1440 창에서도 2000셀을 넘지 않고, 그게 밝기
	// 기준으로 두 벌에 갈려 담기므로 벌당 1024면 넉넉하다. 모자라면 그 벌을
	// 다음 패스에 비우고 다시 채운다.
	constexpr int32_t k16BitCapacity = 1024;

	// 칸 여백. 글리프가 조금 삐져나와도 옆 칸을 침범하지 않게 한다.
	constexpr float kSlotPadding = 2.0f;

	uint32_t DigitCountOf(uint32_t maxValue)
	{
		uint32_t digits = 1;
		while (maxValue >= 10)
		{
			maxValue /= 10;
			++digits;
		}
		return digits;
	}
}

ValueLabelAtlas::~ValueLabelAtlas()
{
	Shutdown();
}

bool ValueLabelAtlas::Initialize(ID2D1DeviceContext* engineContext, FontManager* fontManager)
{
	Shutdown();

	if (!engineContext || !fontManager)
		return false;

	m_fontManager = fontManager;
	m_dwrite = fontManager->GetDWriteFactory();

	if (!m_dwrite)
		return false;

	// 굽기 전용 컨텍스트.
	//
	// 엔진 컨텍스트를 그대로 쓰면 안 된다. 그쪽은
	// ENABLE_MULTITHREADED_OPTIMIZATIONS 로 만들어져 있고 프레임 중에는
	// 타깃이 물려 있다. 같은 디바이스에서 컨텍스트를 하나 더 만드는 것이
	// D2D1.1 의 정식 경로다.
	ID2D1Device* device = nullptr;
	engineContext->GetDevice(&device);

	if (device)
	{
		device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_glyphContext);
		device->Release();
	}

	if (!m_glyphContext)
		return false;

	// 알파가 있는 타깃에 ClearType 은 쓸 수 없다.
	m_glyphContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

	if (FAILED(m_glyphContext->CreateSolidColorBrush(
		D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &m_darkBrush)))
	{
		Shutdown();
		return false;
	}

	if (FAILED(m_glyphContext->CreateSolidColorBrush(
		D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_lightBrush)))
	{
		Shutdown();
		return false;
	}

	for (int lod = 0; lod < kLodCount; ++lod)
	{
		// FontManager 소유라 해제하지 않는다. 참조만 들고 있는다.
		m_formats[lod] = m_fontManager->GetTextFormat(
			L"Consolas", kLodFontSize[lod], DWRITE_FONT_WEIGHT_NORMAL);

		if (!m_formats[lod])
		{
			Shutdown();
			return false;
		}
	}

	m_pages.resize(static_cast<size_t>(kLodCount) * 2);

	return true;
}

void ValueLabelAtlas::Shutdown()
{
	for (Page& page : m_pages)
	{
		ReleasePage(page);
	}

	m_pages.clear();

	for (int lod = 0; lod < kLodCount; ++lod)
	{
		m_formats[lod] = nullptr;
	}

	SafeRelease(m_lightBrush);
	SafeRelease(m_darkBrush);

	if (m_glyphContext)
	{
		m_glyphContext->SetTarget(nullptr);
	}

	SafeRelease(m_glyphContext);

	m_fontManager = nullptr;
	m_dwrite = nullptr;

	m_maxValue = 0;
	m_digits = 1;
	m_capacity = 0;
	m_prebuilt = false;
}

bool ValueLabelAtlas::IsReady() const
{
	return m_glyphContext != nullptr && !m_pages.empty();
}

int ValueLabelAtlas::PageIndex(int lod, bool darkText) const
{
	return lod * 2 + (darkText ? 1 : 0);
}

float ValueLabelAtlas::GetLodFontSize(int lod) const
{
	if (lod < 0) lod = 0;
	if (lod >= kLodCount) lod = kLodCount - 1;

	return kLodFontSize[lod];
}

int ValueLabelAtlas::SelectLod(float desiredFontSize) const
{
	// 필요한 크기 이상인 첫 단계. 확대는 하지 않는다.
	for (int lod = 0; lod < kLodCount; ++lod)
	{
		if (kLodFontSize[lod] >= desiredFontSize)
			return lod;
	}

	// 맨 위보다 크게 필요하면 맨 위를 쓴다. 폰트 크기가 20 에서 물리므로
	// 여기 오는 것은 그 경계뿐이다.
	return kLodCount - 1;
}

void ValueLabelAtlas::ReleasePage(Page& page)
{
	SafeRelease(page.texture);

	page.slotOfValue.clear();
	page.slotOfValue.shrink_to_fit();
	page.valueOfSlot.clear();
	page.valueOfSlot.shrink_to_fit();
	page.pending.clear();

	page.used = 0;
	page.capacity = 0;
	page.columns = 0;

	page.slotW = 0.0f;
	page.slotH = 0.0f;
	page.textW = 0.0f;
	page.textH = 0.0f;

	page.overflowed = false;
	page.needsClear = false;
}

void ValueLabelAtlas::ResetPage(Page& page)
{
	// 텍스처는 그대로 두고 내용만 버린다. 다시 채워질 때 지운다.
	page.slotOfValue.assign(page.slotOfValue.size(), -1);
	page.valueOfSlot.clear();
	page.pending.clear();

	page.used = 0;
	page.overflowed = false;
	page.needsClear = (page.texture != nullptr);
}

void ValueLabelAtlas::ReleaseTextures()
{
	for (Page& page : m_pages)
	{
		ReleasePage(page);
	}

	m_prebuilt = false;
}

bool ValueLabelAtlas::SetValueRange(uint32_t maxValue)
{
	if (!IsReady())
		return false;

	if (m_maxValue != maxValue || m_capacity == 0)
	{
		ReleaseTextures();

		m_maxValue = maxValue;
		m_digits = DigitCountOf(maxValue);
		m_capacity = (maxValue < 65535u) ? k8BitCapacity : k16BitCapacity;
	}

	// 8bit 는 값이 256가지뿐이라 전부 미리 구워 둔다. 그러면 상호작용 중에는
	// 굽는 일이 아예 없다. 16bit 는 전부 구우면 433MB 라 불가능하다.
	if (!m_prebuilt && m_capacity == k8BitCapacity)
	{
		PrebuildAllValues();
		m_prebuilt = true;
	}

	return true;
}

bool ValueLabelAtlas::EnsurePageMetrics(Page& page, int lod)
{
	if (page.slotW > 0.0f)
		return true;

	if (!m_dwrite || !m_formats[lod])
		return false;

	// 가장 넓은 라벨을 기준으로 칸 크기를 잡는다. Consolas 는 고정폭이라
	// 자릿수만 맞추면 된다.
	wchar_t widest[8] = {};
	const uint32_t digits = (m_digits < 7u) ? m_digits : 7u;

	for (uint32_t i = 0; i < digits; ++i)
	{
		widest[i] = L'8';
	}

	IDWriteTextLayout* layout = nullptr;
	if (FAILED(m_dwrite->CreateTextLayout(
		widest, digits, m_formats[lod], 256.0f, 256.0f, &layout)) || !layout)
	{
		return false;
	}

	layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	DWRITE_TEXT_METRICS metrics = {};
	layout->GetMetrics(&metrics);
	layout->Release();

	page.textW = metrics.width;
	page.textH = metrics.height;

	page.slotW = ceilf(metrics.width + kSlotPadding);
	page.slotH = ceilf(metrics.height + kSlotPadding);

	page.capacity = m_capacity;
	page.columns = static_cast<int32_t>(ceilf(sqrtf(static_cast<float>(m_capacity))));

	if (page.columns < 1)
		page.columns = 1;

	return true;
}

bool ValueLabelAtlas::EnsurePageTexture(Page& page)
{
	if (page.texture)
		return true;

	if (page.columns < 1 || page.slotW <= 0.0f || page.slotH <= 0.0f)
		return false;

	const int32_t rows = (page.capacity + page.columns - 1) / page.columns;

	const UINT32 pixelW = static_cast<UINT32>(page.columns * page.slotW);
	const UINT32 pixelH = static_cast<UINT32>(rows * page.slotH);

	// D3D11 텍스처 한 변 한계. 지금 값으로는 근처에도 못 가지만, 자릿수나
	// 칸 수를 나중에 손댔을 때 조용히 실패하지 않게 막아 둔다.
	if (pixelW == 0 || pixelH == 0 || pixelW > 16384 || pixelH > 16384)
		return false;

	D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

	if (FAILED(m_glyphContext->CreateBitmap(
		D2D1::SizeU(pixelW, pixelH), nullptr, 0, &props, &page.texture)) || !page.texture)
	{
		return false;
	}

	page.needsClear = true;

	if (page.slotOfValue.size() != static_cast<size_t>(m_maxValue) + 1)
	{
		page.slotOfValue.assign(static_cast<size_t>(m_maxValue) + 1, -1);
	}

	return true;
}

void ValueLabelAtlas::BeginPass()
{
	for (Page& page : m_pages)
	{
		if (page.overflowed)
		{
			ResetPage(page);
		}
	}
}

void ValueLabelAtlas::Reserve(int lod, uint32_t value, bool darkText)
{
	if (!IsReady() || value > m_maxValue)
		return;

	if (lod < 0 || lod >= kLodCount)
		return;

	Page& page = m_pages[PageIndex(lod, darkText)];

	if (!EnsurePageMetrics(page, lod) || !EnsurePageTexture(page))
		return;

	if (page.slotOfValue[value] >= 0)
		return;

	if (page.used >= page.capacity)
	{
		// 이번 패스는 이 값을 포기한다. 그리는 도중에 비우면 이미 자리를
		// 받은 값이 같이 사라지므로, 비우는 것은 다음 패스 시작에서 한다.
		page.overflowed = true;
		return;
	}

	const int32_t slot = page.used++;

	page.slotOfValue[value] = slot;
	page.valueOfSlot.push_back(value);
	page.pending.push_back(slot);
}

void ValueLabelAtlas::Flush()
{
	if (!IsReady())
		return;

	for (size_t index = 0; index < m_pages.size(); ++index)
	{
		Page& page = m_pages[index];

		if (!page.texture)
			continue;

		if (page.pending.empty() && !page.needsClear)
			continue;

		// 벌 순서는 [단계][색] 이다. 색은 인덱스의 홀짝이다.
		ID2D1SolidColorBrush* brush = ((index & 1u) != 0) ? m_darkBrush : m_lightBrush;
		IDWriteTextFormat* format = m_formats[index / 2];

		m_glyphContext->SetTarget(page.texture);
		m_glyphContext->BeginDraw();

		if (page.needsClear)
		{
			m_glyphContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
			page.needsClear = false;
		}

		for (int32_t slot : page.pending)
		{
			if (slot < 0 || static_cast<size_t>(slot) >= page.valueOfSlot.size())
				continue;

			const uint32_t value = page.valueOfSlot[slot];

			wchar_t text[16] = {};
			swprintf_s(text, L"%u", value);

			const float x = static_cast<float>(slot % page.columns) * page.slotW;
			const float y = static_cast<float>(slot / page.columns) * page.slotH;

			const D2D1_RECT_F rect = D2D1::RectF(
				x + kSlotPadding * 0.5f, y + kSlotPadding * 0.5f,
				x + page.slotW, y + page.slotH);

			// DrawTextLayout 이 아니라 DrawText 다.
			//
			// 오프스크린 타깃에 DrawTextLayout 을 쓰면 HRESULT 는 전부 S_OK
			// 인데 아무것도 그려지지 않는다. 같은 자리에서 Clear 와
			// FillRectangle 은 정상이었고, 엔진 컨텍스트든 전용 컨텍스트든
			// 결과가 같았다. DrawText 로 바꾸면 나온다.
			m_glyphContext->DrawText(
				text, static_cast<UINT32>(wcslen(text)), format, rect, brush);
		}

		m_glyphContext->EndDraw();
		m_glyphContext->SetTarget(nullptr);

		page.pending.clear();
	}
}

bool ValueLabelAtlas::Find(int lod, uint32_t value, bool darkText, Slot& out) const
{
	if (!IsReady() || value > m_maxValue)
		return false;

	if (lod < 0 || lod >= kLodCount)
		return false;

	const Page& page = m_pages[PageIndex(lod, darkText)];

	if (!page.texture || value >= page.slotOfValue.size())
		return false;

	const int32_t slot = page.slotOfValue[value];
	if (slot < 0)
		return false;

	out.texture = page.texture;
	out.srcX = static_cast<float>(slot % page.columns) * page.slotW + kSlotPadding * 0.5f;
	out.srcY = static_cast<float>(slot / page.columns) * page.slotH + kSlotPadding * 0.5f;
	out.width = page.textW;
	out.height = page.textH;

	return true;
}

void ValueLabelAtlas::PrebuildAllValues()
{
	BeginPass();

	for (int lod = 0; lod < kLodCount; ++lod)
	{
		for (uint32_t value = 0; value <= m_maxValue; ++value)
		{
			Reserve(lod, value, false);
			Reserve(lod, value, true);
		}
	}

	Flush();
}
