#include "pch.h"
#include "ImageView_Impl.h"
#include "../Render Layer/OverlayRenderLayer.h"

using namespace Core::ShapeType;

void ImageView_Impl::ImageOverlayClear()
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayClear();

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayShow(bool show)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayShow(show);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(circles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(circles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(ellipses, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(ellipses, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(polylines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::ImageOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->ImageOverlayAdd(polygons, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayClear()
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayClear();

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayShow(bool show)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayShow(show);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(points, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(lines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(rectangles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(circles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(circles, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(ellipses, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(ellipses, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(polylines, count, style);

	InvalidateFrame();
}

void ImageView_Impl::WindowOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (!m_overlayLayer)
		return;

	m_overlayLayer->WindowOverlayAdd(polygons, count, style);

	InvalidateFrame();
}

