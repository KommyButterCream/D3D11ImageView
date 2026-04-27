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
class ROIRenderLayer;
class UIRenderLayer;
class ImageCenterRenderLayer;

class Camera2D;
class UIEventDispatcher;

struct OverlayStyle;
class RenderThread;
class TileManager;

enum class UICommand;
enum class UIMouseEventType : uint8_t;
enum class UIEventResult;

struct PixelValue;

enum class PendingImageUpdateType : uint8_t
{
	None,
	RawImage,
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
	HANDLE sharedHandle = nullptr;

	void Reset()
	{
		type = PendingImageUpdateType::None;
		rawData = nullptr;
		width = 0;
		height = 0;
		stride = 0;
		channel = 0;
		sharedHandle = nullptr;
	}
};

class ImageView_Impl : public Core::Window::WindowBase
{
public:
	ImageView_Impl();
	~ImageView_Impl();

public:
	bool Initialize(HWND hWndParent, const RECT& rect, DWORD style);
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

	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool UpdateSharedTexture(HANDLE sharedHandle);

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
	LRESULT OnMouseWheel(WPARAM wParam, LPARAM lParam);
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
	void FetchIntPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]);

	void FetchFloatPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4]);

	bool GetPixelValueForStatusbar(const ImageBase* image, int32_t x, int32_t y, int32_t channel, PixelValue outValue[4]);
	void UpdateStatusbar(int32_t mouseX, int32_t mouseY);
	bool QueueImageUpdate(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool QueueSharedTextureUpdate(HANDLE sharedHandle);
	bool ApplyPendingImageUpdate();

	static bool CALLBACK RenderCallback(void* param);
	bool Render(uint64_t frameID);

	static constexpr float m_defaultZoomFactor = 0.15f;
	MouseButtonMode m_mouseButtonMode = MouseButtonMode::NOTHING;
	Point2i m_lButtonDown = { 0, 0 };
	Point2i m_lButtonDragPoint = { 0, 0 };
	Point2i m_rButtonDown = { 0, 0 };
	Point2i m_rButtonDownInitial = { 0, 0 };

	enum class LayerType
	{
		IMAGE_LAYER,
		SELECTION_RECT_LAYER
	};

	SRWLOCK m_renderLock = SRWLOCK_INIT;
	SRWLOCK m_pendingImageLock = SRWLOCK_INIT;

	std::atomic<bool> m_isDirty = { true };
	std::atomic<bool> m_hasPendingImageUpdate = { false };
	std::unique_ptr<Camera2D> m_camera = nullptr;
	std::unique_ptr<D3D11RenderEngine> m_renderEngine = nullptr;
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
inline void ImageView_Impl::FetchIntPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4])
{
	const T* pixel = reinterpret_cast<const T*>(image->Ptr(y)) + x * channelCount;

	for (int c = 0; c < channelCount; ++c)
	{
		outValue[c].format = PixelValueFormat::Integer;
		outValue[c].i = static_cast<int64_t>(pixel[c]);
	}
}

inline void ImageView_Impl::FetchFloatPixel(const ImageBase* image, int32_t x, int32_t y, int32_t channelCount, PixelValue outValue[4])
{
	const float* pixel = reinterpret_cast<const float*>(image->Ptr(y)) + x * channelCount;

	for (int c = 0; c < channelCount; ++c)
	{
		outValue[c].format = PixelValueFormat::Float;
		outValue[c].f = static_cast<double>(pixel[c]);
	}
}
