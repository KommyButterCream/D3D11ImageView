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

	bool ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	void ROIClear();

	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
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

private:
	D3D11ImageView_Impl* m_impl = nullptr;
};
