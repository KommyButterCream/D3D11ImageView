#include "pch.h"
#include "ROIEllipseRenderer.h"

#include "ROIUtilities.h"

using namespace Core::ShapeType;

ROIEllipseRenderer::ROIEllipseRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

bool ROIEllipseRenderer::UpdateDefinition(const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	m_name = name ? name : L"";
	m_ellipse = ellipse;
	m_ellipse.radiusX = max(fabsf(m_ellipse.radiusX), ROIUtilities::kMinShapeSize);
	m_ellipse.radiusY = max(fabsf(m_ellipse.radiusY), ROIUtilities::kMinShapeSize);
	m_strokeColor = ROIUtilities::ConvertColor(rgb);
	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;
	UpdateBounds();
	return true;
}

ROIObjectType ROIEllipseRenderer::GetObjectType() const
{
	return ROIObjectType::Ellipse;
}

const std::wstring& ROIEllipseRenderer::GetKey() const
{
	return m_key;
}

const Rect2f& ROIEllipseRenderer::GetBounds() const
{
	return m_bounds;
}

bool ROIEllipseRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROIEllipseRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROIEllipseRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush)
	{
		return;
	}

	D2D1_COLOR_F fillColor = m_strokeColor;
	fillColor.a = isSelected ? 0.18f : (isHovered ? 0.12f : 0.08f);

	context.fillBrush->SetColor(fillColor);
	context.strokeBrush->SetColor(m_strokeColor);

	float strokeWidth = context.strokeWidth;
	if (isSelected)
	{
		strokeWidth *= 1.5f;
	}

	const D2D1_POINT_2F center = { m_ellipse.x + 0.5f, m_ellipse.y + 0.5f };
	const D2D1_ELLIPSE d2dEllipse = D2D1::Ellipse(center, m_ellipse.radiusX, m_ellipse.radiusY);
	const float angleDeg = m_ellipse.angleRad * 180.0f / 3.14159265358979323846f;

	D2D1_MATRIX_3X2_F originalTransform = {};
	context.d2dContext->GetTransform(&originalTransform);
	context.d2dContext->SetTransform(D2D1::Matrix3x2F::Rotation(angleDeg, center) * originalTransform);

	context.d2dContext->FillEllipse(d2dEllipse, context.fillBrush);
	context.d2dContext->DrawEllipse(d2dEllipse, context.strokeBrush, strokeWidth);
	context.d2dContext->SetTransform(originalTransform);

	if (isSelected || isHovered)
	{
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Left), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Top), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Right), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Bottom), m_strokeColor);
	}
}

ROIHitResult ROIEllipseRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
{
	ROIHitResult hitResult = {};

	if (m_isResizable)
	{
		const ROIHitType handleTypes[] = { ROIHitType::Left, ROIHitType::Top, ROIHitType::Right, ROIHitType::Bottom };
		for (ROIHitType handleType : handleTypes)
		{
			const float distance = ROIUtilities::DistanceToPoint(imagePoint, GetHandlePoint(handleType));
			if (distance <= tolerance)
			{
				hitResult.type = handleType;
				hitResult.distance = distance;
				return hitResult;
			}
		}
	}

	if (m_isMovable && m_ellipse.Contains(imagePoint))
	{
		hitResult.type = ROIHitType::Body;
		hitResult.distance = 0.0f;
	}

	return hitResult;
}

void ROIEllipseRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;
	m_dragStartEllipse = m_ellipse;
}

void ROIEllipseRenderer::UpdateDrag(const Point2f& imagePoint)
{
	if (m_activeHit.type == ROIHitType::Body)
	{
		const Point2f delta = imagePoint - m_dragStartImagePoint;
		m_ellipse = m_dragStartEllipse;
		m_ellipse.x += delta.x;
		m_ellipse.y += delta.y;
		UpdateBounds();
		return;
	}

	if (m_activeHit.type == ROIHitType::None)
	{
		return;
	}

	const Point2f localPoint = ROIUtilities::ToLocal(imagePoint, { m_dragStartEllipse.x, m_dragStartEllipse.y }, m_dragStartEllipse.angleRad);
	m_ellipse = m_dragStartEllipse;

	switch (m_activeHit.type)
	{
	case ROIHitType::Left:
	case ROIHitType::Right:
		m_ellipse.radiusX = max(fabsf(localPoint.x), ROIUtilities::kMinShapeSize);
		break;
	case ROIHitType::Top:
	case ROIHitType::Bottom:
		m_ellipse.radiusY = max(fabsf(localPoint.y), ROIUtilities::kMinShapeSize);
		break;
	default:
		break;
	}

	UpdateBounds();
}

void ROIEllipseRenderer::EndDrag()
{
	m_activeHit = {};
}

Point2f ROIEllipseRenderer::GetCenter() const
{
	return { m_ellipse.x, m_ellipse.y };
}

Point2f ROIEllipseRenderer::GetHandlePoint(ROIHitType hitType) const
{
	Point2f localPoint = {};

	switch (hitType)
	{
	case ROIHitType::Left:
		localPoint = { -m_ellipse.radiusX, 0.0f };
		break;
	case ROIHitType::Top:
		localPoint = { 0.0f, -m_ellipse.radiusY };
		break;
	case ROIHitType::Right:
		localPoint = { m_ellipse.radiusX, 0.0f };
		break;
	case ROIHitType::Bottom:
		localPoint = { 0.0f, m_ellipse.radiusY };
		break;
	default:
		break;
	}

	return GetCenter() + ROIUtilities::RotateVector(localPoint, m_ellipse.angleRad);
}

void ROIEllipseRenderer::UpdateBounds()
{
	m_bounds = m_ellipse.BoundingBoxF();
}

