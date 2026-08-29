#include "pch.h"
#include "ROILineRenderer.h"

#include "ROIUtilities.h"

using namespace Core::ShapeType;

namespace
{
	// 끝점 핸들 인덱스. HitTest 와 UpdateDrag 가 같은 값을 써야 한다.
	constexpr int kHandleStart = 0;
	constexpr int kHandleEnd = 1;
}

ROILineRenderer::ROILineRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

ROILineRenderer::~ROILineRenderer()
{
}

bool ROILineRenderer::UpdateDefinition(const wchar_t* name, const Line2f& line,
	COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	m_name = name ? name : L"";
	m_line = line;
	m_strokeColor = ROIUtilities::ConvertColor(rgb);

	// 원본 COLORREF 를 따로 보관한다. D2D1_COLOR_F 에서 역변환하면
	// 부동소수 반올림으로 원래 값이 그대로 돌아오지 않는다.
	m_colorRGB = static_cast<uint32_t>(rgb);

	m_isMovable = isMovable;
	m_isResizable = isResizable;
	m_fontSize = fontSize;

	UpdateBounds();
	return true;
}

void ROILineRenderer::SetEndPoint(const Point2f& point)
{
	m_line.ex = point.x;
	m_line.ey = point.y;
	UpdateBounds();
}

ROIObjectType ROILineRenderer::GetObjectType() const
{
	return ROIObjectType::Line;
}

const std::wstring& ROILineRenderer::GetKey() const
{
	return m_key;
}

const std::wstring& ROILineRenderer::GetName() const
{
	return m_name;
}

uint32_t ROILineRenderer::GetColorRGB() const
{
	return m_colorRGB;
}

int32_t ROILineRenderer::GetFontSize() const
{
	return m_fontSize;
}

void ROILineRenderer::GetShape(ROIShapeData& outShape) const
{
	outShape = {};
	outShape.type = ROIObjectType::Line;
	outShape.vertexCount = 2;

	outShape.u.line.x1 = m_line.sx;
	outShape.u.line.y1 = m_line.sy;
	outShape.u.line.x2 = m_line.ex;
	outShape.u.line.y2 = m_line.ey;
}

uint32_t ROILineRenderer::GetVertices(Point2f* buffer, uint32_t capacity, uint32_t /*segmentsPerCurve*/) const
{
	constexpr uint32_t kNeeded = 2;

	if (!buffer || capacity < kNeeded)
		return kNeeded;

	buffer[0] = { m_line.sx, m_line.sy };
	buffer[1] = { m_line.ex, m_line.ey };

	return kNeeded;
}

const Rect2f& ROILineRenderer::GetBounds() const
{
	return m_bounds;
}

bool ROILineRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROILineRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROILineRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
{
	if (!context.d2dContext || !context.strokeBrush)
	{
		return;
	}

	context.strokeBrush->SetColor(m_strokeColor);

	float strokeWidth = context.strokeWidth;
	if (isSelected)
	{
		strokeWidth *= 1.5f;
	}

	const D2D1_POINT_2F start = { m_line.sx, m_line.sy };
	const D2D1_POINT_2F end = { m_line.ex, m_line.ey };

	context.d2dContext->DrawLine(start, end, context.strokeBrush, strokeWidth);

	// 끝점은 항상 보여준다. 다른 도형과 달리 선은 잡을 곳이 두 점뿐이라,
	// hover 해야 핸들이 나오면 어디를 눌러야 할지 알 수 없다.
	ROIUtilities::DrawHandle(context, { m_line.sx, m_line.sy }, m_strokeColor);
	ROIUtilities::DrawHandle(context, { m_line.ex, m_line.ey }, m_strokeColor);

	UNREFERENCED_PARAMETER(isHovered);
}

ROIHitResult ROILineRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
{
	ROIHitResult hitResult = {};

	// 끝점이 먼저다. 몸통보다 좁은 표적이라 순서가 뒤바뀌면 절대 잡히지 않는다.
	if (m_isResizable)
	{
		const Point2f start = { m_line.sx, m_line.sy };
		const Point2f end = { m_line.ex, m_line.ey };

		const float startDistance = ROIUtilities::DistanceToPoint(imagePoint, start);
		const float endDistance = ROIUtilities::DistanceToPoint(imagePoint, end);

		// 두 끝점이 겹쳐 있을 때(측정 시작 직후) 끝점 쪽을 집어야
		// 이어서 드래그가 자연스럽다.
		if (endDistance <= tolerance && endDistance <= startDistance)
		{
			hitResult.type = ROIHitType::Vertex;
			hitResult.distance = endDistance;
			hitResult.handleIndex = kHandleEnd;
			return hitResult;
		}

		if (startDistance <= tolerance)
		{
			hitResult.type = ROIHitType::Vertex;
			hitResult.distance = startDistance;
			hitResult.handleIndex = kHandleStart;
			return hitResult;
		}
	}

	if (m_isMovable)
	{
		const float distance = ROIUtilities::DistanceToSegment(imagePoint,
			{ m_line.sx, m_line.sy }, { m_line.ex, m_line.ey });

		if (distance <= tolerance)
		{
			hitResult.type = ROIHitType::Body;
			hitResult.distance = distance;
		}
	}

	return hitResult;
}

void ROILineRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;
	m_dragStartLine = m_line;
}

void ROILineRenderer::UpdateDrag(const Point2f& imagePoint)
{
	// 프레임마다 누적하지 않고 시작 스냅샷에서 다시 계산한다.
	// 누적하면 부동소수 오차가 쌓이고, 마우스를 시작점으로 되돌려도
	// 원래 위치로 돌아오지 않는다.
	switch (m_activeHit.type)
	{
	case ROIHitType::Body:
	{
		const float deltaX = imagePoint.x - m_dragStartImagePoint.x;
		const float deltaY = imagePoint.y - m_dragStartImagePoint.y;

		m_line = m_dragStartLine;
		m_line.Offset(deltaX, deltaY);
		break;
	}
	case ROIHitType::Vertex:
		m_line = m_dragStartLine;

		if (m_activeHit.handleIndex == kHandleStart)
		{
			m_line.SetStart(imagePoint.x, imagePoint.y);
		}
		else
		{
			m_line.SetEnd(imagePoint.x, imagePoint.y);
		}
		break;

	case ROIHitType::None:
	default:
		return;
	}

	UpdateBounds();
}

void ROILineRenderer::EndDrag()
{
	m_activeHit = {};
}

void ROILineRenderer::OnDeviceLost()
{
	// 버릴 것이 없다. 다각형과 달리 선은 ID2D1PathGeometry 를 캐시하지 않고
	// DrawLine 으로 바로 그리므로 디바이스 독립 리소스를 들고 있지 않다.
}

void ROILineRenderer::UpdateBounds()
{
	// 선은 축 정렬이 아니므로 두 끝점을 감싸는 사각형을 만든다.
	// 레이어의 컬링과 라벨 앵커가 이 값을 쓴다.
	m_bounds = {
		min(m_line.sx, m_line.ex),
		min(m_line.sy, m_line.ey),
		max(m_line.sx, m_line.ex),
		max(m_line.sy, m_line.ey)
	};
}
