#include "pch.h"
#include "ROIPolygonRenderer.h"

#include "ROIUtilities.h"

using namespace Core::ShapeType;

ROIPolygonRenderer::ROIPolygonRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

bool ROIPolygonRenderer::UpdateDefinition(const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!polygon.IsValid())
	{
		return false;
	}

	m_name = name ? name : L"";
	m_strokeColor = ROIUtilities::ConvertColor(rgb);
	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;

	m_points.assign(polygon.GetVertices(), polygon.GetVertices() + polygon.Size());
	UpdateBounds();
	return true;
}

ROIObjectType ROIPolygonRenderer::GetObjectType() const
{
	return ROIObjectType::Polygon;
}

const std::wstring& ROIPolygonRenderer::GetKey() const
{
	return m_key;
}

const Rect2f& ROIPolygonRenderer::GetBounds() const
{
	return m_bounds;
}

bool ROIPolygonRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROIPolygonRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROIPolygonRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush || m_points.size() < 3)
	{
		return;
	}

	ID2D1Factory* d2dFactory = nullptr;
	context.d2dContext->GetFactory(&d2dFactory);
	if (!d2dFactory)
	{
		return;
	}

	ID2D1PathGeometry* pathGeometry = nullptr;
	if (FAILED(d2dFactory->CreatePathGeometry(&pathGeometry)))
	{
		d2dFactory->Release();
		return;
	}

	ID2D1GeometrySink* geometrySink = nullptr;
	if (FAILED(pathGeometry->Open(&geometrySink)))
	{
		pathGeometry->Release();
		d2dFactory->Release();
		return;
	}

	D2D1_POINT_2F firstPoint = { m_points[0].x + 0.5f, m_points[0].y + 0.5f };
	geometrySink->BeginFigure(firstPoint, D2D1_FIGURE_BEGIN_FILLED);

	for (size_t index = 1; index < m_points.size(); ++index)
	{
		geometrySink->AddLine({ m_points[index].x + 0.5f, m_points[index].y + 0.5f });
	}

	geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
	geometrySink->Close();
	geometrySink->Release();
	d2dFactory->Release();

	D2D1_COLOR_F fillColor = m_strokeColor;
	fillColor.a = isSelected ? 0.18f : (isHovered ? 0.12f : 0.08f);

	context.fillBrush->SetColor(fillColor);
	context.strokeBrush->SetColor(m_strokeColor);

	float strokeWidth = context.strokeWidth;
	if (isSelected)
	{
		strokeWidth *= 1.5f;
	}

	context.d2dContext->FillGeometry(pathGeometry, context.fillBrush);
	context.d2dContext->DrawGeometry(pathGeometry, context.strokeBrush, strokeWidth);
	pathGeometry->Release();

	if (isSelected || isHovered)
	{
		for (const Point2f& point : m_points)
		{
			ROIUtilities::DrawHandle(context, point, m_strokeColor);
		}
	}
}

ROIHitResult ROIPolygonRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
{
	ROIHitResult hitResult = {};

	if (m_isResizable)
	{
		for (size_t index = 0; index < m_points.size(); ++index)
		{
			const float distance = ROIUtilities::DistanceToPoint(imagePoint, m_points[index]);
			if (distance <= tolerance)
			{
				hitResult.type = ROIHitType::Vertex;
				hitResult.distance = distance;
				hitResult.handleIndex = static_cast<int>(index);
				return hitResult;
			}
		}
	}

	if (m_isMovable && ROIUtilities::PointInPolygon(m_points, imagePoint))
	{
		hitResult.type = ROIHitType::Body;
		hitResult.distance = 0.0f;
	}

	return hitResult;
}

void ROIPolygonRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;
	m_dragStartPoints = m_points;
}

void ROIPolygonRenderer::UpdateDrag(const Point2f& imagePoint)
{
	if (m_activeHit.type == ROIHitType::None)
	{
		return;
	}

	m_points = m_dragStartPoints;

	if (m_activeHit.type == ROIHitType::Body)
	{
		const Point2f delta = imagePoint - m_dragStartImagePoint;
		for (Point2f& point : m_points)
		{
			point += delta;
		}
	}
	else if (m_activeHit.type == ROIHitType::Vertex &&
		m_activeHit.handleIndex >= 0 &&
		m_activeHit.handleIndex < static_cast<int>(m_points.size()))
	{
		m_points[static_cast<size_t>(m_activeHit.handleIndex)] = imagePoint;
	}

	UpdateBounds();
}

void ROIPolygonRenderer::EndDrag()
{
	m_activeHit = {};
	m_dragStartPoints.clear();
}

void ROIPolygonRenderer::UpdateBounds()
{
	m_bounds = ROIUtilities::BuildBounds(m_points);
}

