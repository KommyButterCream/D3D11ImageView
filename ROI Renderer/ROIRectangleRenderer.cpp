#include "pch.h"
#include "ROIRectangleRenderer.h"

#include "ROIRenderContext.h"

#include <math.h>

using namespace Core::ShapeType;

namespace
{
	constexpr float kMinRectangleSize = 1.0f;
}

ROIRectangleRenderer::ROIRectangleRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

bool ROIRectangleRenderer::UpdateDefinition(const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	m_name = name ? name : L"";
	m_rect = rect;
	m_rect.Normalize();
	m_strokeColor = ConvertColor(rgb);
	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;

	return true;
}

ROIObjectType ROIRectangleRenderer::GetObjectType() const
{
	return ROIObjectType::Rectangle;
}

const std::wstring& ROIRectangleRenderer::GetKey() const
{
	return m_key;
}

const Rect2f& ROIRectangleRenderer::GetBounds() const
{
	return m_rect;
}

bool ROIRectangleRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROIRectangleRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROIRectangleRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush)
	{
		return;
	}

	const D2D1_RECT_F rect = {
		m_rect.left,
		m_rect.top,
		m_rect.right,
		m_rect.bottom
	};

	D2D1_COLOR_F fillColor = m_strokeColor;
	fillColor.a = isSelected ? 0.18f : (isHovered ? 0.12f : 0.08f);

	context.fillBrush->SetColor(fillColor);
	context.strokeBrush->SetColor(m_strokeColor);

	float strokeWidth = context.strokeWidth;
	if (isSelected)
	{
		strokeWidth *= 1.5f;
	}

	context.d2dContext->FillRectangle(rect, context.fillBrush);
	context.d2dContext->DrawRectangle(rect, context.strokeBrush, strokeWidth);

	if (isSelected || isHovered)
	{
		DrawHandle(context, { m_rect.left, m_rect.top }, m_strokeColor);
		DrawHandle(context, { m_rect.right, m_rect.top }, m_strokeColor);
		DrawHandle(context, { m_rect.left, m_rect.bottom }, m_strokeColor);
		DrawHandle(context, { m_rect.right, m_rect.bottom }, m_strokeColor);
	}
}

ROIHitResult ROIRectangleRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
{
	ROIHitResult hitResult = {};

	if (m_isResizable)
	{
		const Point2f topLeft = { m_rect.left, m_rect.top };
		const Point2f topRight = { m_rect.right, m_rect.top };
		const Point2f bottomLeft = { m_rect.left, m_rect.bottom };
		const Point2f bottomRight = { m_rect.right, m_rect.bottom };

		const float topLeftDistance = DistanceToPoint(imagePoint, topLeft);
		if (topLeftDistance <= tolerance)
		{
			hitResult.type = ROIHitType::TopLeft;
			hitResult.distance = topLeftDistance;
			return hitResult;
		}

		const float topRightDistance = DistanceToPoint(imagePoint, topRight);
		if (topRightDistance <= tolerance)
		{
			hitResult.type = ROIHitType::TopRight;
			hitResult.distance = topRightDistance;
			return hitResult;
		}

		const float bottomLeftDistance = DistanceToPoint(imagePoint, bottomLeft);
		if (bottomLeftDistance <= tolerance)
		{
			hitResult.type = ROIHitType::BottomLeft;
			hitResult.distance = bottomLeftDistance;
			return hitResult;
		}

		const float bottomRightDistance = DistanceToPoint(imagePoint, bottomRight);
		if (bottomRightDistance <= tolerance)
		{
			hitResult.type = ROIHitType::BottomRight;
			hitResult.distance = bottomRightDistance;
			return hitResult;
		}
	}

	if (m_isMovable && m_rect.Contains(imagePoint))
	{
		hitResult.type = ROIHitType::Body;
		hitResult.distance = 0.0f;
	}

	return hitResult;
}

void ROIRectangleRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;
	m_dragStartRect = m_rect;
}

void ROIRectangleRenderer::UpdateDrag(const Point2f& imagePoint)
{
	switch (m_activeHit.type)
	{
	case ROIHitType::Body:
	{
		const float deltaX = imagePoint.x - m_dragStartImagePoint.x;
		const float deltaY = imagePoint.y - m_dragStartImagePoint.y;
		m_rect = m_dragStartRect;
		m_rect.Offset(deltaX, deltaY);
		break;
	}
	case ROIHitType::TopLeft:
		m_rect.left = min(imagePoint.x, m_dragStartRect.right - kMinRectangleSize);
		m_rect.top = min(imagePoint.y, m_dragStartRect.bottom - kMinRectangleSize);
		m_rect.right = m_dragStartRect.right;
		m_rect.bottom = m_dragStartRect.bottom;
		break;
	case ROIHitType::TopRight:
		m_rect.left = m_dragStartRect.left;
		m_rect.top = min(imagePoint.y, m_dragStartRect.bottom - kMinRectangleSize);
		m_rect.right = max(imagePoint.x, m_dragStartRect.left + kMinRectangleSize);
		m_rect.bottom = m_dragStartRect.bottom;
		break;
	case ROIHitType::BottomLeft:
		m_rect.left = min(imagePoint.x, m_dragStartRect.right - kMinRectangleSize);
		m_rect.top = m_dragStartRect.top;
		m_rect.right = m_dragStartRect.right;
		m_rect.bottom = max(imagePoint.y, m_dragStartRect.top + kMinRectangleSize);
		break;
	case ROIHitType::BottomRight:
		m_rect.left = m_dragStartRect.left;
		m_rect.top = m_dragStartRect.top;
		m_rect.right = max(imagePoint.x, m_dragStartRect.left + kMinRectangleSize);
		m_rect.bottom = max(imagePoint.y, m_dragStartRect.top + kMinRectangleSize);
		break;
	case ROIHitType::None:
	default:
		break;
	}

	m_rect.Normalize();
}

void ROIRectangleRenderer::EndDrag()
{
	m_activeHit = {};
}

float ROIRectangleRenderer::DistanceToPoint(const Point2f& point1, const Point2f& point2)
{
	const float deltaX = point1.x - point2.x;
	const float deltaY = point1.y - point2.y;
	return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

D2D1_COLOR_F ROIRectangleRenderer::ConvertColor(COLORREF rgb)
{
	return {
		static_cast<float>(GetRValue(rgb)) / 255.0f,
		static_cast<float>(GetGValue(rgb)) / 255.0f,
		static_cast<float>(GetBValue(rgb)) / 255.0f,
		1.0f
	};
}

void ROIRectangleRenderer::DrawHandle(const ROIRenderContext& context, const Point2f& point, D2D1_COLOR_F outlineColor) const
{
	if (!context.handleFillBrush || !context.handleOutlineBrush)
	{
		return;
	}

	const float handleHalfSize = context.handleHalfSize;
	const D2D1_RECT_F handleRect = {
		point.x - handleHalfSize,
		point.y - handleHalfSize,
		point.x + handleHalfSize,
		point.y + handleHalfSize
	};

	context.handleFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
	context.handleOutlineBrush->SetColor(outlineColor);

	context.d2dContext->FillRectangle(handleRect, context.handleFillBrush);
	context.d2dContext->DrawRectangle(handleRect, context.handleOutlineBrush, context.strokeWidth);
}

