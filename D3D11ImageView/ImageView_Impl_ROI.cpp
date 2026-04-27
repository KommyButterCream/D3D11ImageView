#include "pch.h"
#include "ImageView_Impl.h"

#include "../Render Layer/ROIRenderLayer.h"

using namespace Core::ShapeType;

bool ImageView_Impl::ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!m_roiLayer)
	{
		return false;
	}

	const bool result = m_roiLayer->ROISet(key, name, rect, rgb, isMovable, isResizable, fontSize);
	if (result)
	{
		InvalidateFrame();
	}

	return result;
}

bool ImageView_Impl::ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!m_roiLayer)
	{
		return false;
	}

	const bool result = m_roiLayer->ROISet(key, name, ellipse, rgb, isMovable, isResizable, fontSize);
	if (result)
	{
		InvalidateFrame();
	}

	return result;
}

bool ImageView_Impl::ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!m_roiLayer)
	{
		return false;
	}

	const bool result = m_roiLayer->ROISet(key, name, circle, rgb, isMovable, isResizable, fontSize);
	if (result)
	{
		InvalidateFrame();
	}

	return result;
}

bool ImageView_Impl::ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!m_roiLayer)
	{
		return false;
	}

	const bool result = m_roiLayer->ROISet(key, name, polygon, rgb, isMovable, isResizable, fontSize);
	if (result)
	{
		InvalidateFrame();
	}

	return result;
}

void ImageView_Impl::ROIClear()
{
	if (!m_roiLayer)
	{
		return;
	}

	m_roiLayer->ROIClear();
	InvalidateFrame();
}

