#include "pch.h"
#include "OverlayPointRenderer.h"

#include "OverlayRenderContext.h"

using namespace Core::ShapeType;

namespace
{
	float ResolveStrokeWidth(const OverlayRenderContext& context, const OverlayStyle& style)
	{
		float strokeWidth = style.strokeWidth;
		if (context.mode == ImageOverlayMode::ImageSpace && context.scale > 0.0f)
		{
			strokeWidth = max(style.strokeWidth / context.scale, 1.0f);
		}

		return strokeWidth;
	}
}

OverlayPointRenderer::OverlayPointRenderer(const OverlayPoint& point)
	: m_point(point)
{
	m_bounds = { m_point.x, m_point.y, m_point.x, m_point.y };
}

OverlayShapeType OverlayPointRenderer::GetShapeType() const
{
	return OverlayShapeType::Point;
}

const Rect2f& OverlayPointRenderer::GetBounds() const
{
	return m_bounds;
}

void OverlayPointRenderer::Render(const OverlayRenderContext& context) const
{
	if (!context.d2dContext || !context.strokeBrush)
	{
		return;
	}

	context.strokeBrush->SetColor(m_point.style.strokeColorD2D);

	const float strokeWidth = ResolveStrokeWidth(context, m_point.style);
	const D2D1_POINT_2F point = { m_point.x + 0.5f, m_point.y + 0.5f };

	if (context.scale >= 3.0f)
	{
		DrawCross(context.d2dContext, point, max(1.0f, strokeWidth), context.strokeBrush);
	}
	else
	{
		DrawDot(context.d2dContext, point, max(2.0f, strokeWidth), context.strokeBrush);
	}
}

void OverlayPointRenderer::DrawCross(ID2D1DeviceContext* d2dContext, const D2D1_POINT_2F& point, float strokeWidth, ID2D1SolidColorBrush* brush)
{
	constexpr float half = 0.5f;

	const D2D1_POINT_2F up = { point.x, point.y - half };
	const D2D1_POINT_2F down = { point.x, point.y + half };
	const D2D1_POINT_2F left = { point.x - half, point.y };
	const D2D1_POINT_2F right = { point.x + half, point.y };

	d2dContext->DrawLine(up, down, brush, strokeWidth);
	d2dContext->DrawLine(left, right, brush, strokeWidth);
}

void OverlayPointRenderer::DrawDot(ID2D1DeviceContext* d2dContext, const D2D1_POINT_2F& point, float size, ID2D1SolidColorBrush* brush)
{
	const float half = size * 0.5f;

	const D2D1_RECT_F rect = {
		point.x - half,
		point.y - half,
		point.x + half,
		point.y + half
	};

	d2dContext->FillRectangle(rect, brush);
}

