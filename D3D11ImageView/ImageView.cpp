#include "pch.h"
#include "ImageView.h"
#include "ImageView_Impl.h"

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

ImageView::ImageView()
	: m_impl(new ImageView_Impl)
{
}

ImageView::~ImageView()
{
	if (m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}

bool ImageView::Initialize(HWND hWndParent, const RECT& rect, DWORD style)
{
	if (!m_impl->GetHWND())
	{
		if (!m_impl->Initialize(hWndParent, rect, style))
			return false;
	}

	return true;
}

HWND ImageView::GetHWND() const
{
	if (m_impl && m_impl->GetHWND())
	{
		return m_impl->GetHWND();
	}

	return nullptr;
}

ID3D11Device* ImageView::GetDevice() const
{
	if (m_impl)
	{
		return m_impl->GetDevice();
	}

	return nullptr;
}

ID3D11DeviceContext* ImageView::GetDeviceContext() const
{
	if (m_impl)
	{
		return m_impl->GetDeviceContext();
	}

	return nullptr;
}

void ImageView::RenderLock()
{
	if (m_impl)
	{
		m_impl->RenderLock();
	}
}

void ImageView::RenderUnLock()
{
	if (m_impl)
	{
		m_impl->RenderUnLock();
	}
}

void ImageView::InvalidateFrame()
{
	if (m_impl)
	{
		m_impl->InvalidateFrame();
	}
}

void ImageView::ImageOverlayClear()
{
	if (m_impl)
	{
		m_impl->ImageOverlayClear();
	}
}

void ImageView::ImageOverlayShow(bool show)
{
	if (m_impl)
	{
		m_impl->ImageOverlayShow(show);
	}
}

void ImageView::ImageOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(points, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(lines, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(rectangles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(circles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(circles, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(ellipses, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(ellipses, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(polylines, count, style);
	}
}

void ImageView::ImageOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->ImageOverlayAdd(polygons, count, style);
	}
}

void ImageView::WindowOverlayClear()
{
	if (m_impl)
	{
		m_impl->WindowOverlayClear();
	}
}

void ImageView::WindowOverlayShow(bool show)
{
	if (m_impl)
	{
		m_impl->WindowOverlayShow(show);
	}
}

void ImageView::WindowOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(points, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(lines, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(rectangles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(circles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(circles, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(ellipses, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(ellipses, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(polylines, count, style);
	}
}

void ImageView::WindowOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (m_impl)
	{
		m_impl->WindowOverlayAdd(polygons, count, style);
	}
}

bool ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, rect, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, ellipse, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, circle, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

bool ImageView::ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (m_impl)
	{
		return m_impl->ROISet(key, name, polygon, rgb, isMovable, isResizable, fontSize);
	}

	return false;
}

void ImageView::ROIClear()
{
	if (m_impl)
	{
		m_impl->ROIClear();
	}
}

bool ImageView::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	if (m_impl)
	{
		return m_impl->UpdateImage(data, width, height, stride, channel);
	}
	else
	{
		return false;
	}
}

bool ImageView::UpdateSharedTexture(HANDLE sharedHandle)
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






