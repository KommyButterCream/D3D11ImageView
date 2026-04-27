#include "pch.h"
#include "OverlayLineRenderer.h"

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

OverlayLineRenderer::OverlayLineRenderer(const OverlayLine& line)
	: m_line(line)
{
	m_bounds = {
		min(m_line.x1, m_line.x2),
		min(m_line.y1, m_line.y2),
		max(m_line.x1, m_line.x2),
		max(m_line.y1, m_line.y2)
	};
}

OverlayShapeType OverlayLineRenderer::GetShapeType() const
{
	return OverlayShapeType::Line;
}

const Rect2f& OverlayLineRenderer::GetBounds() const
{
	return m_bounds;
}

void OverlayLineRenderer::Render(const OverlayRenderContext& context) const
{
	if (!context.d2dContext || !context.strokeBrush)
	{
		return;
	}

	context.strokeBrush->SetColor(m_line.style.strokeColorD2D);

	const float strokeWidth = ResolveStrokeWidth(context, m_line.style);
	const D2D1_POINT_2F beginPoint = { m_line.x1 + 0.5f, m_line.y1 + 0.5f };
	const D2D1_POINT_2F endPoint = { m_line.x2 + 0.5f, m_line.y2 + 0.5f };

	context.d2dContext->DrawLine(beginPoint, endPoint, context.strokeBrush, strokeWidth);
}

