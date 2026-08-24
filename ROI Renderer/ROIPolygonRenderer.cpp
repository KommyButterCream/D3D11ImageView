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

ROIPolygonRenderer::~ROIPolygonRenderer()
{
	SafeRelease(m_geometry);
}

bool ROIPolygonRenderer::UpdateDefinition(const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize)
{
	if (!polygon.IsValid())
	{
		return false;
	}

	m_name = name ? name : L"";
	m_strokeColor = ROIUtilities::ConvertColor(rgb);

	// 원본 COLORREF 를 따로 보관한다. D2D1_COLOR_F 에서 역변환하면
	// 부동소수 반올림으로 원래 값이 그대로 돌아오지 않는다.
	m_colorRGB = static_cast<uint32_t>(rgb);

	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;

	m_points.assign(polygon.GetVertices(), polygon.GetVertices() + polygon.Size());
	UpdateBounds();
	InvalidateGeometry();
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

const std::wstring& ROIPolygonRenderer::GetName() const
{
	return m_name;
}

uint32_t ROIPolygonRenderer::GetColorRGB() const
{
	return m_colorRGB;
}

int32_t ROIPolygonRenderer::GetFontSize() const
{
	return static_cast<int32_t>(m_fontSize);
}

void ROIPolygonRenderer::GetShape(ROIShapeData& outShape) const
{
	outShape = {};
	outShape.type = ROIObjectType::Polygon;
	outShape.vertexCount = static_cast<uint32_t>(m_points.size());

	// 다각형은 가변 길이라 union 에 담지 않는다. GetVertices 로 받는다.
}

uint32_t ROIPolygonRenderer::GetVertices(Core::ShapeType::Point2f* buffer,
	uint32_t capacity, uint32_t /*segmentsPerCurve*/) const
{
	const uint32_t needed = static_cast<uint32_t>(m_points.size());

	if (!buffer || capacity < needed)
		return needed;

	for (uint32_t i = 0; i < needed; ++i)
	{
		buffer[i] = m_points[i];
	}

	return needed;
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
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush ||
		!context.d2dFactory || m_points.size() < 3)
	{
		return;
	}

	// 좌표가 바뀌었을 때만 다시 만든다.
	if (m_geometryDirty)
	{
		SafeRelease(m_geometry);

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

		geometrySink->BeginFigure({ m_points[0].x, m_points[0].y }, D2D1_FIGURE_BEGIN_FILLED);

		for (size_t index = 1; index < m_points.size(); ++index)
		{
			geometrySink->AddLine({ m_points[index].x, m_points[index].y });
		}

		geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);

		const HRESULT hr = geometrySink->Close();
		geometrySink->Release();

		if (FAILED(hr))
		{
			pathGeometry->Release();
			return;
		}

		m_geometry = pathGeometry;
		m_geometryDirty = false;
	}

	if (!m_geometry)
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

	context.d2dContext->FillGeometry(m_geometry, context.fillBrush);
	context.d2dContext->DrawGeometry(m_geometry, context.strokeBrush, strokeWidth);

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
	InvalidateGeometry();
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

void ROIPolygonRenderer::InvalidateGeometry()
{
	m_geometryDirty = true;
}


void ROIPolygonRenderer::OnDeviceLost()
{
	// 지오메트리는 ID2D1Factory 소속인데, 이 엔진은 디바이스 재생성 시
	// D2D 팩토리까지 다시 만든다. 옛 팩토리의 지오메트리를 새 컨텍스트로
	// 그리면 D2DERR_WRONG_FACTORY 가 나므로 버리고 다시 만들게 한다.
	SafeRelease(m_geometry);
	m_geometryDirty = true;
}
