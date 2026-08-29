#pragma once

#include <stdint.h>
#include <vector>

struct ID2D1Bitmap1;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;
struct IDWriteFactory;
struct IDWriteTextFormat;

class FontManager;

// 픽셀 값 라벨을 텍스처 몇 장에 미리 구워 두고 잘라 쓴다.
//
// 왜 이렇게까지 하는가는 전부 실측에서 나왔다.
//
//  - 셀마다 DrawTextLayout 을 부르면 하드웨어 D2D 타깃에서 호출당 9.4us 다
//    (글리프 런 분석 + 아틀라스 조회). 416셀에 5.98ms 였다.
//  - 값을 한 번 그려 비트맵으로 남기고 셀마다 DrawBitmap 만 하면 그 1/12 이다.
//  - 그런데 값마다 비트맵을 따로 만들면 값 하나당 텍스처 할당 한 번과
//    플러시 한 번이라 개당 0.22ms 가 든다(Release). 한 장에 몰아 구우면
//    그게 장당 한 번이 된다.
//  - 마지막 함정은 배율이다. 폰트 크기를 배율에 연속으로 붙여 두면 줌 스텝
//    마다 캐시가 통째로 낡아서, 값이 수백 종인 실이미지에서는 한 프레임에
//    수십 ms 를 다시 굽게 된다.
//
// 그래서 크기를 네 단계로 끊는다. 단계 안에서는 다시 굽지 않고 축소해서
// 그린다. 단계는 그 구간의 최대 크기로 굽는다 — 텍스트는 키우면 흐려지지만
// 줄이면 훨씬 덜 티가 난다. 확대는 하지 않는다.
//
// 메모리(칸 여백 포함, 실측 기준 추정):
//   8bit  네 단계 두 벌 전부      약 5MB   (미리 굽는다)
//   16bit 네 단계 두 벌 최대치    약 35MB  (화면에 나온 값만 채운다)
// 16bit 를 전부 미리 구우면 433MB 라 불가능하다. 격자를 끄면 전부 놓는다.
class ValueLabelAtlas
{
public:
	// 폰트 크기 단계 수.
	//
	// 실제로 쓰이는 크기 범위는 10.9~20 (1.83배) 뿐이다. 임계 배율에서
	// 10.9 로 시작해 8bit 는 47배율, 16bit 는 78배율에서 20 에 물린다.
	// 네 단계면 단계당 1.16배라 단계 안에서 최대 16% 축소로 끝난다.
	static constexpr int kLodCount = 4;

	// 아틀라스 한 칸. 단위는 전부 DIP 다.
	struct Slot
	{
		ID2D1Bitmap1* texture = nullptr;

		float srcX = 0.0f;      // 텍스처 안에서의 위치
		float srcY = 0.0f;
		float width = 0.0f;     // 글자 상자 크기
		float height = 0.0f;
	};

	ValueLabelAtlas() = default;
	~ValueLabelAtlas();

	ValueLabelAtlas(const ValueLabelAtlas&) = delete;
	ValueLabelAtlas& operator=(const ValueLabelAtlas&) = delete;

	// engineContext 는 D2D 디바이스를 얻기 위해서만 쓴다. 굽는 것은 여기서
	// 따로 만든 컨텍스트다.
	bool Initialize(ID2D1DeviceContext* engineContext, FontManager* fontManager);
	void Shutdown();

	bool IsReady() const;

	// 값 범위가 바뀌면(8bit <-> 16bit) 전부 버리고 다시 잡는다.
	//
	// 8bit(256값)는 네 단계를 두 벌 다 여기서 미리 굽는다. 16bit(65536값)는
	// 미리 구울 수 없어 화면에 나온 값만 채운다.
	//
	// ★ 프레임의 BeginDraw 바깥에서만 부를 것.
	bool SetValueRange(uint32_t maxValue);

	// 필요한 크기 이상인 단계를 고른다. 축소만 하고 확대는 하지 않는다.
	int SelectLod(float desiredFontSize) const;

	// 그 단계가 실제로 구워진 크기. 그리는 쪽이 축소 배율을 여기서 낸다.
	float GetLodFontSize(int lod) const;

	// 한 프레임 분의 Reserve 를 시작한다. 지난번에 칸이 모자랐던 벌을 여기서
	// 비운다. 그리는 도중에 비우면 이미 자리를 받은 값이 사라지기 때문이다.
	void BeginPass();

	// 값 하나를 이 단계에 넣어 둔다. 실제로 굽는 것은 Flush 다.
	void Reserve(int lod, uint32_t value, bool darkText);

	// Reserve 로 쌓인 것들을 텍스처에 굽는다. 벌 하나당 BeginDraw 한 번이다.
	//
	// ★ 프레임의 BeginDraw 바깥에서만 부를 것. 부모 컨텍스트가 그리는 중에
	//    오프스크린 타깃에 그리면 D2D 가 순서를 보장하지 않아 내용이 빈다.
	void Flush();

	// 조회만 한다. 그리는 쪽에서 쓴다.
	bool Find(int lod, uint32_t value, bool darkText, Slot& out) const;

	// 안 쓸 때는 텍스처를 들고 있지 않는다. 16bit 는 수십 MB 가 될 수 있다.
	void ReleaseTextures();

private:
	// (단계, 색) 하나 = 텍스처 한 장.
	struct Page
	{
		ID2D1Bitmap1* texture = nullptr;

		// 값 -> 칸 번호. 없으면 -1. 이 벌을 실제로 쓸 때만 잡는다
		// (8bit 1KB, 16bit 256KB).
		std::vector<int32_t> slotOfValue;

		// 칸 번호 -> 값. 굽는 데 필요하다.
		std::vector<uint32_t> valueOfSlot;

		// 이번 패스에 새로 자리를 받아 아직 안 구운 칸들.
		std::vector<int32_t> pending;

		int32_t used = 0;
		int32_t capacity = 0;
		int32_t columns = 0;

		float slotW = 0.0f;     // 칸 크기 (여백 포함)
		float slotH = 0.0f;
		float textW = 0.0f;     // 글자 상자 크기
		float textH = 0.0f;

		// 칸이 모자라서 다음 패스에 비워야 한다.
		bool overflowed = false;

		// 텍스처를 새로 잡아 아직 지우지 않았다.
		bool needsClear = false;
	};

	int PageIndex(int lod, bool darkText) const;

	bool EnsurePageMetrics(Page& page, int lod);
	bool EnsurePageTexture(Page& page);
	void ResetPage(Page& page);
	void ReleasePage(Page& page);

	// 8bit 일 때 네 단계 두 벌을 전부 채운다.
	void PrebuildAllValues();

private:
	ID2D1DeviceContext* m_glyphContext = nullptr;
	FontManager* m_fontManager = nullptr;
	IDWriteFactory* m_dwrite = nullptr;

	ID2D1SolidColorBrush* m_darkBrush = nullptr;
	ID2D1SolidColorBrush* m_lightBrush = nullptr;

	// 단계별 텍스트 포맷. 크기가 고정이라 네 개면 된다.
	IDWriteTextFormat* m_formats[kLodCount] = {};

	// [단계][색] 순서. 색은 0 = 흰 글자, 1 = 검은 글자.
	std::vector<Page> m_pages;

	uint32_t m_maxValue = 0;
	uint32_t m_digits = 1;
	int32_t m_capacity = 0;

	// 8bit 를 전부 구워 뒀는가. 텍스처를 놓으면 다시 구워야 한다.
	bool m_prebuilt = false;
};
