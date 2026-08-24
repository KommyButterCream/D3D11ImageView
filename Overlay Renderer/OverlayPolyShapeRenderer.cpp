#include "pch.h"
#include "OverlayPolyShapeRenderer.h"

#include "OverlayRenderContext.h"
#include "OverlayUtilities.h"

#include <utility> // for std::move

using namespace Core::ShapeType;

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

OverlayPolyShapeRenderer::~OverlayPolyShapeRenderer()
{
	SafeRelease(m_geometry);
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

	// 도형이 불변이므로 한 번만 만들고 계속 재사용한다.
	// 실패했으면 매 프레임 다시 시도하지 않는다.
	if (!m_geometry && !m_geometryBuildFailed)
	{
		m_geometry = OverlayUtilities::CreatePolyGeometry(
			context.d2dFactory,
			m_polyShape.points,
			m_polyShape.pointCount,
			m_polyShape.isClosed);

		m_geometryBuildFailed = (m_geometry == nullptr);
	}

	if (!m_geometry)
	{
		return;
	}

	const OverlayStyle& style = m_polyShape.style;
	const float strokeWidth = OverlayUtilities::ResolveStrokeWidth(context, style);

	context.strokeBrush->SetColor(style.strokeColorD2D);
	context.fillBrush->SetColor(style.fillColorD2D);

	// 주의: 다른 렌더러(Ellipse/Rectangle)는 style.transparentFill 을 보는데
	// 여기만 보지 않는다. 기존 동작을 유지하려고 그대로 두었다.
	if (m_polyShape.isClosed)
	{
		context.d2dContext->FillGeometry(m_geometry, context.fillBrush);
	}

	context.d2dContext->DrawGeometry(m_geometry, context.strokeBrush, strokeWidth);
}

void OverlayPolyShapeRenderer::OnDeviceLost()
{
	// 지오메트리는 ID2D1Factory 소속인데, 이 엔진은 디바이스 재생성 시
	// D2D 팩토리까지 다시 만든다. 옛 팩토리의 지오메트리를 새 컨텍스트로
	// 그리면 D2DERR_WRONG_FACTORY 가 나므로 버리고 다시 만들게 한다.
	SafeRelease(m_geometry);
	m_geometryBuildFailed = false;
}
