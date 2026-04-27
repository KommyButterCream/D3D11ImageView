#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"

#include "../ROI Renderer/IROIObject.h"

#include "../../../Module/Core/ShapeType/Circle2f.h"
#include "../../../Module/Core/ShapeType/Ellipse2f.h"
#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Polygon2f.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"

#include <memory>
#include <vector>

class Camera2D;
class IRenderContext;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;

class ROIRenderLayer
	: public IRenderLayer
	, public IDeviceEventListener
{
public:
	ROIRenderLayer();
	virtual ~ROIRenderLayer();

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

	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);
	void ROIClear();

	bool OnLButtonDown(float screenX, float screenY);
	bool OnMouseMove(float screenX, float screenY);
	bool OnLButtonUp(float screenX, float screenY);

private:
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

	Core::ShapeType::Rect2f GetVisibleClientRect() const;
	Core::ShapeType::Rect2f ToScreenBounds(const Core::ShapeType::Rect2f& bounds) const;
	bool IsVisibleOnClient(const Core::ShapeType::Rect2f& bounds, const Core::ShapeType::Rect2f& visibleRect, float padding) const;

	Core::ShapeType::Point2f ScreenToImage(float screenX, float screenY) const;
	float GetHitToleranceInImage() const;

	IROIObject* FindObjectByKey(const wchar_t* key) const;
	IROIObject* FindObjectByKey(const wchar_t* key, ROIObjectType objectType) const;
	void RemoveObjectByKey(const wchar_t* key);
	IROIObject* HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance, ROIHitResult& hitResult) const;
	bool UpdateHoverObject(IROIObject* hoveredObject);

private:
	IRenderContext* m_context = nullptr;
	ID2D1DeviceContext* m_d2dContext = nullptr;
	const Camera2D* m_camera = nullptr;

	ID2D1SolidColorBrush* m_strokeBrush = nullptr;
	ID2D1SolidColorBrush* m_fillBrush = nullptr;
	ID2D1SolidColorBrush* m_handleFillBrush = nullptr;
	ID2D1SolidColorBrush* m_handleOutlineBrush = nullptr;

	std::vector<std::unique_ptr<IROIObject>> m_roiObjects;
	IROIObject* m_hoveredObject = nullptr;
	IROIObject* m_selectedObject = nullptr;
	IROIObject* m_activeObject = nullptr;
	ROIHitResult m_activeHit = {};
	bool m_isDragging = false;

	SRWLOCK m_roiLock = SRWLOCK_INIT;
	bool m_initialized = false;
};




