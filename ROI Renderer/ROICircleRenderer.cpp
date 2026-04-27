#include "pch.h"
#include "ROICircleRenderer.h"

#include "ROIUtilities.h"

using namespace Core::ShapeType;

ROICircleRenderer::ROICircleRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

bool ROICircleRenderer::UpdateDefinition(const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	m_name = name ? name : L"";
	m_circle = circle;
	m_circle.radius = max(fabsf(m_circle.radius), ROIUtilities::kMinShapeSize);
	m_strokeColor = ROIUtilities::ConvertColor(rgb);
	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;
	UpdateBounds();
	return true;
}

ROIObjectType ROICircleRenderer::GetObjectType() const
{
	return ROIObjectType::Circle;
}

const std::wstring& ROICircleRenderer::GetKey() const
{
	return m_key;
}

const Rect2f& ROICircleRenderer::GetBounds() const
{
	return m_bounds;
}

bool ROICircleRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROICircleRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROICircleRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
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

	const D2D1_ELLIPSE ellipse = D2D1::Ellipse({ m_circle.x + 0.5f, m_circle.y + 0.5f }, m_circle.radius, m_circle.radius);

	context.d2dContext->FillEllipse(ellipse, context.fillBrush);
	context.d2dContext->DrawEllipse(ellipse, context.strokeBrush, strokeWidth);

	if (isSelected || isHovered)
	{
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Left), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Top), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Right), m_strokeColor);
		ROIUtilities::DrawHandle(context, GetHandlePoint(ROIHitType::Bottom), m_strokeColor);
	}
}

ROIHitResult ROICircleRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
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

	if (m_isMovable && m_circle.Contains(imagePoint))
	{
		hitResult.type = ROIHitType::Body;
		hitResult.distance = 0.0f;
	}

	return hitResult;
}

void ROICircleRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;
	m_dragStartCircle = m_circle;
}

void ROICircleRenderer::UpdateDrag(const Point2f& imagePoint)
{
	switch (m_activeHit.type)
	{
	case ROIHitType::Body:
	{
		const Point2f delta = imagePoint - m_dragStartImagePoint;
		m_circle = m_dragStartCircle;
		m_circle.x += delta.x;
		m_circle.y += delta.y;
		break;
	}
	case ROIHitType::Left:
	case ROIHitType::Right:
		m_circle = m_dragStartCircle;
		m_circle.radius = max(fabsf(imagePoint.x - m_dragStartCircle.x), ROIUtilities::kMinShapeSize);
		break;
	case ROIHitType::Top:
	case ROIHitType::Bottom:
		m_circle = m_dragStartCircle;
		m_circle.radius = max(fabsf(imagePoint.y - m_dragStartCircle.y), ROIUtilities::kMinShapeSize);
		break;
	case ROIHitType::None:
	default:
		return;
	}

	UpdateBounds();
}

void ROICircleRenderer::EndDrag()
{
	m_activeHit = {};
}

Point2f ROICircleRenderer::GetHandlePoint(ROIHitType hitType) const
{
	switch (hitType)
	{
	case ROIHitType::Left:
		return { m_circle.x - m_circle.radius, m_circle.y };
	case ROIHitType::Top:
		return { m_circle.x, m_circle.y - m_circle.radius };
	case ROIHitType::Right:
		return { m_circle.x + m_circle.radius, m_circle.y };
	case ROIHitType::Bottom:
		return { m_circle.x, m_circle.y + m_circle.radius };
	default:
		return { m_circle.x, m_circle.y };
	}
}

void ROICircleRenderer::UpdateBounds()
{
	m_bounds = m_circle.BoundingBoxF();
}

