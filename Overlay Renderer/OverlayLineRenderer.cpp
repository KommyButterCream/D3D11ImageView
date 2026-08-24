#include "pch.h"
#include "OverlayLineRenderer.h"

#include "OverlayRenderContext.h"
#include "OverlayUtilities.h"

using namespace Core::ShapeType;

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

	const float strokeWidth = OverlayUtilities::ResolveStrokeWidth(context, m_line.style);
	const D2D1_POINT_2F beginPoint = { m_line.x1, m_line.y1 };
	const D2D1_POINT_2F endPoint = { m_line.x2, m_line.y2 };

	context.d2dContext->DrawLine(beginPoint, endPoint, context.strokeBrush, strokeWidth);
}

