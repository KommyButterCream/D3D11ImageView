#include "pch.h"
#include "PixelGridRenderLayer.h"

#include "ImageRenderLayer.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11Engine/Font/FontManager.h"

using namespace Core::ImageType;

namespace
{
	// 값이 몇 자리인가. 칸 크기 임계를 여기에 비례시킨다.
	uint32_t DigitCount(uint32_t maxValue)
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

PixelGridRenderLayer::~PixelGridRenderLayer()
{
	Shutdown();
}

bool PixelGridRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
		return false;

	m_context = context;

	if (!AcquireDeviceResources())
		return false;

	m_context->AddDeviceListener(this);

	return true;
}

void PixelGridRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}

	ReleaseDeviceResources();

	m_context = nullptr;
	m_camera = nullptr;
	m_imageLayer = nullptr;
}

bool PixelGridRenderLayer::AcquireDeviceResources()
{
	ID2D1DeviceContext* d2d = m_context ? m_context->GetD2DDeviceContext() : nullptr;
	if (!d2d)
		return false;

	if (FAILED(d2d->CreateSolidColorBrush(
		D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.35f), &m_gridBrush)))
		return false;

	auto* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	FontManager* fontManager = engine ? engine->GetFontManager() : nullptr;

	if (!m_atlas.Initialize(d2d, fontManager))
		return false;

	return true;
}

void PixelGridRenderLayer::ReleaseDeviceResources()
{
	m_atlas.Shutdown();

	SafeRelease(m_gridBrush);
}

void PixelGridRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void PixelGridRenderLayer::OnDeviceRestored()
{
	AcquireDeviceResources();
}

void PixelGridRenderLayer::SetCamera2D(const Camera2D* camera)
{
	m_camera = camera;
}

void PixelGridRenderLayer::SetImageLayer(const ImageRenderLayer* imageLayer)
{
	m_imageLayer = imageLayer;
}

void PixelGridRenderLayer::SetEnabled(bool enable)
{
	m_enabled = enable;

	if (!m_enabled)
	{
		m_visibleNow = false;

		// 꺼져 있는 동안 텍스처를 들고 있을 이유가 없다. 16bit 는 수십 MB 다.
		m_atlas.ReleaseTextures();
	}
}

bool PixelGridRenderLayer::IsEnabled() const
{
	return m_enabled;
}

bool PixelGridRenderLayer::IsVisibleNow() const
{
	return m_visibleNow;
}

bool PixelGridRenderLayer::WantsDarkText(uint32_t value, uint32_t maxValue)
{
	const float luminance = static_cast<float>(value) / static_cast<float>(maxValue);
	return luminance > 0.46f;
}

bool PixelGridRenderLayer::BuildPlan(GridPlan& plan) const
{
	if (!m_enabled || !m_context || !m_camera || !m_imageLayer)
		return false;

	// 원본이 CPU 에 있어야 값을 읽을 수 있다.
	// 텍스처 입력은 GPU 에만 있으므로 여기서는 그리지 않는다.
	const ImageBase* image = m_imageLayer->GetImage();
	if (!image || image->IsEmpty())
		return false;

	const float zoom = m_camera->GetZoom();

	const uint32_t channel = static_cast<uint32_t>(image->Channel());
	const PixelType pixelType = image->GetPixelType();
	const uint32_t maxValue = (pixelType == PixelType::U16) ? 65535u : 255u;

	// 셀 하나가 화면에서 차지하는 크기 = 배율.
	//
	// 격자와 값이 같은 임계를 쓴다. 컬러 이미지는 값이 없지만 격자만으로도
	// 쓸모가 있으므로, 자릿수는 값 표시 여부와 무관하게 임계 계산에 쓴다.
	const uint32_t digits = DigitCount(maxValue);
	const float minZoom = m_minCellPerDigit * static_cast<float>(digits);

	if (zoom < minZoom)
		return false;

	// 화면에 보이는 이미지 픽셀 범위를 구한다.
	const float viewW = static_cast<float>(m_context->GetWidth());
	const float viewH = static_cast<float>(m_context->GetHeight());

	float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
	m_camera->ScreenToImage(0.0f, 0.0f, x0, y0);
	m_camera->ScreenToImage(viewW, viewH, x1, y1);

	int32_t left = static_cast<int32_t>(floorf(x0));
	int32_t top = static_cast<int32_t>(floorf(y0));
	int32_t right = static_cast<int32_t>(ceilf(x1));
	int32_t bottom = static_cast<int32_t>(ceilf(y1));

	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > image->Width()) right = image->Width();
	if (bottom > image->Height()) bottom = image->Height();

	if (right <= left || bottom <= top)
		return false;

	const int64_t cellCount =
		static_cast<int64_t>(right - left) * static_cast<int64_t>(bottom - top);

	// 창이 아주 크면 임계를 넘겨도 셀이 많아질 수 있다.
	//
	// 격자와 값에 함께 건다. 한쪽만 사라지면 "왜 숫자만 없지" 가 된다.
	if (cellCount > m_maxCells)
		return false;

	plan.drawGrid = true;

	// 값은 단일 채널에서만. 컬러는 한 칸에 세 숫자라 읽히지 않는다.
	plan.drawValues = (channel == 1);

	plan.zoom = zoom;
	plan.maxValue = maxValue;
	plan.channel = channel;
	plan.left = left;
	plan.top = top;
	plan.right = right;
	plan.bottom = bottom;

	// 자릿수가 칸 폭의 80% 를 넘지 않게.
	float fontSize = zoom * 0.8f / static_cast<float>(digits) * 1.6f;
	if (fontSize < 7.0f) fontSize = 7.0f;
	if (fontSize > 20.0f) fontSize = 20.0f;

	plan.fontSize = fontSize;
	plan.lod = m_atlas.SelectLod(fontSize);

	return true;
}

bool PixelGridRenderLayer::Prepare()
{
	// 값 라벨은 여기서 아틀라스에 채운다. Render 는 프레임의 BeginDraw 안이라
	// 오프스크린 렌더가 통하지 않는다(ValueLabelAtlas 주석 참고).
	GridPlan plan;
	if (!BuildPlan(plan) || !plan.drawValues)
		return true;

	if (!m_atlas.SetValueRange(plan.maxValue))
		return true;

	m_atlas.BeginPass();

	const ImageBase* image = m_imageLayer->GetImage();
	const PixelType pixelType = image->GetPixelType();

	for (int32_t iy = plan.top; iy < plan.bottom; ++iy)
	{
		const uint8_t* row = image->Ptr(iy);

		for (int32_t ix = plan.left; ix < plan.right; ++ix)
		{
			const uint32_t value = (pixelType == PixelType::U16)
				? reinterpret_cast<const uint16_t*>(row)[ix]
				: row[ix];

			m_atlas.Reserve(plan.lod, value, WantsDarkText(value, plan.maxValue));
		}
	}

	m_atlas.Flush();

	return true;
}

bool PixelGridRenderLayer::Render()
{
	m_visibleNow = false;

	GridPlan plan;
	if (!BuildPlan(plan))
		return true;

	ID2D1DeviceContext* d2d = m_context->GetD2DDeviceContext();
	if (!d2d || !m_gridBrush)
		return true;

	m_visibleNow = true;

	// ── 격자선 ───────────────────────────────────────────────────────
	//
	// 픽셀 경계에 정확히 그린다. 반투명이라 이미지가 비친다.
	for (int32_t ix = plan.left; ix <= plan.right; ++ix)
	{
		float sx = 0.0f, sy = 0.0f;
		m_camera->ImageToScreen(
			static_cast<float>(ix), static_cast<float>(plan.top), sx, sy);

		float ex = 0.0f, ey = 0.0f;
		m_camera->ImageToScreen(
			static_cast<float>(ix), static_cast<float>(plan.bottom), ex, ey);

		d2d->DrawLine(D2D1::Point2F(sx, sy), D2D1::Point2F(ex, ey), m_gridBrush, 1.0f);
	}

	for (int32_t iy = plan.top; iy <= plan.bottom; ++iy)
	{
		float sx = 0.0f, sy = 0.0f;
		m_camera->ImageToScreen(
			static_cast<float>(plan.left), static_cast<float>(iy), sx, sy);

		float ex = 0.0f, ey = 0.0f;
		m_camera->ImageToScreen(
			static_cast<float>(plan.right), static_cast<float>(iy), ex, ey);

		d2d->DrawLine(D2D1::Point2F(sx, sy), D2D1::Point2F(ex, ey), m_gridBrush, 1.0f);
	}

	if (!plan.drawValues)
		return true;

	// ── 값 ───────────────────────────────────────────────────────────
	//
	// 여기서는 굽지 않고 아틀라스에서 잘라 붙이기만 한다.
	//
	// 단계는 필요한 크기 이상으로 구워져 있으므로 배율은 항상 1 이하다.
	// 텍스트는 키우면 흐려지지만 줄이면 덜 티가 난다.
	const float scale = plan.fontSize / m_atlas.GetLodFontSize(plan.lod);

	const ImageBase* image = m_imageLayer->GetImage();
	const PixelType pixelType = image->GetPixelType();

	for (int32_t iy = plan.top; iy < plan.bottom; ++iy)
	{
		const uint8_t* row = image->Ptr(iy);

		for (int32_t ix = plan.left; ix < plan.right; ++ix)
		{
			const uint32_t value = (pixelType == PixelType::U16)
				? reinterpret_cast<const uint16_t*>(row)[ix]
				: row[ix];

			ValueLabelAtlas::Slot slot;
			if (!m_atlas.Find(plan.lod, value, WantsDarkText(value, plan.maxValue), slot))
				continue;

			float sx = 0.0f, sy = 0.0f;
			m_camera->ImageToScreen(
				static_cast<float>(ix), static_cast<float>(iy), sx, sy);

			const float w = slot.width * scale;
			const float h = slot.height * scale;

			const float cx = sx + (plan.zoom - w) * 0.5f;
			const float cy = sy + (plan.zoom - h) * 0.5f;

			const D2D1_RECT_F dest = D2D1::RectF(cx, cy, cx + w, cy + h);
			const D2D1_RECT_F source = D2D1::RectF(
				slot.srcX, slot.srcY,
				slot.srcX + slot.width, slot.srcY + slot.height);

			d2d->DrawBitmap(slot.texture, &dest, 1.0f,
				D2D1_INTERPOLATION_MODE_LINEAR, &source, nullptr);
		}
	}

	return true;
}
