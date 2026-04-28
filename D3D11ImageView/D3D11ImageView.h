#pragma once

#ifdef BUILD_D3D11_IMAGE_VIEW_DLL
#define D3D11_IMAGE_VIEW_API __declspec(dllexport)
#else
#define D3D11_IMAGE_VIEW_API __declspec(dllimport)
#endif

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
	bool Initialize(HWND hWndParent, const RECT& rect, DWORD style);

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

	bool ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	void ROIClear();

	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool UpdateSharedTexture(HANDLE sharedHandle);

private:
	D3D11ImageView_Impl* m_impl = nullptr;
};
