#include "pch.h"
#include "OverlayPolyShapeRenderer.h"

#include "OverlayRenderContext.h"

#include <utility> // for std::move

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

OverlayPolyShapeRenderer::OverlayPolyShapeRenderer(OverlayPolyShape&& polyShape)
	: m_polyShape(std::move(polyShape))
{
	if (!m_polyShape.points || m_polyShape.pointCount == 0)
	{
		m_bounds = {};
		return;
	}

	float minX = m_polyShape.points[0].x;
	float maxX = m_polyShape.points[0].x;
	float minY = m_polyShape.points[0].y;
	float maxY = m_polyShape.points[0].y;

	for (size_t pointIndex = 1; pointIndex < m_polyShape.pointCount; ++pointIndex)
	{
		const D2D1_POINT_2F& point = m_polyShape.points[pointIndex];
		minX = min(minX, point.x);
		maxX = max(maxX, point.x);
		minY = min(minY, point.y);
		maxY = max(maxY, point.y);
	}

	m_bounds = { minX, minY, maxX, maxY };
}

OverlayShapeType OverlayPolyShapeRenderer::GetShapeType() const
{
	return OverlayShapeType::Polygon;
}

const Rect2f& OverlayPolyShapeRenderer::GetBounds() const
{
	return m_bounds;
}

void OverlayPolyShapeRenderer::Render(const OverlayRenderContext& context) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush || !context.d2dFactory)
	{
		return;
	}

	if (!m_polyShape.points || m_polyShape.pointCount < 2)
	{
		return;
	}

	const OverlayStyle& style = m_polyShape.style;
	const float strokeWidth = ResolveStrokeWidth(context, style);

	ID2D1PathGeometry* pathGeometry = nullptr;
	if (FAILED(context.d2dFactory->CreatePathGeometry(&pathGeometry)))
	{
		return;
	}

	ID2D1GeometrySink* geometrySink = nullptr;
	if (FAILED(pathGeometry->Open(&geometrySink)))
	{
		pathGeometry->Release();
		return;
	}

	D2D1_POINT_2F firstPoint = m_polyShape.points[0];
	firstPoint.x += 0.5f;
	firstPoint.y += 0.5f;

	geometrySink->BeginFigure(
		firstPoint,
		m_polyShape.isClosed ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);

	for (size_t pointIndex = 1; pointIndex < m_polyShape.pointCount; ++pointIndex)
	{
		D2D1_POINT_2F point = m_polyShape.points[pointIndex];
		point.x += 0.5f;
		point.y += 0.5f;
		geometrySink->AddLine(point);
	}

	geometrySink->EndFigure(
		m_polyShape.isClosed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
	geometrySink->Close();
	geometrySink->Release();

	context.strokeBrush->SetColor(style.strokeColorD2D);
	context.fillBrush->SetColor(style.fillColorD2D);

	if (m_polyShape.isClosed)
	{
		context.d2dContext->FillGeometry(pathGeometry, context.fillBrush);
	}

	context.d2dContext->DrawGeometry(pathGeometry, context.strokeBrush, strokeWidth);
	pathGeometry->Release();
}

