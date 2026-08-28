#include "pch.h"
#include "OverlayRectangleRenderer.h"

#include "OverlayRenderContext.h"
#include "OverlayUtilities.h"

using namespace Core::ShapeType;

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
	const float strokeWidth = OverlayUtilities::ResolveStrokeWidth(context, style);

	context.strokeBrush->SetColor(style.strokeColorD2D);
	context.fillBrush->SetColor(style.fillColorD2D);

	const D2D1_POINT_2F point1{ m_rect.p1.x, m_rect.p1.y };
	const D2D1_POINT_2F point2{ m_rect.p2.x, m_rect.p2.y };
	const D2D1_POINT_2F point3{ m_rect.p3.x, m_rect.p3.y };
	const D2D1_POINT_2F point4{ m_rect.p4.x, m_rect.p4.y };

	if (!style.transparentFill)
	{
		if (!m_geometry && !m_geometryBuildFailed)
		{
			const D2D1_POINT_2F points[4] = { point1, point2, point3, point4 };

			m_geometry = OverlayUtilities::CreatePolyGeometry(
				context.d2dFactory, points, 4, true);

			if (!m_geometry)
			{
				m_geometryBuildFailed = true;
			}
		}

		if (m_geometry)
		{
			context.d2dContext->FillGeometry(m_geometry, context.fillBrush);
			context.d2dContext->DrawGeometry(m_geometry, context.strokeBrush, strokeWidth);
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

OverlayRectangleRenderer::~OverlayRectangleRenderer()
{
	SafeRelease(m_geometry);
}

void OverlayRectangleRenderer::OnDeviceLost()
{
	SafeRelease(m_geometry);
	m_geometryBuildFailed = false;
}
