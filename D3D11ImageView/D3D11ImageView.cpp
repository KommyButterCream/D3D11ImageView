#include "pch.h"
#include "D3D11ImageView.h"
#include "D3D11ImageView_Impl.h"

using namespace Core::ShapeType;

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

#include "../Overlay Renderer/OverlayTypes.h"

D3D11ImageView::D3D11ImageView()
	: m_impl(new D3D11ImageView_Impl)
{
}

D3D11ImageView::~D3D11ImageView()
{
	if (m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}

bool D3D11ImageView::Initialize(HWND hWndParent, const RECT& rect, DWORD style, D3D11RenderEngine* D3D11Engine)
{
	return Initialize(D3D11Engine, hWndParent, rect, style);
}

bool D3D11ImageView::Initialize(D3D11RenderEngine* D3D11Engine, HWND hWndParent, const RECT& rect, DWORD style)
{
	if (!m_impl->GetHWND())
	{
		if (!m_impl->Initialize(D3D11Engine, hWndParent, rect, style))
			return false;
	}

	return true;
}

HWND D3D11ImageView::GetHWND() const
{
	if (m_impl && m_impl->GetHWND())
	{
		return m_impl->GetHWND();
	}

	return nullptr;
}

ID3D11Device* D3D11ImageView::GetDevice() const
{
	if (m_impl)
	{
		return m_impl->GetDevice();
	}

	return nullptr;
}

ID3D11DeviceContext* D3D11ImageView::GetDeviceContext() const
{
	if (m_impl)
	{
		return m_impl->GetDeviceContext();
	}

	return nullptr;
}

void D3D11ImageView::RenderLock()
{
	if (m_impl)
	{
		m_impl->RenderLock();
	}
}

void D3D11ImageView::RenderUnLock()
{
	if (m_impl)
	{
		m_impl->RenderUnLock();
	}
}

void D3D11ImageView::InvalidateFrame()
{
	if (m_impl)
	{
		m_impl->InvalidateFrame();
	}
}

void D3D11ImageView::ImageOverlayClear()
{
	if (m_impl)
	{
		m_impl->ImageOverlayClear();
	}
}

void D3D11ImageView::ImageOverlayShow(bool show)
{
	if (m_impl)
	{
		m_impl->ImageOverlayShow(show);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(circles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(circles, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(ellipses, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(ellipses, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(polylines, count, style);
	}
}

void D3D11ImageView::ImageOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(polygons, count, style);
	}
}

void D3D11ImageView::WindowOverlayClear()
{
	if (m_impl)
	{
		m_impl->WindowOverlayClear();
	}
}

void D3D11ImageView::WindowOverlayShow(bool show)
{
	if (m_impl)
	{
		m_impl->WindowOverlayShow(show);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(circles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(circles, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(ellipses, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(ellipses, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(polylines, count, style);
	}
}

void D3D11ImageView::WindowOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(polygons, count, style);
	}
}

bool D3D11ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, rect, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool D3D11ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, ellipse, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool D3D11ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, circle, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool D3D11ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, polygon, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

void D3D11ImageView::ROIClear()
{
	if (m_impl)
	{
		m_impl->ROIClear();
	}
}

bool D3D11ImageView::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	return UpdateImage(data, width, height, stride, channel, 8);
}

bool D3D11ImageView::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth)
{
	if (m_impl)
	{
		return m_impl->UpdateImage(data, width, height, stride, channel, bitDepth);
	}
	else
	{
		return false;
	}
}

bool D3D11ImageView::UpdateTexture(ID3D11Texture2D* texture)
{
	if (m_impl)
	{
		return m_impl->UpdateTexture(texture);
	}
	else
	{
		return false;
	}
}

bool D3D11ImageView::UpdateSharedTexture(HANDLE sharedHandle)
{
	if (m_impl)
	{
		return m_impl->UpdateSharedTexture(sharedHandle);
	}
	else
	{
		return false;
	}
}
void D3D11ImageView::DetachImage()
{
	if (m_impl)
	{
		m_impl->DetachImage();
	}
}

bool D3D11ImageView::SimulateDeviceLost()
{
	if (m_impl)
	{
		return m_impl->SimulateDeviceLost();
	}

	return false;
}

uint32_t D3D11ImageView::ROIGetCount() const
{
	return m_impl ? m_impl->ROIGetCount() : 0u;
}

bool D3D11ImageView::ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const
{
	return m_impl ? m_impl->ROIGetShape(key, outShape) : false;
}

uint32_t D3D11ImageView::ROIGetVertices(const wchar_t* key, Point2f* buffer,
	uint32_t capacity, uint32_t segmentsPerCurve) const
{
	return m_impl ? m_impl->ROIGetVertices(key, buffer, capacity, segmentsPerCurve) : 0u;
}

bool D3D11ImageView::ROIGetBounds(const wchar_t* key, Rect2f& outBounds) const
{
	return m_impl ? m_impl->ROIGetBounds(key, outBounds) : false;
}

bool D3D11ImageView::ROIGetInfo(const wchar_t* key, ROIInfo& outInfo) const
{
	if (!m_impl)
	{
		return false;
	}

	ROIRenderLayer::ROIInfoData internalInfo;
	if (!m_impl->ROIGetInfo(key, internalInfo))
	{
		return false;
	}

	outInfo.type = internalInfo.type;
	outInfo.colorRGB = internalInfo.colorRGB;
	outInfo.isMovable = internalInfo.isMovable;
	outInfo.isResizable = internalInfo.isResizable;
	outInfo.isSelected = internalInfo.isSelected;
	outInfo.isHovered = internalInfo.isHovered;
	outInfo.fontSize = internalInfo.fontSize;

	return true;
}

uint32_t D3D11ImageView::ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars) const
{
	return m_impl ? m_impl->ROIGetName(key, buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView::ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const
{
	return m_impl ? m_impl->ROIGetKeyAt(index, buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView::ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const
{
	return m_impl ? m_impl->ROIGetSelectedKey(buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView::ROIHitTestKey(float imageX, float imageY, float tolerance,
	wchar_t* buffer, uint32_t bufferChars) const
{
	return m_impl ? m_impl->ROIHitTestKey(imageX, imageY, tolerance, buffer, bufferChars) : 0u;
}

bool D3D11ImageView::ROIRemove(const wchar_t* key)
{
	return m_impl ? m_impl->ROIRemove(key) : false;
}

/*=====================================================
	콜백 트램폴린

	Impl 시그니처를 공개 시그니처로 옮긴다. 함수 포인터를
	reinterpret_cast 하지 않고 실제 변환을 거치는 이유는,
	두 열거형/구조체가 지금은 같은 배치여도 앞으로 갈라질 수
	있어서다.
=====================================================*/
struct D3D11ImageViewCallbackBridge
{
	static void ROIEventTrampoline(ROIRenderLayer::ROIEvent event, const wchar_t* key, void* userData)
	{
		D3D11ImageView* view = static_cast<D3D11ImageView*>(userData);
		if (!view)
		{
			return;
		}

		view->OnROIEventInternal(static_cast<uint32_t>(event), key);
	}

	static bool MouseTrampoline(const D3D11ImageView_Impl::MouseEventData& data, void* userData)
	{
		D3D11ImageView* view = static_cast<D3D11ImageView*>(userData);
		if (!view)
		{
			return false;
		}

		return view->OnMouseEventInternal(&data);
	}
};

void D3D11ImageView::OnROIEventInternal(uint32_t event, const wchar_t* key)
{
	if (m_roiHandler)
	{
		m_roiHandler(static_cast<ROIEvent>(event), key, m_roiUserData);
	}
}

bool D3D11ImageView::OnMouseEventInternal(const void* implEventData)
{
	if (!m_mouseHandler || !implEventData)
	{
		return false;
	}

	const D3D11ImageView_Impl::MouseEventData& src =
		*static_cast<const D3D11ImageView_Impl::MouseEventData*>(implEventData);

	MouseEvent e;
	e.type = static_cast<MouseEventType>(src.type);
	e.screenX = src.screenX;
	e.screenY = src.screenY;
	e.imageX = src.imageX;
	e.imageY = src.imageY;
	e.isInsideImage = src.isInsideImage;
	e.wheelDelta = src.wheelDelta;
	e.modifiers = src.modifiers;
	e.buttons = src.buttons;

	return m_mouseHandler(e, m_mouseUserData);
}

void D3D11ImageView::SetROIEventHandler(ROIEventHandler handler, void* userData)
{
	m_roiHandler = handler;
	m_roiUserData = userData;

	if (m_impl)
	{
		// 핸들러를 지웠으면 Impl 쪽도 떼어 불필요한 호출을 막는다.
		m_impl->SetROIEventHandler(handler ? &D3D11ImageViewCallbackBridge::ROIEventTrampoline : nullptr, this);
	}
}

void D3D11ImageView::SetMouseHandler(MouseHandler handler, void* userData)
{
	m_mouseHandler = handler;
	m_mouseUserData = userData;

	if (m_impl)
	{
		m_impl->SetMouseHandler(handler ? &D3D11ImageViewCallbackBridge::MouseTrampoline : nullptr, this);
	}
}

/*=====================================================
	뷰 제어
=====================================================*/
void D3D11ImageView::SetZoom(float zoom, bool animate)
{
	if (m_impl)
	{
		m_impl->SetZoomLevel(zoom, animate);
	}
}

float D3D11ImageView::GetZoom() const
{
	return m_impl ? m_impl->GetZoomLevel() : 0.0f;
}

void D3D11ImageView::ZoomFit(bool animate)
{
	if (m_impl)
	{
		m_impl->ZoomFitProgrammatic(animate);
	}
}

void D3D11ImageView::Zoom1To1(bool animate)
{
	if (m_impl)
	{
		m_impl->Zoom1To1Programmatic(animate);
	}
}

void D3D11ImageView::SetCenter(float imageX, float imageY, bool animate)
{
	if (m_impl)
	{
		m_impl->SetViewCenter(imageX, imageY, animate);
	}
}

void D3D11ImageView::GetCenter(float& outImageX, float& outImageY) const
{
	outImageX = 0.0f;
	outImageY = 0.0f;

	if (m_impl)
	{
		m_impl->GetViewCenter(outImageX, outImageY);
	}
}

void D3D11ImageView::ZoomToRect(const Rect2f& imageRect, float marginRatio, bool animate)
{
	if (m_impl)
	{
		m_impl->ZoomToRect(imageRect, marginRatio, animate);
	}
}

bool D3D11ImageView::GetVisibleImageRect(Rect2f& outRect) const
{
	return m_impl ? m_impl->GetVisibleImageRect(outRect) : false;
}

/*=====================================================
	좌표 변환
=====================================================*/
bool D3D11ImageView::ScreenToImage(int32_t screenX, int32_t screenY, float& outImageX, float& outImageY) const
{
	outImageX = 0.0f;
	outImageY = 0.0f;

	return m_impl ? m_impl->ScreenToImage(screenX, screenY, outImageX, outImageY) : false;
}

bool D3D11ImageView::ImageToScreen(float imageX, float imageY, int32_t& outScreenX, int32_t& outScreenY) const
{
	outScreenX = 0;
	outScreenY = 0;

	return m_impl ? m_impl->ImageToScreen(imageX, imageY, outScreenX, outScreenY) : false;
}

/*=====================================================
	이미지 정보
=====================================================*/
bool D3D11ImageView::GetImageSize(uint32_t& outWidth, uint32_t& outHeight) const
{
	outWidth = 0;
	outHeight = 0;

	return m_impl ? m_impl->GetImageSize(outWidth, outHeight) : false;
}

bool D3D11ImageView::GetImageChannelInfo(uint32_t& outChannel, uint32_t& outBitDepth) const
{
	outChannel = 0;
	outBitDepth = 0;

	return m_impl ? m_impl->GetImageChannelInfo(outChannel, outBitDepth) : false;
}

bool D3D11ImageView::GetPixelValueAt(int32_t imageX, int32_t imageY,
	double* outValues, uint32_t valueCapacity, uint32_t& outChannelCount) const
{
	outChannelCount = 0;

	return m_impl
		? m_impl->GetPixelValueAt(imageX, imageY, outValues, valueCapacity, outChannelCount)
		: false;
}

/*=====================================================
	표시 옵션
=====================================================*/
void D3D11ImageView::SetToolbarVisible(bool visible)
{
	if (m_impl)
	{
		m_impl->SetToolbarVisible(visible);
	}
}

void D3D11ImageView::SetStatusBarVisible(bool visible)
{
	if (m_impl)
	{
		m_impl->SetStatusBarVisible(visible);
	}
}

void D3D11ImageView::SetBackgroundColor(uint32_t colorRGB)
{
	if (m_impl)
	{
		m_impl->SetBackgroundColor(colorRGB);
	}
}

void D3D11ImageView::SetVSyncEnabled(bool enable)
{
	if (m_impl)
	{
		m_impl->SetVSyncEnabled(enable);
	}
}

bool D3D11ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Line2f& line, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, line, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

void D3D11ImageView::SetPixelScale(double xScale, double yScale, const wchar_t* unit)
{
	if (m_impl)
	{
		m_impl->SetPixelScale(xScale, yScale, unit);
	}
}

void D3D11ImageView::ToggleMeasureDistance()
{
	if (m_impl)
	{
		m_impl->ToggleMeasureDistance();
	}
}

bool D3D11ImageView::SaveImage(const wchar_t* filePath)
{
	return m_impl ? m_impl->SaveImage(filePath) : false;
}

bool D3D11ImageView::IsLutSupported() const
{
	return m_impl ? m_impl->IsLutSupported() : false;
}

void D3D11ImageView::SetLutEnabled(bool enable)
{
	if (m_impl) m_impl->SetLutEnabled(enable);
}

bool D3D11ImageView::IsLutEnabled() const
{
	return m_impl ? m_impl->IsLutEnabled() : false;
}

void D3D11ImageView::ToggleLut()
{
	if (m_impl) m_impl->ToggleLut();
}

void D3D11ImageView::SetLutPreset(LutPreset preset)
{
	if (m_impl) m_impl->SetLutPreset(preset);
}

LutPreset D3D11ImageView::GetLutPreset() const
{
	return m_impl ? m_impl->GetLutPreset() : LutPreset::Grayscale;
}

bool D3D11ImageView::IsMeasureActive() const
{
	return m_impl ? m_impl->IsMeasureActive() : false;
}

void D3D11ImageView::ToggleMeasureAngle()
{
	if (m_impl)
	{
		m_impl->ToggleMeasureAngle();
	}
}

bool D3D11ImageView::IsMeasureAngleActive() const
{
	return m_impl ? m_impl->IsMeasureAngleActive() : false;
}
