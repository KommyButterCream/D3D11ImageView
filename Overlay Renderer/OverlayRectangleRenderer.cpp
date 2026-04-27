#include "pch.h"
#include "OverlayRectangleRenderer.h"

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

OverlayRectangleRenderer::OverlayRectangleRenderer(const OverlayRect& rect)
	: m_rect(rect)
{
	float minX = m_rect.p1.x;
	float maxX = m_rect.p1.x;
	float minY = m_rect.p1.y;
	float maxY = m_rect.p1.y;

	const Point2f points[3] = { m_rect.p2, m_rect.p3, m_rect.p4 };
	for (const Point2f& point : points)
	{
		minX = min(minX, point.x);
		maxX = max(maxX, point.x);
		minY = min(minY, point.y);
		maxY = max(maxY, point.y);
	}

	m_bounds = { minX, minY, maxX, maxY };
}

OverlayShapeType OverlayRectangleRenderer::GetShapeType() const
{
	return OverlayShapeType::Rectangle;
}

const Rect2f& OverlayRectangleRenderer::GetBounds() const
{
	return m_bounds;
}

void OverlayRectangleRenderer::Render(const OverlayRenderContext& context) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush || !context.d2dFactory)
	{
		return;
	}

	const OverlayStyle& style = m_rect.style;
	const float strokeWidth = ResolveStrokeWidth(context, style);

	context.strokeBrush->SetColor(style.strokeColorD2D);
	context.fillBrush->SetColor(style.fillColorD2D);

	const D2D1_POINT_2F point1{ m_rect.p1.x + 0.5f, m_rect.p1.y + 0.5f };
	const D2D1_POINT_2F point2{ m_rect.p2.x + 0.5f, m_rect.p2.y + 0.5f };
	const D2D1_POINT_2F point3{ m_rect.p3.x + 0.5f, m_rect.p3.y + 0.5f };
	const D2D1_POINT_2F point4{ m_rect.p4.x + 0.5f, m_rect.p4.y + 0.5f };

	if (!style.transparentFill)
	{
		ID2D1PathGeometry* geometry = nullptr;
		ID2D1GeometrySink* sink = nullptr;

		if (SUCCEEDED(context.d2dFactory->CreatePathGeometry(&geometry)) &&
			SUCCEEDED(geometry->Open(&sink)))
		{
			sink->BeginFigure(point1, D2D1_FIGURE_BEGIN_FILLED);
			sink->AddLine(point2);
			sink->AddLine(point3);
			sink->AddLine(point4);
			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			sink->Close();

			context.d2dContext->FillGeometry(geometry, context.fillBrush);
			context.d2dContext->DrawGeometry(geometry, context.strokeBrush, strokeWidth);

			sink->Release();
			geometry->Release();
		}
	}
	else
	{
		context.d2dContext->DrawLine(point1, point2, context.strokeBrush, strokeWidth);
		context.d2dContext->DrawLine(point2, point3, context.strokeBrush, strokeWidth);
		context.d2dContext->DrawLine(point3, point4, context.strokeBrush, strokeWidth);
		context.d2dContext->DrawLine(point4, point1, context.strokeBrush, strokeWidth);
	}
}

