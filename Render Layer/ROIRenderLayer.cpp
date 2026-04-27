#include "pch.h"
#include "ROIRenderLayer.h"

#include "../ROI Renderer/IROIObject.h"
#include "../ROI Renderer/ROICircleRenderer.h"
#include "../ROI Renderer/ROIEllipseRenderer.h"
#include "../ROI Renderer/ROIRenderContext.h"
#include "../ROI Renderer/ROIPolygonRenderer.h"
#include "../ROI Renderer/ROIRectangleRenderer.h"

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11EngineInterface/IRenderContext.h"

using namespace Core::ShapeType;

ROIRenderLayer::ROIRenderLayer()
{
}

ROIRenderLayer::~ROIRenderLayer()
{
	Shutdown();
}

bool ROIRenderLayer::Initialize(IRenderContext* context)
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

void ROIRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}

	ROIClear();
	ReleaseDeviceResources();

	m_context = nullptr;
	m_camera = nullptr;
	m_hoveredObject = nullptr;
	m_selectedObject = nullptr;
	m_activeObject = nullptr;
	m_activeHit = {};
	m_isDragging = false;
	m_initialized = false;
}

bool ROIRenderLayer::Prepare()
{
	return true;
}

bool ROIRenderLayer::Render()
{
	if (!m_initialized)
	{
		return true;
	}

	if (!m_d2dContext || !m_camera)
	{
		return false;
	}

	const float zoom = max(m_camera->GetZoom(), 0.0001f);
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	ROIRenderContext renderContext = {};
	renderContext.d2dContext = m_d2dContext;
	renderContext.strokeBrush = m_strokeBrush;
	renderContext.fillBrush = m_fillBrush;
	renderContext.handleFillBrush = m_handleFillBrush;
	renderContext.handleOutlineBrush = m_handleOutlineBrush;
	renderContext.zoom = zoom;
	renderContext.strokeWidth = 1.0f / zoom;
	renderContext.handleHalfSize = 4.0f / zoom;
	const Rect2f visibleRect = GetVisibleClientRect();
	const float visiblePadding = 6.0f;

	D2D1_MATRIX_3X2_F originalTransform = {};
	m_d2dContext->GetTransform(&originalTransform);
	m_d2dContext->SetTransform(
		D2D1::Matrix3x2F::Scale(zoom, zoom) *
		D2D1::Matrix3x2F::Translation(-offsetX * zoom, -offsetY * zoom));

	::AcquireSRWLockShared(&m_roiLock);

	for (const auto& roiObject : m_roiObjects)
	{
		if (roiObject.get() == m_selectedObject)
		{
			continue;
		}

		if (!IsVisibleOnClient(roiObject->GetBounds(), visibleRect, visiblePadding))
		{
			continue;
		}

		roiObject->Render(renderContext, false, roiObject.get() == m_hoveredObject);
	}

	if (m_selectedObject && IsVisibleOnClient(m_selectedObject->GetBounds(), visibleRect, visiblePadding))
	{
		m_selectedObject->Render(renderContext, true, m_selectedObject == m_hoveredObject);
	}

	::ReleaseSRWLockShared(&m_roiLock);

	m_d2dContext->SetTransform(originalTransform);
	return true;
}

void ROIRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void ROIRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

void ROIRenderLayer::SetCamera2D(const Camera2D* camera)
{
	m_camera = camera;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIRectangleRenderer* rectangleObject = static_cast<ROIRectangleRenderer*>(FindObjectByKey(key, ROIObjectType::Rectangle));

	if (!rectangleObject)
	{
		RemoveObjectByKey(key);
		auto newRectangleObject = std::make_unique<ROIRectangleRenderer>(key);
		rectangleObject = newRectangleObject.get();
		m_roiObjects.push_back(std::move(newRectangleObject));
	}

	const bool result = rectangleObject->UpdateDefinition(name, rect, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIEllipseRenderer* ellipseObject = static_cast<ROIEllipseRenderer*>(FindObjectByKey(key, ROIObjectType::Ellipse));

	if (!ellipseObject)
	{
		RemoveObjectByKey(key);
		auto newEllipseObject = std::make_unique<ROIEllipseRenderer>(key);
		ellipseObject = newEllipseObject.get();
		m_roiObjects.push_back(std::move(newEllipseObject));
	}

	const bool result = ellipseObject->UpdateDefinition(name, ellipse, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROICircleRenderer* circleObject = static_cast<ROICircleRenderer*>(FindObjectByKey(key, ROIObjectType::Circle));

	if (!circleObject)
	{
		RemoveObjectByKey(key);
		auto newCircleObject = std::make_unique<ROICircleRenderer>(key);
		circleObject = newCircleObject.get();
		m_roiObjects.push_back(std::move(newCircleObject));
	}

	const bool result = circleObject->UpdateDefinition(name, circle, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!key || key[0] == L'\0' || !polygon.IsValid())
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIPolygonRenderer* polygonObject = static_cast<ROIPolygonRenderer*>(FindObjectByKey(key, ROIObjectType::Polygon));

	if (!polygonObject)
	{
		RemoveObjectByKey(key);
		auto newPolygonObject = std::make_unique<ROIPolygonRenderer>(key);
		polygonObject = newPolygonObject.get();
		m_roiObjects.push_back(std::move(newPolygonObject));
	}

	const bool result = polygonObject->UpdateDefinition(name, polygon, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

void ROIRenderLayer::ROIClear()
{
	::AcquireSRWLockExclusive(&m_roiLock);
	m_roiObjects.clear();
	m_hoveredObject = nullptr;
	m_selectedObject = nullptr;
	m_activeObject = nullptr;
	m_activeHit = {};
	m_isDragging = false;
	::ReleaseSRWLockExclusive(&m_roiLock);
}

bool ROIRenderLayer::OnLButtonDown(float screenX, float screenY)
{
	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);
	const float tolerance = GetHitToleranceInImage();

	ROIHitResult hitResult = {};

	::AcquireSRWLockExclusive(&m_roiLock);
	IROIObject* hitObject = HitTest(imagePoint, tolerance, hitResult);

	m_selectedObject = hitObject;
	m_activeObject = hitObject;
	m_activeHit = hitResult;
	m_isDragging = hitObject != nullptr;
	m_hoveredObject = hitObject;

	if (m_activeObject)
	{
		m_activeObject->BeginDrag(imagePoint, hitResult);
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	return hitObject != nullptr;
}

bool ROIRenderLayer::OnMouseMove(float screenX, float screenY)
{
	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);
	bool changed = false;

	::AcquireSRWLockExclusive(&m_roiLock);

	if (m_isDragging && m_activeObject)
	{
		m_activeObject->UpdateDrag(imagePoint);
		changed = true;
	}
	else
	{
		ROIHitResult hitResult = {};
		IROIObject* hoveredObject = HitTest(imagePoint, GetHitToleranceInImage(), hitResult);
		changed = UpdateHoverObject(hoveredObject);
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	return changed;
}

bool ROIRenderLayer::OnLButtonUp(float screenX, float screenY)
{
	UNREFERENCED_PARAMETER(screenX);
	UNREFERENCED_PARAMETER(screenY);

	bool changed = false;

	::AcquireSRWLockExclusive(&m_roiLock);

	if (m_activeObject)
	{
		m_activeObject->EndDrag();
		m_activeObject = nullptr;
		m_activeHit = {};
		changed = true;
	}

	m_isDragging = false;

	::ReleaseSRWLockExclusive(&m_roiLock);

	return changed;
}

bool ROIRenderLayer::AcquireDeviceResources()
{
	m_d2dContext = m_context ? m_context->GetD2DDeviceContext() : nullptr;
	if (!m_d2dContext)
	{
		return false;
	}

	ReleaseDeviceResources();

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_strokeBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_fillBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_handleFillBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_handleOutlineBrush)))
	{
		return false;
	}

	return true;
}

void ROIRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_strokeBrush);
	SafeRelease(m_fillBrush);
	SafeRelease(m_handleFillBrush);
	SafeRelease(m_handleOutlineBrush);
}

Rect2f ROIRenderLayer::GetVisibleClientRect() const
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

Rect2f ROIRenderLayer::ToScreenBounds(const Rect2f& bounds) const
{
	if (!m_camera)
	{
		return {};
	}

	const float zoom = max(m_camera->GetZoom(), 0.0001f);
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	Rect2f screenBounds = {
		(bounds.left - offsetX) * zoom,
		(bounds.top - offsetY) * zoom,
		(bounds.right - offsetX) * zoom,
		(bounds.bottom - offsetY) * zoom
	};
	screenBounds.Normalize();
	return screenBounds;
}

bool ROIRenderLayer::IsVisibleOnClient(const Rect2f& bounds, const Rect2f& visibleRect, float padding) const
{
	Rect2f screenBounds = ToScreenBounds(bounds);
	screenBounds.Inflate(padding, padding);
	return screenBounds.Intersects(visibleRect);
}

Point2f ROIRenderLayer::ScreenToImage(float screenX, float screenY) const
{
	Point2f imagePoint = {};
	imagePoint.x = m_camera->GetOffsetX() + screenX / max(m_camera->GetZoom(), 0.0001f);
	imagePoint.y = m_camera->GetOffsetY() + screenY / max(m_camera->GetZoom(), 0.0001f);
	return imagePoint;
}

float ROIRenderLayer::GetHitToleranceInImage() const
{
	if (!m_camera)
	{
		return 6.0f;
	}

	return 6.0f / max(m_camera->GetZoom(), 0.0001f);
}

IROIObject* ROIRenderLayer::FindObjectByKey(const wchar_t* key) const
{
	if (!key)
	{
		return nullptr;
	}

	for (const auto& roiObject : m_roiObjects)
	{
		if (roiObject->GetKey() == key)
		{
			return roiObject.get();
		}
	}

	return nullptr;
}

IROIObject* ROIRenderLayer::FindObjectByKey(const wchar_t* key, ROIObjectType objectType) const
{
	IROIObject* roiObject = FindObjectByKey(key);
	if (!roiObject || roiObject->GetObjectType() != objectType)
	{
		return nullptr;
	}

	return roiObject;
}

void ROIRenderLayer::RemoveObjectByKey(const wchar_t* key)
{
	if (!key)
	{
		return;
	}

	for (auto iterator = m_roiObjects.begin(); iterator != m_roiObjects.end(); ++iterator)
	{
		if ((*iterator)->GetKey() != key)
		{
			continue;
		}

		IROIObject* roiObject = iterator->get();
		if (m_hoveredObject == roiObject)
		{
			m_hoveredObject = nullptr;
		}
		if (m_selectedObject == roiObject)
		{
			m_selectedObject = nullptr;
		}
		if (m_activeObject == roiObject)
		{
			m_activeObject = nullptr;
			m_activeHit = {};
			m_isDragging = false;
		}

		m_roiObjects.erase(iterator);
		return;
	}
}

IROIObject* ROIRenderLayer::HitTest(const Point2f& imagePoint, float tolerance, ROIHitResult& hitResult) const
{
	IROIObject* hitObject = nullptr;
	float minDistance = FLT_MAX;

	for (auto iterator = m_roiObjects.rbegin(); iterator != m_roiObjects.rend(); ++iterator)
	{
		const Rect2f& bounds = (*iterator)->GetBounds();
		Rect2f expandedBounds = bounds;
		expandedBounds.Inflate(tolerance, tolerance);

		if (!expandedBounds.Contains(imagePoint))
		{
			continue;
		}

		ROIHitResult currentHit = (*iterator)->HitTest(imagePoint, tolerance);
		if (!currentHit.IsHit())
		{
			continue;
		}

		if (currentHit.distance < minDistance)
		{
			minDistance = currentHit.distance;
			hitResult = currentHit;
			hitObject = iterator->get();

			if (currentHit.type != ROIHitType::Body)
			{
				break;
			}
		}
	}

	return hitObject;
}

bool ROIRenderLayer::UpdateHoverObject(IROIObject* hoveredObject)
{
	if (m_hoveredObject == hoveredObject)
	{
		return false;
	}

	m_hoveredObject = hoveredObject;
	return true;
}




