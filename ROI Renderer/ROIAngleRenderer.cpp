#include "pch.h"
#include "ROIAngleRenderer.h"

#include "ROIUtilities.h"

using namespace Core::ShapeType;

namespace
{
	// 핸들 인덱스. HitTest 와 UpdateDrag 가 같은 값을 써야 한다.
	constexpr int kHandleFirst = 0;
	constexpr int kHandleVertex = 1;
	constexpr int kHandleSecond = 2;

	// 꼭짓점에 그리는 호의 반지름(화면 픽셀).
	//
	// 이미지 좌표가 아니라 화면 기준이다. 이미지 좌표로 잡으면 확대할수록
	// 호가 같이 커져서 화면을 덮어 버린다.
	constexpr float kArcScreenRadius = 22.0f;

	float LengthOf(const Point2f& v)
	{
		return sqrtf(v.x * v.x + v.y * v.y);
	}
}

ROIAngleRenderer::ROIAngleRenderer(const wchar_t* key)
{
	if (key)
	{
		m_key = key;
	}
}

ROIAngleRenderer::~ROIAngleRenderer()
{
}

bool ROIAngleRenderer::UpdateDefinition(const wchar_t* name,
	const Point2f& first, const Point2f& vertex, const Point2f& second,
	COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	m_name = name ? name : L"";

	m_first = first;
	m_vertex = vertex;
	m_second = second;

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

void ROIAngleRenderer::SetVertex(const Point2f& point)
{
	m_vertex = point;
	UpdateBounds();
}

void ROIAngleRenderer::SetSecond(const Point2f& point)
{
	m_second = point;
	UpdateBounds();
}

float ROIAngleRenderer::GetAngleDegrees() const
{
	const Point2f arm1 = { m_first.x - m_vertex.x, m_first.y - m_vertex.y };
	const Point2f arm2 = { m_second.x - m_vertex.x, m_second.y - m_vertex.y };

	const float length1 = LengthOf(arm1);
	const float length2 = LengthOf(arm2);

	// 아직 점이 겹쳐 있는 단계(첫 클릭 직후)에는 각이 정의되지 않는다.
	if (length1 <= 1e-6f || length2 <= 1e-6f)
	{
		return 0.0f;
	}

	// atan2 의 차이가 아니라 내적을 쓴다. 그래야 결과가 바로 0~180 이고,
	// 부호나 2pi 감싸기를 따로 손볼 필요가 없다.
	float cosValue = (arm1.x * arm2.x + arm1.y * arm2.y) / (length1 * length2);

	// 부동소수 오차로 1 을 아주 살짝 넘으면 acosf 가 NaN 을 준다.
	if (cosValue > 1.0f) cosValue = 1.0f;
	if (cosValue < -1.0f) cosValue = -1.0f;

	return acosf(cosValue) * 180.0f / 3.14159265358979323846f;
}

ROIObjectType ROIAngleRenderer::GetObjectType() const
{
	return ROIObjectType::Angle;
}

const std::wstring& ROIAngleRenderer::GetKey() const
{
	return m_key;
}

const std::wstring& ROIAngleRenderer::GetName() const
{
	return m_name;
}

uint32_t ROIAngleRenderer::GetColorRGB() const
{
	return m_colorRGB;
}

int32_t ROIAngleRenderer::GetFontSize() const
{
	return m_fontSize;
}

void ROIAngleRenderer::GetShape(ROIShapeData& outShape) const
{
	outShape = {};
	outShape.type = ROIObjectType::Angle;
	outShape.vertexCount = 3;

	outShape.u.angle.x1 = m_first.x;
	outShape.u.angle.y1 = m_first.y;
	outShape.u.angle.vx = m_vertex.x;
	outShape.u.angle.vy = m_vertex.y;
	outShape.u.angle.x2 = m_second.x;
	outShape.u.angle.y2 = m_second.y;
}

uint32_t ROIAngleRenderer::GetVertices(Point2f* buffer, uint32_t capacity, uint32_t /*segmentsPerCurve*/) const
{
	constexpr uint32_t kNeeded = 3;

	if (!buffer || capacity < kNeeded)
		return kNeeded;

	// 그리는 순서 그대로 준다. 첫 점 -> 꼭짓점 -> 둘째 점.
	buffer[0] = m_first;
	buffer[1] = m_vertex;
	buffer[2] = m_second;

	return kNeeded;
}

const Rect2f& ROIAngleRenderer::GetBounds() const
{
	return m_bounds;
}

bool ROIAngleRenderer::IsMovable() const
{
	return m_isMovable;
}

bool ROIAngleRenderer::IsResizable() const
{
	return m_isResizable;
}

void ROIAngleRenderer::Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const
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

	const D2D1_POINT_2F first = { m_first.x, m_first.y };
	const D2D1_POINT_2F vertex = { m_vertex.x, m_vertex.y };
	const D2D1_POINT_2F second = { m_second.x, m_second.y };

	context.d2dContext->DrawLine(first, vertex, context.strokeBrush, strokeWidth);
	context.d2dContext->DrawLine(vertex, second, context.strokeBrush, strokeWidth);

	// ── 각을 표시하는 호
	//
	// 두 변 위에서 꼭짓점으로부터 같은 거리에 있는 두 점을 잇는다. 정확한
	// 원호 대신 선분 몇 개로 근사한다 — ID2D1PathGeometry 를 만들면 팩토리가
	// 재생성될 때 캐시를 버리는 처리가 따라붙는데, 이 크기에서는 눈으로
	// 구분되지 않는다.
	const Point2f arm1 = { m_first.x - m_vertex.x, m_first.y - m_vertex.y };
	const Point2f arm2 = { m_second.x - m_vertex.x, m_second.y - m_vertex.y };

	const float length1 = LengthOf(arm1);
	const float length2 = LengthOf(arm2);

	if (length1 > 1e-6f && length2 > 1e-6f)
	{
		// 화면에서 일정한 크기로 보이도록 이미지 좌표로 되돌린다.
		const float zoom = (context.zoom > 1e-6f) ? context.zoom : 1.0f;
		float radius = kArcScreenRadius / zoom;

		// 변보다 길면 호가 변 밖으로 삐져나온다. 짧은 변의 절반으로 막는다.
		const float shortest = (length1 < length2) ? length1 : length2;
		if (radius > shortest * 0.5f)
		{
			radius = shortest * 0.5f;
		}

		const Point2f unit1 = { arm1.x / length1, arm1.y / length1 };
		const Point2f unit2 = { arm2.x / length2, arm2.y / length2 };

		const float startAngle = atan2f(unit1.y, unit1.x);
		float sweep = atan2f(unit2.y, unit2.x) - startAngle;

		// 항상 짧은 쪽으로 돈다. 그래야 호가 사잇각을 가리킨다.
		constexpr float kPi = 3.14159265358979323846f;
		while (sweep > kPi) sweep -= 2.0f * kPi;
		while (sweep < -kPi) sweep += 2.0f * kPi;

		constexpr int kArcSegments = 16;

		D2D1_POINT_2F previous = {
			m_vertex.x + cosf(startAngle) * radius,
			m_vertex.y + sinf(startAngle) * radius
		};

		for (int i = 1; i <= kArcSegments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kArcSegments);
			const float angle = startAngle + sweep * t;

			const D2D1_POINT_2F current = {
				m_vertex.x + cosf(angle) * radius,
				m_vertex.y + sinf(angle) * radius
			};

			context.d2dContext->DrawLine(previous, current, context.strokeBrush, context.strokeWidth);
			previous = current;
		}
	}

	// 핸들은 항상 보여준다. 선과 마찬가지로 잡을 곳이 점 세 개뿐이라,
	// hover 해야 나오면 어디를 눌러야 할지 알 수 없다.
	ROIUtilities::DrawHandle(context, m_first, m_strokeColor);
	ROIUtilities::DrawHandle(context, m_vertex, m_strokeColor);
	ROIUtilities::DrawHandle(context, m_second, m_strokeColor);

	UNREFERENCED_PARAMETER(isHovered);
}

ROIHitResult ROIAngleRenderer::HitTest(const Point2f& imagePoint, float tolerance) const
{
	ROIHitResult hitResult = {};

	// 점이 먼저다. 변보다 좁은 표적이라 순서가 뒤바뀌면 절대 잡히지 않는다.
	if (m_isResizable)
	{
		const Point2f points[3] = { m_first, m_vertex, m_second };
		const int handles[3] = { kHandleFirst, kHandleVertex, kHandleSecond };

		int bestHandle = -1;
		float bestDistance = tolerance;

		// 측정 직후에는 세 점이 겹쳐 있을 수 있다. 뒤에서부터 보면 아직 안
		// 찍은 점(둘째 -> 꼭짓점)이 먼저 잡혀 이어지는 드래그가 자연스럽다.
		for (int i = 2; i >= 0; --i)
		{
			const float distance = ROIUtilities::DistanceToPoint(imagePoint, points[i]);
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				bestHandle = handles[i];
			}
		}

		if (bestHandle >= 0)
		{
			hitResult.type = ROIHitType::Vertex;
			hitResult.distance = bestDistance;
			hitResult.handleIndex = bestHandle;
			return hitResult;
		}
	}

	if (m_isMovable)
	{
		const float distance1 = ROIUtilities::DistanceToSegment(imagePoint, m_first, m_vertex);
		const float distance2 = ROIUtilities::DistanceToSegment(imagePoint, m_vertex, m_second);

		const float distance = (distance1 < distance2) ? distance1 : distance2;

		if (distance <= tolerance)
		{
			hitResult.type = ROIHitType::Body;
			hitResult.distance = distance;
		}
	}

	return hitResult;
}

void ROIAngleRenderer::BeginDrag(const Point2f& imagePoint, const ROIHitResult& hitResult)
{
	m_activeHit = hitResult;
	m_dragStartImagePoint = imagePoint;

	m_dragStartFirst = m_first;
	m_dragStartVertex = m_vertex;
	m_dragStartSecond = m_second;
}

void ROIAngleRenderer::UpdateDrag(const Point2f& imagePoint)
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

		m_first = { m_dragStartFirst.x + deltaX, m_dragStartFirst.y + deltaY };
		m_vertex = { m_dragStartVertex.x + deltaX, m_dragStartVertex.y + deltaY };
		m_second = { m_dragStartSecond.x + deltaX, m_dragStartSecond.y + deltaY };
		break;
	}
	case ROIHitType::Vertex:
	{
		m_first = m_dragStartFirst;
		m_vertex = m_dragStartVertex;
		m_second = m_dragStartSecond;

		if (m_activeHit.handleIndex == kHandleFirst)
		{
			m_first = imagePoint;
		}
		else if (m_activeHit.handleIndex == kHandleSecond)
		{
			m_second = imagePoint;
		}
		else
		{
			// 꼭짓점을 잡으면 두 변이 함께 따라온다. 각도만 바뀌는 것이
			// 아니라 도형이 꺾이는 지점이 옮겨지는 것이다.
			m_vertex = imagePoint;
		}
		break;
	}
	case ROIHitType::None:
	default:
		return;
	}

	UpdateBounds();
}

void ROIAngleRenderer::EndDrag()
{
	m_activeHit = {};
}

void ROIAngleRenderer::OnDeviceLost()
{
	// 버릴 것이 없다. 호까지 DrawLine 으로 그려서 디바이스 독립 리소스를
	// 들고 있지 않다.
}

void ROIAngleRenderer::UpdateBounds()
{
	// 세 점을 감싸는 사각형. 레이어의 컬링과 라벨 앵커가 이 값을 쓴다.
	const float minX = min(m_first.x, min(m_vertex.x, m_second.x));
	const float minY = min(m_first.y, min(m_vertex.y, m_second.y));
	const float maxX = max(m_first.x, max(m_vertex.x, m_second.x));
	const float maxY = max(m_first.y, max(m_vertex.y, m_second.y));

	m_bounds = { minX, minY, maxX, maxY };
}
