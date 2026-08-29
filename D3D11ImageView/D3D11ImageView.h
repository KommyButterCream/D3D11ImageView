#pragma once

#ifdef BUILD_D3D11_IMAGE_VIEW_DLL
#define D3D11_IMAGE_VIEW_API __declspec(dllexport)
#else
#define D3D11_IMAGE_VIEW_API __declspec(dllimport)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <stdint.h>

#include "../../../Module/Core/ShapeType/Point2i.h"
#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Point2d.h"
#include "../../../Module/Core/ShapeType/Line2i.h"
#include "../../../Module/Core/ShapeType/Line2f.h"
#include "../../../Module/Core/ShapeType/Line2d.h"
#include "../../../Module/Core/ShapeType/Rect2i.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"
#include "../../../Module/Core/ShapeType/Rect2d.h"
#include "../../../Module/Core/ShapeType/QuadRect2i.h"
#include "../../../Module/Core/ShapeType/QuadRect2f.h"
#include "../../../Module/Core/ShapeType/QuadRect2d.h"
#include "../../../Module/Core/ShapeType/RotatedRect2i.h"
#include "../../../Module/Core/ShapeType/RotatedRect2f.h"
#include "../../../Module/Core/ShapeType/RotatedRect2d.h"
#include "../../../Module/Core/ShapeType/Circle2f.h"
#include "../../../Module/Core/ShapeType/Circle2d.h"
#include "../../../Module/Core/ShapeType/Ellipse2f.h"
#include "../../../Module/Core/ShapeType/Ellipse2d.h"
#include "../../../Module/Core/ShapeType/Polyline2f.h"
#include "../../../Module/Core/ShapeType/Polygon2f.h"

// ROIObjectType / ROIShapeData. 조회 API 가 해석적 형상을 그대로 내보낸다.
#include "../ROI Renderer/IROIObject.h"
#include "../Lut/LutTable.h"

using Core::ShapeType::Circle2d;
using Core::ShapeType::Circle2f;
using Core::ShapeType::Ellipse2d;
using Core::ShapeType::Ellipse2f;
using Core::ShapeType::Line2d;
using Core::ShapeType::Line2f;
using Core::ShapeType::Line2i;
using Core::ShapeType::Point2d;
using Core::ShapeType::Point2f;
using Core::ShapeType::Point2i;
using Core::ShapeType::Polygon2f;
using Core::ShapeType::Polyline2f;
using Core::ShapeType::QuadRect2d;
using Core::ShapeType::QuadRect2f;
using Core::ShapeType::QuadRect2i;
using Core::ShapeType::Rect2d;
using Core::ShapeType::Rect2f;
using Core::ShapeType::Rect2i;
using Core::ShapeType::RotatedRect2d;
using Core::ShapeType::RotatedRect2f;
using Core::ShapeType::RotatedRect2i;

struct OverlayStyle;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
class D3D11RenderEngine;

class D3D11ImageView_Impl;

class D3D11_IMAGE_VIEW_API D3D11ImageView
{
public:
	D3D11ImageView();
	~D3D11ImageView();
	D3D11ImageView(const D3D11ImageView&) = delete;
	D3D11ImageView& operator=(const D3D11ImageView&) = delete;
	D3D11ImageView(D3D11ImageView&&) = delete;
	D3D11ImageView& operator=(D3D11ImageView&&) = delete;

public:
	bool Initialize(HWND hWndParent, const RECT& rect, DWORD style, D3D11RenderEngine* D3D11Engine = nullptr);
	bool Initialize(D3D11RenderEngine* D3D11Engine, HWND hWndParent, const RECT& rect, DWORD style);

	HWND GetHWND() const;
	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;

	void RenderLock();
	void RenderUnLock();

	void InvalidateFrame();

	void ImageOverlayClear();
	void ImageOverlayShow(bool show);

	void ImageOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style);
	void ImageOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style);

	void WindowOverlayClear();
	void WindowOverlayShow(bool show);

	void WindowOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style);
	void WindowOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style);

	bool ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Line2f& line, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	void ROIClear();

	// 8bit 소스. bitDepth 를 받는 아래 오버로드에 8 을 넘기는 것과 같다.
	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);

	// bitDepth: 채널당 비트 수. 8 또는 16.
	//
	// 16bit 은 Gray(channel == 1) 만 지원한다. R16_UNORM 텍스처로 올라가고
	// 상태바에도 0~65535 원본값이 그대로 표시된다. 3채널 16bit 은 D3D11 에
	// 48bit 포맷이 없어서, 4채널 16bit 은 타일 샘플러/셰이더가 아직 다루지
	// 못해서 거절한다(false 반환).
	//
	// stride 는 바이트 단위이며 width * channel * (bitDepth / 8) 이상이어야 한다.
	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth);
	bool UpdateTexture(ID3D11Texture2D* texture);
	bool UpdateSharedTexture(HANDLE sharedHandle);

	// UpdateImage 로 넘긴 원본 버퍼 참조를 끊는다.
	//
	// 뷰어는 그 포인터를 복사하지 않고 빌려 쓰며, 렌더 스레드와 타일 워커
	// 스레드가 비동기로 읽는다. 따라서 호출자가 버퍼를 해제(또는 재할당)하기
	// 전에 반드시 이 함수를 호출해야 use-after-free 를 피할 수 있다.
	// 반환 시점 이후로 뷰어는 그 메모리를 읽지 않는다.
	//
	// 새 이미지로 교체만 할 경우에는 필요 없다. UpdateImage 가 내부적으로
	// 워커를 배수한 뒤 포인터를 바꾼다. 단, 이전 버퍼를 해제하려면
	// 교체 후에도 이 함수가 필요하다.
	void DetachImage();

	// 테스트용: 디바이스 로스트를 강제로 유발한다.
	// 리스너 통지 -> 리소스 해제 -> 디바이스 재생성 -> 복구 통지까지
	// 실제 경로가 그대로 실행된다.
	bool SimulateDeviceLost();


	// ─────────────────────────────────────────────────────────────
	// ROI 조회
	//
	// 문자열/정점은 소유권을 넘기지 않는다. 호출자 버퍼에만 쓰고,
	// 반환값은 "필요한 크기"다. buffer == nullptr 로 한 번 불러 크기를
	// 받고, 버퍼를 잡아 다시 부르는 2회 호출 패턴을 쓴다.
	// ─────────────────────────────────────────────────────────────
	uint32_t ROIGetCount() const;

	// 해석적 형상. 원/타원을 원본 파라미터 그대로 받아 무손실 왕복이 된다.
	bool ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const;

	// 정점 형상. 곡선은 segmentsPerCurve 등분해 근사한다.
	// 반환값은 필요한 정점 개수(버퍼가 작아도 그대로 알려준다).
	uint32_t ROIGetVertices(const wchar_t* key, Point2f* buffer,
		uint32_t capacity, uint32_t segmentsPerCurve = 64) const;

	bool ROIGetBounds(const wchar_t* key, Rect2f& outBounds) const;

	struct ROIInfo
	{
		ROIObjectType type = ROIObjectType::Rectangle;
		uint32_t colorRGB = 0;          // COLORREF 0x00BBGGRR
		bool isMovable = false;
		bool isResizable = false;
		bool isSelected = false;
		bool isHovered = false;
		int32_t fontSize = 0;
	};
	bool ROIGetInfo(const wchar_t* key, ROIInfo& outInfo) const;

	// 반환값은 종료 널을 포함한 필요 문자 수. 0 이면 대상이 없다.
	uint32_t ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIHitTestKey(float imageX, float imageY, float tolerance,
		wchar_t* buffer, uint32_t bufferChars) const;

	bool ROIRemove(const wchar_t* key);

	// ─────────────────────────────────────────────────────────────
	// ROI 이벤트
	//
	// 핸들러는 UI 스레드에서, ROI 내부 락 밖에서 호출된다.
	// 따라서 핸들러 안에서 위 조회 API 를 그대로 불러도 데드락이 없다.
	// ─────────────────────────────────────────────────────────────
	enum class ROIEvent : uint32_t
	{
		Selected = 0,
		Deselected,
		EditBegin,
		EditChanged,
		EditEnd,
		DoubleClicked
	};
	using ROIEventHandler = void (*)(ROIEvent event, const wchar_t* key, void* userData);
	void SetROIEventHandler(ROIEventHandler handler, void* userData);

	// ─────────────────────────────────────────────────────────────
	// 마우스 콜백
	//
	// 뷰어 내장 UI(툴바/상태바/컨텍스트 메뉴) 다음, ROI 처리 앞에서
	// 호출된다. 핸들러가 true 를 반환하면 뷰어는 그 이벤트를 처리하지
	// 않는다(팬/줌/ROI 편집 모두 건너뛴다).
	// ─────────────────────────────────────────────────────────────
	enum class MouseEventType : uint32_t
	{
		Move = 0,
		LButtonDown, LButtonUp, LButtonDoubleClick,
		RButtonDown, RButtonUp,
		MButtonDown, MButtonUp,
		Wheel,
		Leave
	};

	enum : int32_t
	{
		MouseModifier_Ctrl = 0x0001,
		MouseModifier_Shift = 0x0002,
		MouseModifier_Alt = 0x0004,

		MouseButton_Left = 0x0001,
		MouseButton_Right = 0x0002,
		MouseButton_Middle = 0x0004,
	};

	struct MouseEvent
	{
		MouseEventType type = MouseEventType::Move;
		int32_t screenX = 0;
		int32_t screenY = 0;

		// 이미지 좌표. isInsideImage == false 면 이미지 밖이므로
		// 외삽된 값이다(음수나 크기 초과가 나올 수 있다).
		float imageX = 0.0f;
		float imageY = 0.0f;
		bool isInsideImage = false;

		int32_t wheelDelta = 0;   // Wheel 일 때만 유효. WHEEL_DELTA 단위
		int32_t modifiers = 0;    // MouseModifier_* 비트합
		int32_t buttons = 0;      // MouseButton_* 비트합
	};

	using MouseHandler = bool (*)(const MouseEvent& e, void* userData);
	void SetMouseHandler(MouseHandler handler, void* userData);

	// ─────────────────────────────────────────────────────────────
	// 뷰 제어
	//
	// animate == true 면 기존 휠 줌과 같은 감쇠 애니메이션을 타고,
	// false 면 즉시 반영된다. 카메라 갱신은 다음 프레임에 반영된다.
	// ─────────────────────────────────────────────────────────────
	void SetZoom(float zoom, bool animate = true);
	float GetZoom() const;
	void ZoomFit(bool animate = true);
	void Zoom1To1(bool animate = true);

	// 뷰 중심에 놓을 이미지 좌표.
	void SetCenter(float imageX, float imageY, bool animate = true);
	void GetCenter(float& outImageX, float& outImageY) const;

	// 지정한 이미지 영역이 화면에 꽉 차도록 줌/중심을 맞춘다.
	// marginRatio 는 영역 바깥 여백 비율(0.1 = 10%).
	void ZoomToRect(const Rect2f& imageRect, float marginRatio = 0.1f, bool animate = true);

	// 현재 화면에 보이는 이미지 영역. 이미지가 없으면 false.
	bool GetVisibleImageRect(Rect2f& outRect) const;

	// ─────────────────────────────────────────────────────────────
	// 좌표 변환
	//
	// 반환값은 결과가 이미지 안인지 여부다. 밖이어도 out 값은 채워진다.
	// ─────────────────────────────────────────────────────────────
	bool ScreenToImage(int32_t screenX, int32_t screenY,
		float& outImageX, float& outImageY) const;
	bool ImageToScreen(float imageX, float imageY,
		int32_t& outScreenX, int32_t& outScreenY) const;

	// ─────────────────────────────────────────────────────────────
	// 이미지 정보
	// ─────────────────────────────────────────────────────────────
	bool GetImageSize(uint32_t& outWidth, uint32_t& outHeight) const;
	bool GetImageChannelInfo(uint32_t& outChannel, uint32_t& outBitDepth) const;

	// 원본 픽셀값. outValues 에 채널 수만큼 쓴다.
	// 8bit 은 0~255, 16bit 은 0~65535 원본 스케일이다.
	bool GetPixelValueAt(int32_t imageX, int32_t imageY,
		double* outValues, uint32_t valueCapacity, uint32_t& outChannelCount) const;

	// ─────────────────────────────────────────────────────────────
	// 표시 옵션
	// ─────────────────────────────────────────────────────────────
	// 호스트가 자체 UI 를 쓰는 경우 내장 패널을 숨긴다.
	void SetToolbarVisible(bool visible);
	void SetStatusBarVisible(bool visible);

	// 이미지 바깥 배경색. COLORREF 0x00BBGGRR.
	void SetBackgroundColor(uint32_t colorRGB);

	// 기본값 true. false 로 두면 티어링 대신 프레임이 버려진다.
	void SetVSyncEnabled(bool enable);

	// Single 모드 텍스처의 mip chain 생성 여부. 기본값 false.
	// Tiled 모드는 이 값과 무관하게 TileManager의 LOD를 사용한다.
	void SetMipMapGenerationEnabled(bool enable);
	bool IsMipMapGenerationEnabled() const;

	// 이미지 1픽셀이 실제로 몇 단위인지. 기본 1px = 1 unit.
	// Line ROI 의 길이 라벨이 이 값을 적용해 표시한다.
	// X/Y 를 따로 받는 것은 라인스캔 카메라의 비정방형 픽셀 때문이다.
	void SetPixelScale(double xScale, double yScale, const wchar_t* unit = L"px");

	// 거리 측정 도구 토글. 누를 때마다 기존 측정선을 리셋한다.
	// 활성 상태에서 이미지를 클릭하면 첫 점, 다시 클릭하면 확정된다.
	void ToggleMeasureDistance();
	bool IsMeasureActive() const;

	// 각도 측정 도구 토글. 활성 상태에서 세 번 클릭한다 —
	// 첫 점, 꼭짓점, 둘째 점 순서다. 결과는 0~180도의 사잇각이다.
	//
	// 거리 측정과 동시에 켜지지 않는다. 한쪽을 켜면 다른 쪽은 내려간다.
	void ToggleMeasureAngle();
	bool IsMeasureAngleActive() const;

	// 붙어 있는 원본 이미지를 파일로 저장한다.
	//
	// 화면 캡처가 아니다. 줌 배율, 팬 위치, ROI, 오버레이는 결과에 들어가지
	// 않는다. UpdateImage 로 넘긴 그 픽셀이 그대로 나간다.
	//
	// 형식은 확장자가 정한다 — .png / .jpg / .jpeg / .bmp / .tif.
	// 채널과 비트깊이는 원본을 따라간다. 컨테이너가 담지 못하는 조합이면
	// 가장 가까운 형태로 낮춰서 저장한다(예: JPEG 는 16bit Gray 를 8bit 로).
	//
	// 텍스처/공유 텍스처로 붙인 이미지는 CPU 원본이 없으므로 GPU 에서 읽어
	// 내려 BGRA 로 저장한다.
	//
	// 실패하면 false. 이미지가 없거나, 경로를 쓸 수 없거나, 그 조합을
	// 인코딩할 수 없는 경우다.
	bool SaveImage(const wchar_t* filePath);

	// ── 표시용 LUT ───────────────────────────────────────────────────
	//
	// 자동 대비(퍼센타일 스트레치)와 컬러맵을 함께 적용한다.
	//
	// **Gray(1채널) 이미지 전용이다.** 컬러 이미지는 이미 표시용 공간으로
	// 나온 결과라 다시 매핑할 이유가 없고, 의사색을 씌우면 실제 색을 버리게
	// 된다. 컬러가 붙어 있으면 IsLutSupported() 가 false 를 주고 켜도
	// 아무 일도 일어나지 않는다.
	//
	// 16bit 이미지에서는 사실상 필수다. 백버퍼가 8bit 라 LUT 없이는 65536
	// 계조가 256 으로 뭉개져 관심 구간이 몇 계조 안에 갇힌다.
	//
	// **표시에만 적용된다.** SaveImage 는 원본을 그대로 쓴다 — 검사 데이터에
	// 표시용 변환이 섞이면 안 되기 때문이다.
	bool IsLutSupported() const;
	void SetLutEnabled(bool enable);
	bool IsLutEnabled() const;
	void ToggleLut();

	// 프리셋을 바꾸면 LUT 가 자동으로 켜진다.
	void SetLutPreset(LutPreset preset);
	LutPreset GetLutPreset() const;

private:
	// 호스트 콜백. Impl 에는 이 인스턴스를 userData 로 넘기고
	// .cpp 의 트램폴린이 여기로 되돌린다. 그래야 공개 헤더가 내부
	// 타입(ROIRenderLayer 등)을 노출하지 않는다.
	// 트램폴린은 .cpp 에만 있다. 내부 타입을 헤더로 끌어오지 않기 위해
	// 전방 선언된 브리지 구조체에만 접근을 허용한다.
	friend struct D3D11ImageViewCallbackBridge;

	void OnROIEventInternal(uint32_t event, const wchar_t* key);
	bool OnMouseEventInternal(const void* implEventData);

	ROIEventHandler m_roiHandler = nullptr;
	void* m_roiUserData = nullptr;

	MouseHandler m_mouseHandler = nullptr;
	void* m_mouseUserData = nullptr;

	D3D11ImageView_Impl* m_impl = nullptr;
};
