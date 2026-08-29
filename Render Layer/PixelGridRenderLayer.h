#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"

#include "ValueLabelAtlas.h"

#include <stdint.h>

class IRenderContext;
class Camera2D;
class ImageRenderLayer;
class FontManager;

struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;

// 고배율에서 픽셀 격자와 값을 그린다.
//
// 상태바는 마우스가 있는 한 점만 보여준다. 결함을 판정할 때는 "이 경계가 몇
// 픽셀에 걸쳐 몇 계조 변하는가" 를 한 화면에서 동시에 봐야 하는데, 그게 이
// 레이어가 하는 일이다.
//
// 켜져 있어도 배율이 낮으면 그리지 않는다. 숫자가 안 읽히는 크기에서
// 화면 가득 텍스트를 그리는 것은 비용만 든다.
//
// 값 라벨은 직접 그리지 않고 ValueLabelAtlas 에서 잘라 붙인다. 이유는
// 그쪽 주석에 있다.
class PixelGridRenderLayer
	: public IRenderLayer
	, public IDeviceEventListener
{
public:
	PixelGridRenderLayer() = default;
	virtual ~PixelGridRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;

public:
	void SetCamera2D(const Camera2D* camera);

	// 픽셀 값을 읽어올 곳. 원본이 CPU 에 있어야 하므로 ImageBase 를 쓴다.
	void SetImageLayer(const ImageRenderLayer* imageLayer);

	void SetEnabled(bool enable);
	bool IsEnabled() const;

	// 지금 실제로 그려지는가.
	//
	// 켜져 있어도 배율이 낮거나 CPU 원본이 없으면 false 다. 툴바 버튼이
	// "켰는데 아무 일도 안 일어난다" 로 보이지 않게 호스트가 물어볼 수 있다.
	bool IsVisibleNow() const;

private:
	// 이번 프레임에 무엇을 어디에 그릴지. Prepare 와 Render 가 같은 답을
	// 봐야 하므로 한 곳에서 계산한다.
	struct GridPlan
	{
		bool drawGrid = false;      // 격자선을 그리는가
		bool drawValues = false;    // 값까지 그리는가

		float zoom = 0.0f;          // 셀 하나의 화면 크기
		uint32_t maxValue = 255u;   // 255 또는 65535
		uint32_t channel = 1u;

		int32_t left = 0;
		int32_t top = 0;
		int32_t right = 0;
		int32_t bottom = 0;

		// 이 배율에 맞는 글자 크기와, 아틀라스에서 고른 단계.
		float fontSize = 0.0f;
		int lod = 0;
	};

	// 그릴 조건인지 판단하고 보이는 픽셀 범위를 채운다.
	bool BuildPlan(GridPlan& plan) const;

	// 칸 배경이 밝으면 검은 글자, 어두우면 흰 글자.
	//
	// 임계 0.46 은 눈대중이 아니다. sRGB 회색에서 흰 글자와 검은 글자의
	// 명암비가 같아지는 지점이 여기다(값으로는 약 117). 처음에 0.55 로
	// 잡았더니 134 같은 밝은 칸에 흰 글자가 남아 거의 안 읽혔다.
	//
	// LUT 가 켜져 있으면 화면 색이 원본 값과 다르므로 이 판정이 어긋날 수
	// 있다. 특히 Inverted 는 정확히 반대가 된다. 아틀라스가 두 색을 다
	// 들고 있으므로, 고칠 때는 여기만 바꾸면 된다.
	static bool WantsDarkText(uint32_t value, uint32_t maxValue);

	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

private:
	IRenderContext* m_context = nullptr;
	const Camera2D* m_camera = nullptr;
	const ImageRenderLayer* m_imageLayer = nullptr;

	ID2D1SolidColorBrush* m_gridBrush = nullptr;

	ValueLabelAtlas m_atlas;

	bool m_enabled = false;

	// 마지막 Render 에서 실제로 그렸는가.
	bool m_visibleNow = false;

	// 격자와 값이 나타나는 최소 셀 크기. 자릿수 하나당 이만큼 필요하다.
	//
	// 8bit(3자리)는 25.5px, 16bit(5자리)는 42.5px 부터다. 65535 를 칸에
	// 넣으려면 255 보다 넓은 칸이 있어야 하므로 자릿수에 비례시킨다.
	//
	// ★ 격자선과 값은 같은 조건으로 함께 나타난다.
	//
	// 예전에는 격자가 6배율, 값이 27배율에서 따로 나왔다. 그 사이 구간은
	// 격자만 있어 어정쩡해서 하나로 합쳤다. 합칠 때는 값 드로우가 비싸서
	// 임계를 42px 까지 올려야 했는데(셀 수는 배율의 제곱에 반비례한다),
	// 값을 아틀라스에서 잘라 붙이게 된 뒤로는 그럴 필요가 없어져 25.5px 로
	// 내렸다. 줌 버튼 기준 3.3스텝 이르다(스텝은 e^0.15 배).
	float m_minCellPerDigit = 8.5f;

	// 창이 아주 크면 임계를 넘겨도 셀이 많아질 수 있다. 마지막 안전망이다.
	// 격자와 값에 함께 걸어 둘이 따로 사라지지 않게 한다.
	//
	// 실측으로 셀당 약 1.1us 다(Release, 1815셀에서 Prepare+Render 1.97ms).
	// 4000셀이면 4.4ms 로 60fps 예산 안에 든다. 1920x1080 창이 임계 배율에서
	// 3150셀이라 여기까지는 안전망에 걸리지 않는다.
	int32_t m_maxCells = 4000;
};
