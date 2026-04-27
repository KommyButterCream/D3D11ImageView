#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"


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

#include <memory>
#include <vector>

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
struct ID2D1DeviceContext;
struct ID2D1Factory1;
struct ID2D1SolidColorBrush;

class Camera2D;
class IRenderContext;
class IOverlayObject;
class OverlayPointRenderer;
class OverlayLineRenderer;
class OverlayRectangleRenderer;
class OverlayEllipseRenderer;
class OverlayPolyShapeRenderer;

class OverlayRenderLayer
	: public IRenderLayer
	, public IDeviceEventListener
{
public:
	OverlayRenderLayer();
	virtual ~OverlayRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;

public:
	void SetCamera2D(const Camera2D* camera);

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



private:
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

	Core::ShapeType::Rect2f GetVisibleImageRect() const;
	Core::ShapeType::Rect2f GetVisibleWindowRect() const;
	bool IsVisible(const Core::ShapeType::Rect2f& bounds, const Core::ShapeType::Rect2f& visibleRect, float padding) const;

	std::vector<std::unique_ptr<IOverlayObject>>& GetImageObjects();
	std::vector<std::unique_ptr<IOverlayObject>>& GetWindowObjects();

	template <typename TObject>
	TObject* AddOverlayObject(std::vector<std::unique_ptr<IOverlayObject>>& objects, std::vector<TObject*>& objectCache, std::unique_ptr<TObject> overlayObject);

	template <typename T>
	void OverlayAddPointInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* points, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddLineInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* lines, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddRectangleInternal_v1(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddRectangleInternal_v2(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddRectangleInternal_v3(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* rectangles, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddCircleInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* circles, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddEllipseInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, const T* ellipses, size_t count, const OverlayStyle& style);

	template <typename T>
	void OverlayAddPolyShapeInternal(std::vector<std::unique_ptr<IOverlayObject>>& objects, bool isClosed, const T* polyshapes, size_t count, const OverlayStyle& style);


private:
	// Context
	IRenderContext* m_context = nullptr;

	// D2D target
	ID2D1DeviceContext* m_d2dContext = nullptr;
	ID2D1Factory1* m_d2dFactory = nullptr;

	// Camera
	const Camera2D* m_camera = nullptr;

	// Overlay Collections
	bool m_showImageOverlay = true;
	std::vector<std::unique_ptr<IOverlayObject>> m_imageOverlayObjects;
	std::vector<OverlayPointRenderer*> m_imagePointObjects;
	std::vector<OverlayLineRenderer*> m_imageLineObjects;
	std::vector<OverlayRectangleRenderer*> m_imageRectangleObjects;
	std::vector<OverlayEllipseRenderer*> m_imageEllipseObjects;
	std::vector<OverlayPolyShapeRenderer*> m_imagePolyShapeObjects;

	bool m_showWindowOverlay = true;
	std::vector<std::unique_ptr<IOverlayObject>> m_windowOverlayObjects;
	std::vector<OverlayPointRenderer*> m_windowPointObjects;
	std::vector<OverlayLineRenderer*> m_windowLineObjects;
	std::vector<OverlayRectangleRenderer*> m_windowRectangleObjects;
	std::vector<OverlayEllipseRenderer*> m_windowEllipseObjects;
	std::vector<OverlayPolyShapeRenderer*> m_windowPolyShapeObjects;

	SRWLOCK m_overlayLock = SRWLOCK_INIT;

	ID2D1SolidColorBrush* m_strokeBrush = nullptr;
	ID2D1SolidColorBrush* m_fillBrush = nullptr;

	// State
	bool m_initialized = false;
};







