#pragma once

#include "../../../Module/Core/Windows/WindowBase.h"

#include <vector>
#include <memory>
#include <atomic>

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

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
#include "../../../Module/Core/ImageType/ImageBase.h"

// ROIRenderLayer::ROIInfoData / ROIEventHandler, ROIShapeData 를 쓰므로
// 전방선언으로는 부족하다.
#include "../Render Layer/ROIRenderLayer.h"

using Core::ImageType::ImageBase;
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

class D3D11RenderEngine;
class D3D11RenderContext;
class IRenderLayer;
class ImageRenderLayer;
class SelectionRectRenderLayer;

class OverlayRenderLayer;
class UIRenderLayer;
class ImageCenterRenderLayer;

class Camera2D;
class UIEventDispatcher;

struct OverlayStyle;
class RenderThread;
class TileManager;
struct ID3D11Texture2D;

enum class UICommand;
enum class UIMouseEventType : uint8_t;
enum class UIEventResult;

struct PixelValue;

enum class PendingImageUpdateType : uint8_t
{
	None,
	RawImage,
	Texture,
	SharedTexture
};

struct PendingImageUpdate
{
	PendingImageUpdateType type = PendingImageUpdateType::None;
	const uint8_t* rawData = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t stride = 0;
	uint32_t channel = 0;
	uint32_t bitDepth = 8;
	ID3D11Texture2D* texture = nullptr;
	HANDLE sharedHandle = nullptr;

	void Reset()
	{
		type = PendingImageUpdateType::None;
		rawData = nullptr;
		width = 0;
		height = 0;
		stride = 0;
		channel = 0;
		bitDepth = 8;
		texture = nullptr;
		sharedHandle = nullptr;
	}
};

class D3D11ImageView_Impl : public Core::Window::WindowBase
{
public:
	D3D11ImageView_Impl();
	~D3D11ImageView_Impl();

public:
	bool Initialize(HWND hWndParent, const RECT& rect, DWORD style, D3D11RenderEngine* D3D11Engine = nullptr);
	bool Initialize(D3D11RenderEngine* D3D11Engine, HWND hWndParent, const RECT& rect, DWORD style);
	void Finalize();

	HWND GetHWND() const;
	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;

	void RenderLock() { ::AcquireSRWLockExclusive(&m_renderLock); }
	void RenderUnLock() { ::ReleaseSRWLockExclusive(&m_renderLock); }

public:
	void InvalidateFrame();

public:
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

	bool ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	void ROIClear();

	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth = 8);
	bool UpdateTexture(ID3D11Texture2D* texture);
	bool UpdateSharedTexture(HANDLE sharedHandle);

	// Attach 된 원본 버퍼 참조를 끊는다. 자세한 계약은 D3D11ImageView.h 참조.
	void DetachImage();

	// ─────────────────────────────────────────────────────────────
	// ROI 조회 / 이벤트  (D3D11ImageView_Impl_Query.cpp)
	// ─────────────────────────────────────────────────────────────
	uint32_t ROIGetCount() const;
	bool ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const;
	uint32_t ROIGetVertices(const wchar_t* key, Point2f* buffer,
		uint32_t capacity, uint32_t segmentsPerCurve) const;
	bool ROIGetBounds(const wchar_t* key, Rect2f& outBounds) const;
	bool ROIGetInfo(const wchar_t* key, ROIRenderLayer::ROIInfoData& outInfo) const;
	uint32_t ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIHitTestKey(float imageX, float imageY, float tolerance,
		wchar_t* buffer, uint32_t bufferChars) const;
	bool ROIRemove(const wchar_t* key);

	// Initialize 전에 불러도 된다. 보관해 두었다가 레이어 생성 후 적용한다.
	void SetROIEventHandler(ROIRenderLayer::ROIEventHandler handler, void* userData);

	// Initialize 가 ROI 레이어를 만든 직후 호출한다.
	void ApplyPendingROIEventHandler();

	// ─────────────────────────────────────────────────────────────
	// 마우스 콜백
	//
	// WndProc(UI 스레드)에서 호출된다. UI 처리 다음, ROI 처리 앞이라
	// m_roiLock 밖이고 콜백에서 ROI API 를 불러도 데드락이 없다.
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

	struct MouseEventData
	{
		MouseEventType type = MouseEventType::Move;
		int32_t screenX = 0;
		int32_t screenY = 0;
		float imageX = 0.0f;
		float imageY = 0.0f;
		bool isInsideImage = false;
		int32_t wheelDelta = 0;
		int32_t modifiers = 0;
		int32_t buttons = 0;
	};

	// true 를 반환하면 뷰어는 그 이벤트를 처리하지 않는다.
	using MouseHandler = bool (*)(const MouseEventData& data, void* userData);
	void SetMouseHandler(MouseHandler handler, void* userData);

	// WndProc 에서 호출한다. true 면 호스트가 처리했으므로 뷰어는 무시한다.
	bool DispatchMouseEvent(MouseEventType type, int32_t screenX, int32_t screenY,
		int32_t wheelDelta = 0);

	// ─────────────────────────────────────────────────────────────
	// 뷰 제어 / 좌표 변환 / 이미지 정보 / 표시 옵션
	// ─────────────────────────────────────────────────────────────
	void SetZoomLevel(float zoom, bool animate);
	float GetZoomLevel() const;
	void ZoomFitProgrammatic(bool animate);
	void Zoom1To1Programmatic(bool animate);
	void SetViewCenter(float imageX, float imageY, bool animate);
	void GetViewCenter(float& outX, float& outY) const;
	void ZoomToRect(const Rect2f& imageRect, float marginRatio, bool animate);
	bool GetVisibleImageRect(Rect2f& outRect) const;

	bool ScreenToImage(int32_t screenX, int32_t screenY,
		float& outImageX, float& outImageY) const;
	bool ImageToScreen(float imageX, float imageY,
		int32_t& outScreenX, int32_t& outScreenY) const;

	bool GetImageSize(uint32_t& outWidth, uint32_t& outHeight) const;
	bool GetImageChannelInfo(uint32_t& outChannel, uint32_t& outBitDepth) const;
	bool GetPixelValueAt(int32_t imageX, int32_t imageY,
		double* outValues, uint32_t valueCapacity, uint32_t& outChannelCount) const;

	void SetToolbarVisible(bool visible);
	void SetStatusBarVisible(bool visible);
	void SetBackgroundColor(uint32_t colorRGB);
	void SetVSyncEnabled(bool enable);

	// 테스트용 디바이스 로스트 유발.
	bool SimulateDeviceLost();

public:
	virtual LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:
	enum class MouseButtonMode
	{
		NOTHING,
		LBUTTON_ROI_EDIT,
		LBUTTON_SELECTION,
		RBUTTON_PANNING,
	};

	LRESULT OnCommand(WPARAM wParam, LPARAM lParam);
	LRESULT OnCreate(WPARAM wParam, LPARAM lParam);
	LRESULT OnDestroy(WPARAM wParam, LPARAM lParam);
	LRESULT OnEraseBkgnd(WPARAM wParam, LPARAM lParam);
	LRESULT OnLButtonDblClk(WPARAM wParam, LPARAM lParam);
	LRESULT OnLButtonDown(WPARAM wParam, LPARAM lParam);
	LRESULT OnLButtonUp(WPARAM wParam, LPARAM lParam);
	LRESULT OnMouseMove(WPARAM wParam, LPARAM lParam);
	LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
	LRESULT OnMouseWheel(WPARAM wParam, LPARAM lParam);
	LRESULT OnCaptureChanged(WPARAM wParam, LPARAM lParam);
	LRESULT OnNcDestroy(WPARAM wParam, LPARAM lParam);
	LRESULT OnPaint(WPARAM wParam, LPARAM lParam);
	LRESULT OnRButtonDown(WPARAM wParam, LPARAM lParam);
	LRESULT OnRButtonUp(WPARAM wParam, LPARAM lParam);
	LRESULT OnSetCursor(WPARAM wParam, LPARAM lParam);
	LRESULT OnSize(WPARAM wParam, LPARAM lParam);
	LRESULT OnTimer(WPARAM wParam, LPARAM lParam);

	UIEventResult HandleMouseEventUI(UIMouseEventType type, int32_t mousePosX, int32_t mousePosY);

	std::unique_ptr<UIEventDispatcher> m_uiEventDispatcher = nullptr;
	static void OnUICommand(UICommand command, void* userData);
	void HandleUICommand(UICommand command);

	void Zoom(float zoomFactor);
	void ZoomIn();
	void ZoomOut();
	void Zoom1To1();
	void ZoomFit();
	void Zoom(float zoomFactor, int32_t mousePosX, int32_t mousePosY);
	void Zoom1To1(int32_t mousePosX, int32_t mousePosY);
	void BeginPan(int32_t mouseX, int32_t mouseY);
	void UpdatePan(int32_t mouseX, int32_t mouseY);
	void EndPan(int32_t mouseX, int32_t mouseY);

	void BeginSelection(const Point2i& point);
	void UpdateSelection(const Point2i& point);
	void EndSelection(const Point2i& point);

	void ToggleImageCenterCrossLine();
	void ShowImageCenterCrossLine();
	void HideImageCenterCrossLine();

	template<typename T>
	void FetchIntPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]) const;

	void FetchFloatPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]) const;

	bool GetPixelValueForStatusbar(const ImageBase* image, int32_t x, int32_t y, int32_t channel, PixelValue outValue[4]) const;

	// UpdateStatusbar: 호출자(UI) 스레드에서 좌표만 예약한다.
	// ApplyPendingStatusbarUpdate: 렌더 스레드가 Render() 안에서 실제로 반영한다.
	// 상태바가 읽는 ImageBase / Camera2D / UILabel 텍스트가 모두 렌더 스레드
	// 소유이므로 갱신 자체를 그쪽으로 넘긴다.
	void UpdateStatusbar(int32_t mouseX, int32_t mouseY);
	void ApplyPendingStatusbarUpdate();

	// 배율 라벨만 따로 본다. 좌표/픽셀값과 달리 배율은 마우스와 무관하게
	// 바뀐다 — 툴바, 그리고 호스트의 SetZoom/ZoomFit/ZoomToRect 와 그 애니메이션.
	// 마우스 이벤트에만 묶어두면 호스트가 뷰를 옮긴 뒤 라벨이 이전 값으로 남는다.
	void UpdateStatusbarZoomIfChanged();
	bool QueueImageUpdate(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth);
	bool QueueTextureUpdate(ID3D11Texture2D* texture);
	bool QueueSharedTextureUpdate(HANDLE sharedHandle);
	bool ApplyPendingImageUpdate();

	static bool CALLBACK RenderCallback(void* param);
	// resumedFromIdle: 유휴에서 깨어난 첫 프레임. 애니메이션 보간에서
	// 유휴 시간을 배제하기 위해 사용한다.
	bool Render(uint64_t frameID, bool resumedFromIdle);

	static constexpr float m_defaultZoomFactor = 0.15f;
	MouseButtonMode m_mouseButtonMode = MouseButtonMode::NOTHING;

	// TrackMouseEvent(TME_LEAVE) 무장 여부. WM_MOUSELEAVE 는 한 번 오면
	// 추적이 풀리므로 OnMouseMove 에서 다시 건다.
	bool m_isMouseTracking = false;

	// 호스트 마우스 콜백
	MouseHandler m_mouseHandler = nullptr;
	void* m_mouseUserData = nullptr;

	// ROI 레이어는 Initialize 에서 만들어진다. 그 전에 등록된 핸들러를
	// 여기 보관했다가 레이어 생성 직후 옮겨 붙인다. 보관하지 않으면
	// Initialize 전 등록이 조용히 사라진다.
	ROIRenderLayer::ROIEventHandler m_roiEventHandler = nullptr;
	void* m_roiEventUserData = nullptr;

	Point2i m_lButtonDown = { 0, 0 };
	Point2i m_lButtonDragPoint = { 0, 0 };
	Point2i m_rButtonDown = { 0, 0 };
	Point2i m_rButtonDownInitial = { 0, 0 };

	enum class LayerType
	{
		IMAGE_LAYER,
		SELECTION_RECT_LAYER
	};

	// const 조회 경로(GetPixelValueAt)도 프레임 밖임을 보장해야 하므로 mutable.
	mutable SRWLOCK m_renderLock = SRWLOCK_INIT;
	SRWLOCK m_pendingImageLock = SRWLOCK_INIT;

	std::atomic<bool> m_isDirty = { true };
	std::atomic<bool> m_hasPendingImageUpdate = { false };

	// 상태바 갱신 예약. UI 스레드가 쓰고 렌더 스레드가 읽는다.
	// 좌표는 x(하위 32bit) | y(상위 32bit) 로 묶어 한 번에 쓴다.
	std::atomic<uint64_t> m_pendingStatusbarPos = { 0 };
	std::atomic<bool> m_hasPendingStatusbarUpdate = { false };

	// 렌더 스레드 전용. 마지막으로 라벨에 넣은 배율(%)을 0.01 단위 정수로 보관한다.
	// 라벨이 "%.2f %%" 로 찍으므로 그보다 작은 변화는 다시 쓸 이유가 없다.
	int32_t m_lastStatusbarZoomCenti = INT32_MIN;

	std::unique_ptr<Camera2D> m_camera = nullptr;
	D3D11RenderEngine* m_renderEngine = nullptr;
	bool m_ownsRenderEngine = false;
	std::unique_ptr<D3D11RenderContext> m_renderContext = nullptr;
	std::unique_ptr<RenderThread> m_renderThread = nullptr;

	std::unique_ptr<TileManager> m_tileManager = nullptr;

	std::vector<IRenderLayer*> m_layers;

	std::unique_ptr<ImageRenderLayer> m_imageLayer = nullptr;
	std::unique_ptr<SelectionRectRenderLayer> m_selectionRectLayer = nullptr;
	std::unique_ptr<OverlayRenderLayer> m_overlayLayer = nullptr;
	std::unique_ptr<ROIRenderLayer> m_roiLayer = nullptr;

	bool m_showImageCenterLineLayer = false;
	std::unique_ptr<ImageCenterRenderLayer> m_imageCenterLineLayer = nullptr;

	std::unique_ptr<UIRenderLayer> m_uiLayer = nullptr;

	PendingImageUpdate m_pendingImageUpdate = {};
	bool m_timePeriodSet = false;
	bool m_isFinalized = false;
};

template<typename T>
inline void D3D11ImageView_Impl::FetchIntPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]) const
{
	const T* pixel = reinterpret_cast<const T*>(image->Ptr(y)) + x * channelCount;

	for (int c = 0; c < channelCount; ++c)
	{
		outValue[c].format = PixelValueFormat::Integer;
		outValue[c].i = static_cast<int64_t>(pixel[c]);
	}
}

inline void D3D11ImageView_Impl::FetchFloatPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]) const
{
	const float* pixel = reinterpret_cast<const float*>(image->Ptr(y)) + x * channelCount;

	for (int c = 0; c < channelCount; ++c)
	{
		outValue[c].format = PixelValueFormat::Float;
		outValue[c].f = static_cast<double>(pixel[c]);
	}
}
