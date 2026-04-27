#include "pch.h"
#include "OverlayRenderLayer.h"

#include <math.h>

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"

#include "../../../Module/Core/ShapeType/Rect2i.h"

#include "../Overlay Renderer/IOverlayObject.h"
#include "../Overlay Renderer/OverlayRenderContext.h"
#include "../Overlay Renderer/OverlayPointRenderer.h"
#include "../Overlay Renderer/OverlayLineRenderer.h"
#include "../Overlay Renderer/OverlayRectangleRenderer.h"
#include "../Overlay Renderer/OverlayEllipseRenderer.h"
#include "../Overlay Renderer/OverlayPolyShapeRenderer.h"

using namespace Core::ShapeType;

OverlayRenderLayer::OverlayRenderLayer()
{
}

OverlayRenderLayer::~OverlayRenderLayer()
{
	Shutdown();
}

bool OverlayRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
	{
		return false;
	}

	m_context = context;

	if (!AcquireDeviceResources())
	{
		return false;
	}

	m_context->AddDeviceListener(this);
	m_initialized = true;

	return true;
}

void OverlayRenderLayer::Shutdown()
{
	ImageOverlayClear();
	WindowOverlayClear();

	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}

	ReleaseDeviceResources();

	m_context = nullptr;
	m_initialized = false;
}

bool OverlayRenderLayer::Prepare()
{
	return true;
}

bool OverlayRenderLayer::Render()
{
	if (!m_initialized)
	{
		return true;
	}

	if (!m_d2dContext || !m_d2dFactory || !m_camera || !m_strokeBrush || !m_fillBrush)
	{
		return false;
	}

	::AcquireSRWLockShared(&m_overlayLock);

	if (!m_showImageOverlay && !m_showWindowOverlay)
	{
		::ReleaseSRWLockShared(&m_overlayLock);
		return true;
	}

	const float zoom = m_camera->GetZoom();
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	OverlayRenderContext overlayRenderContext = {};
	overlayRenderContext.d2dContext = m_d2dContext;
	overlayRenderContext.d2dFactory = m_d2dFactory;
	overlayRenderContext.strokeBrush = m_strokeBrush;
	overlayRenderContext.fillBrush = m_fillBrush;

	const Rect2f imageVisibleRect = GetVisibleImageRect();
	const Rect2f windowVisibleRect = GetVisibleWindowRect();
	const float imagePadding = max(2.0f / max(zoom, 0.0001f), 1.0f / max(zoom, 0.0001f));
	const float windowPadding = 2.0f;

	if (m_showImageOverlay)
	{
		overlayRenderContext.mode = ImageOverlayMode::ImageSpace;
		overlayRenderContext.scale = zoom;
		overlayRenderContext.imageTransform =
			D2D1::Matrix3x2F::Scale(zoom, zoom) *
			D2D1::Matrix3x2F::Translation(-offsetX * zoom, -offsetY * zoom);

		m_d2dContext->SetTransform(overlayRenderContext.imageTransform);

		for (const OverlayPointRenderer* overlayObject : m_imagePointObjects) if (IsVisible(overlayObject->GetBounds(), imageVisibleRect, imagePadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayLineRenderer* overlayObject : m_imageLineObjects) if (IsVisible(overlayObject->GetBounds(), imageVisibleRect, imagePadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayRectangleRenderer* overlayObject : m_imageRectangleObjects) if (IsVisible(overlayObject->GetBounds(), imageVisibleRect, imagePadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayEllipseRenderer* overlayObject : m_imageEllipseObjects) if (IsVisible(overlayObject->GetBounds(), imageVisibleRect, imagePadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayPolyShapeRenderer* overlayObject : m_imagePolyShapeObjects) if (IsVisible(overlayObject->GetBounds(), imageVisibleRect, imagePadding)) overlayObject->Render(overlayRenderContext);

		m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	if (m_showWindowOverlay)
	{
		overlayRenderContext.mode = ImageOverlayMode::WindowSpace;
		overlayRenderContext.scale = 1.0f;
		overlayRenderContext.imageTransform = D2D1::Matrix3x2F::Identity();

		m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

		for (const OverlayPointRenderer* overlayObject : m_windowPointObjects) if (IsVisible(overlayObject->GetBounds(), windowVisibleRect, windowPadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayLineRenderer* overlayObject : m_windowLineObjects) if (IsVisible(overlayObject->GetBounds(), windowVisibleRect, windowPadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayRectangleRenderer* overlayObject : m_windowRectangleObjects) if (IsVisible(overlayObject->GetBounds(), windowVisibleRect, windowPadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayEllipseRenderer* overlayObject : m_windowEllipseObjects) if (IsVisible(overlayObject->GetBounds(), windowVisibleRect, windowPadding)) overlayObject->Render(overlayRenderContext);
		for (const OverlayPolyShapeRenderer* overlayObject : m_windowPolyShapeObjects) if (IsVisible(overlayObject->GetBounds(), windowVisibleRect, windowPadding)) overlayObject->Render(overlayRenderContext);
	}

	::ReleaseSRWLockShared(&m_overlayLock);

	return true;
}

void OverlayRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void OverlayRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

void OverlayRenderLayer::SetCamera2D(const Camera2D* camera)
{
	m_camera = camera;
}

void OverlayRenderLayer::ImageOverlayClear()
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	m_imageOverlayObjects.clear();
	m_imagePointObjects.clear();
	m_imageLineObjects.clear();
	m_imageRectangleObjects.clear();
	m_imageEllipseObjects.clear();
	m_imagePolyShapeObjects.clear();
	::ReleaseSRWLockExclusive(&m_overlayLock);
}

void OverlayRenderLayer::ImageOverlayShow(bool show)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	m_showImageOverlay = show;
	::ReleaseSRWLockExclusive(&m_overlayLock);
}

void OverlayRenderLayer::ImageOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetImageObjects(), points, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetImageObjects(), points, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetImageObjects(), points, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetImageObjects(), lines, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetImageObjects(), lines, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetImageObjects(), lines, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetImageObjects(), rectangles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (!circles || count == 0)
	{
		return;
	}

	OverlayAddCircleInternal(GetImageObjects(), circles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (!circles || count == 0)
	{
		return;
	}

	OverlayAddCircleInternal(GetImageObjects(), circles, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (!ellipses || count == 0)
	{
		return;
	}

	OverlayAddEllipseInternal(GetImageObjects(), ellipses, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (!ellipses || count == 0)
	{
		return;
	}

	OverlayAddEllipseInternal(GetImageObjects(), ellipses, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (!polylines || count == 0)
	{
		return;
	}

	OverlayAddPolyShapeInternal(GetImageObjects(), false, polylines, count, style);
}

void OverlayRenderLayer::ImageOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (!polygons || count == 0)
	{
		return;
	}

	OverlayAddPolyShapeInternal(GetImageObjects(), true, polygons, count, style);
}

void OverlayRenderLayer::WindowOverlayClear()
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	m_windowOverlayObjects.clear();
	m_windowPointObjects.clear();
	m_windowLineObjects.clear();
	m_windowRectangleObjects.clear();
	m_windowEllipseObjects.clear();
	m_windowPolyShapeObjects.clear();
	::ReleaseSRWLockExclusive(&m_overlayLock);
}

void OverlayRenderLayer::WindowOverlayShow(bool show)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	m_showWindowOverlay = show;
	::ReleaseSRWLockExclusive(&m_overlayLock);
}

void OverlayRenderLayer::WindowOverlayAdd(const Point2i* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetWindowObjects(), points, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Point2f* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetWindowObjects(), points, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Point2d* points, size_t count, const OverlayStyle& style)
{
	if (!points || count == 0)
	{
		return;
	}

	OverlayAddPointInternal(GetWindowObjects(), points, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Line2i* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetWindowObjects(), lines, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Line2f* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetWindowObjects(), lines, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Line2d* lines, size_t count, const OverlayStyle& style)
{
	if (!lines || count == 0)
	{
		return;
	}

	OverlayAddLineInternal(GetWindowObjects(), lines, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Rect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Rect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Rect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v1(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const QuadRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const QuadRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const QuadRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v2(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const RotatedRect2i* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const RotatedRect2f* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const RotatedRect2d* rectangles, size_t count, const OverlayStyle& style)
{
	if (!rectangles || count == 0)
	{
		return;
	}

	OverlayAddRectangleInternal_v3(GetWindowObjects(), rectangles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Circle2f* circles, size_t count, const OverlayStyle& style)
{
	if (!circles || count == 0)
	{
		return;
	}

	OverlayAddCircleInternal(GetWindowObjects(), circles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Circle2d* circles, size_t count, const OverlayStyle& style)
{
	if (!circles || count == 0)
	{
		return;
	}

	OverlayAddCircleInternal(GetWindowObjects(), circles, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Ellipse2f* ellipses, size_t count, const OverlayStyle& style)
{
	if (!ellipses || count == 0)
	{
		return;
	}

	OverlayAddEllipseInternal(GetWindowObjects(), ellipses, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Ellipse2d* ellipses, size_t count, const OverlayStyle& style)
{
	if (!ellipses || count == 0)
	{
		return;
	}

	OverlayAddEllipseInternal(GetWindowObjects(), ellipses, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Polyline2f* polylines, size_t count, const OverlayStyle& style)
{
	if (!polylines || count == 0)
	{
		return;
	}

	OverlayAddPolyShapeInternal(GetWindowObjects(), false, polylines, count, style);
}

void OverlayRenderLayer::WindowOverlayAdd(const Polygon2f* polygons, size_t count, const OverlayStyle& style)
{
	if (!polygons || count == 0)
	{
		return;
	}

	OverlayAddPolyShapeInternal(GetWindowObjects(), true, polygons, count, style);
}

bool OverlayRenderLayer::AcquireDeviceResources()
{
	m_d2dContext = m_context->GetD2DDeviceContext();
	if (!m_d2dContext)
	{
		return false;
	}

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
	{
		return false;
	}

	m_d2dFactory = engine->GetD2DFactory();
	if (!m_d2dFactory)
	{
		return false;
	}

	ReleaseDeviceResources();

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &m_strokeBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &m_fillBrush)))
	{
		return false;
	}

	return true;
}

void OverlayRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_strokeBrush);
	SafeRelease(m_fillBrush);
}

Rect2f OverlayRenderLayer::GetVisibleImageRect() const
{
	if (!m_camera)
	{
		return {};
	}

	const Rect2i viewRect = m_camera->GetViewImageRect();
	return {
		static_cast<float>(viewRect.left),
		static_cast<float>(viewRect.top),
		static_cast<float>(viewRect.right),
		static_cast<float>(viewRect.bottom)
	};
}

Rect2f OverlayRenderLayer::GetVisibleWindowRect() const
{
	if (!m_context)
	{
		return {};
	}

	return {
		0.0f,
		0.0f,
		static_cast<float>(m_context->GetWidth()),
		static_cast<float>(m_context->GetHeight())
	};
}

bool OverlayRenderLayer::IsVisible(const Rect2f& bounds, const Rect2f& visibleRect, float padding) const
{
	Rect2f expandedBounds = bounds;
	expandedBounds.Inflate(padding, padding);
	return expandedBounds.Intersects(visibleRect);
}

std::vector<std::unique_ptr<IOverlayObject>>& OverlayRenderLayer::GetImageObjects()
{
	return m_imageOverlayObjects;
}

std::vector<std::unique_ptr<IOverlayObject>>& OverlayRenderLayer::GetWindowObjects()
{
	return m_windowOverlayObjects;
}

template <typename TObject>
TObject* OverlayRenderLayer::AddOverlayObject(std::vector<std::unique_ptr<IOverlayObject>>& objects, std::vector<TObject*>& objectCache, std::unique_ptr<TObject> overlayObject)
{
	TObject* objectPointer = overlayObject.get();
	objects.push_back(std::move(overlayObject));
	objectCache.push_back(objectPointer);
	return objectPointer;
}

template <typename T>
void OverlayRenderLayer::OverlayAddPointInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* points, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayPointRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imagePointObjects : m_windowPointObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayPoint point = {};
		point.x = static_cast<float>(points[index].x);
		point.y = static_cast<float>(points[index].y);
		point.style = style;
		point.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayPointRenderer>(point));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddLineInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* lines, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayLineRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageLineObjects : m_windowLineObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayLine line = {};
		line.x1 = static_cast<float>(lines[index].sx);
		line.y1 = static_cast<float>(lines[index].sy);
		line.x2 = static_cast<float>(lines[index].ex);
		line.y2 = static_cast<float>(lines[index].ey);
		line.style = style;
		line.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayLineRenderer>(line));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddRectangleInternal_v1(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayRectangleRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageRectangleObjects : m_windowRectangleObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayRect rectangle = {};
		const T& source = rectangles[index];

		const float left = static_cast<float>(source.left);
		const float top = static_cast<float>(source.top);
		const float right = static_cast<float>(source.right);
		const float bottom = static_cast<float>(source.bottom);

		rectangle.cx = (left + right) * 0.5f;
		rectangle.cy = (top + bottom) * 0.5f;
		rectangle.hx = (right - left) * 0.5f;
		rectangle.hy = (bottom - top) * 0.5f;
		rectangle.angleRad = 0.0f;

		const float cosA = static_cast<float>(cos(rectangle.angleRad));
		const float sinA = static_cast<float>(sin(rectangle.angleRad));

		auto rotate = [&](float x, float y)
			{
				return Point2f{
					rectangle.cx + x * cosA - y * sinA,
					rectangle.cy + x * sinA + y * cosA
				};
			};

		rectangle.p1 = rotate(-rectangle.hx, -rectangle.hy);
		rectangle.p2 = rotate(+rectangle.hx, -rectangle.hy);
		rectangle.p3 = rotate(+rectangle.hx, +rectangle.hy);
		rectangle.p4 = rotate(-rectangle.hx, +rectangle.hy);
		rectangle.style = style;
		rectangle.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayRectangleRenderer>(rectangle));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddRectangleInternal_v2(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayRectangleRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageRectangleObjects : m_windowRectangleObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayRect rectangle = {};
		const T& source = rectangles[index];

		rectangle.p1 = { static_cast<float>(source.TopLeft.x), static_cast<float>(source.TopLeft.y) };
		rectangle.p2 = { static_cast<float>(source.TopRight.x), static_cast<float>(source.TopRight.y) };
		rectangle.p3 = { static_cast<float>(source.BottomRight.x), static_cast<float>(source.BottomRight.y) };
		rectangle.p4 = { static_cast<float>(source.BottomLeft.x), static_cast<float>(source.BottomLeft.y) };

		rectangle.cx = (rectangle.p1.x + rectangle.p2.x + rectangle.p3.x + rectangle.p4.x) * 0.25f;
		rectangle.cy = (rectangle.p1.y + rectangle.p2.y + rectangle.p3.y + rectangle.p4.y) * 0.25f;

		float minX = rectangle.p1.x;
		float maxX = rectangle.p1.x;
		float minY = rectangle.p1.y;
		float maxY = rectangle.p1.y;

		const Point2f points[3] = { rectangle.p2, rectangle.p3, rectangle.p4 };
		for (const Point2f& point : points)
		{
			minX = min(minX, point.x);
			maxX = max(maxX, point.x);
			minY = min(minY, point.y);
			maxY = max(maxY, point.y);
		}

		rectangle.hx = (maxX - minX) * 0.5f;
		rectangle.hy = (maxY - minY) * 0.5f;
		rectangle.angleRad = 0.0f;
		rectangle.style = style;
		rectangle.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayRectangleRenderer>(rectangle));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddRectangleInternal_v3(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayRectangleRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageRectangleObjects : m_windowRectangleObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayRect rectangle = {};
		const T& source = rectangles[index];

		rectangle.cx = static_cast<float>(source.center.x);
		rectangle.cy = static_cast<float>(source.center.y);
		rectangle.hx = static_cast<float>(source.size.width * 0.5f);
		rectangle.hy = static_cast<float>(source.size.height * 0.5f);
		rectangle.angleRad = static_cast<float>(source.angleRad);

		const float cosValue = cosf(rectangle.angleRad);
		const float sinValue = sinf(rectangle.angleRad);
		const float halfWidth = rectangle.hx;
		const float halfHeight = rectangle.hy;

		auto rotate = [&](float x, float y) noexcept -> Point2f
			{
				return {
					rectangle.cx + x * cosValue - y * sinValue,
					rectangle.cy + x * sinValue + y * cosValue
				};
			};

		rectangle.p1 = rotate(-halfWidth, -halfHeight);
		rectangle.p2 = rotate(halfWidth, -halfHeight);
		rectangle.p3 = rotate(halfWidth, halfHeight);
		rectangle.p4 = rotate(-halfWidth, halfHeight);
		rectangle.style = style;
		rectangle.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayRectangleRenderer>(rectangle));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddCircleInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* circles, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayEllipseRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageEllipseObjects : m_windowEllipseObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayEllipse ellipse = {};
		ellipse.cx = static_cast<float>(circles[index].x);
		ellipse.cy = static_cast<float>(circles[index].y);
		ellipse.rx = static_cast<float>(circles[index].radius);
		ellipse.ry = static_cast<float>(circles[index].radius);
		ellipse.angleRad = 0.0f;
		ellipse.style = style;
		ellipse.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayEllipseRenderer>(ellipse));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddEllipseInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* ellipses, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayEllipseRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imageEllipseObjects : m_windowEllipseObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayEllipse ellipse = {};
		ellipse.cx = static_cast<float>(ellipses[index].x);
		ellipse.cy = static_cast<float>(ellipses[index].y);
		ellipse.rx = static_cast<float>(ellipses[index].radiusX);
		ellipse.ry = static_cast<float>(ellipses[index].radiusY);
		ellipse.angleRad = static_cast<float>(ellipses[index].angleRad);
		ellipse.style = style;
		ellipse.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayEllipseRenderer>(ellipse));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}

template <typename T>
void OverlayRenderLayer::OverlayAddPolyShapeInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, bool isClosed, const T* polyshapes, size_t count, const OverlayStyle& style)
{
	::AcquireSRWLockExclusive(&m_overlayLock);
	std::vector<OverlayPolyShapeRenderer*>& objectCache = (&objects == &m_imageOverlayObjects) ? m_imagePolyShapeObjects : m_windowPolyShapeObjects;

	for (size_t index = 0; index < count; ++index)
	{
		OverlayPolyShape polyShape = {};
		const T& source = polyshapes[index];

		polyShape.isClosed = isClosed;
		polyShape.pointCount = source.Size();
		polyShape.points = new D2D1_POINT_2F[polyShape.pointCount];

		const auto& vertices = source.GetVertices();
		for (size_t pointIndex = 0; pointIndex < polyShape.pointCount; ++pointIndex)
		{
			polyShape.points[pointIndex] = {
				static_cast<float>(vertices[pointIndex].x),
				static_cast<float>(vertices[pointIndex].y)
			};
		}

		polyShape.style = style;
		polyShape.style.UpdateD2DColors();

		AddOverlayObject(objects, objectCache, std::make_unique<OverlayPolyShapeRenderer>(std::move(polyShape)));
	}

	::ReleaseSRWLockExclusive(&m_overlayLock);
}




