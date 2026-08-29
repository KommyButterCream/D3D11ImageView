#pragma once

#include "ROIRenderContext.h"

#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"

#include <math.h>
#include <vector>

namespace ROIUtilities
{
	constexpr float kMinShapeSize = 1.0f;

	inline D2D1_COLOR_F ConvertColor(COLORREF rgb)
	{
		return {
			static_cast<float>(GetRValue(rgb)) / 255.0f,
			static_cast<float>(GetGValue(rgb)) / 255.0f,
			static_cast<float>(GetBValue(rgb)) / 255.0f,
			1.0f
		};
	}

	inline float DistanceToPoint(const Core::ShapeType::Point2f& point1, const Core::ShapeType::Point2f& point2)
	{
		const float deltaX = point1.x - point2.x;
		const float deltaY = point1.y - point2.y;
		return sqrtf(deltaX * deltaX + deltaY * deltaY);
	}

	// 점에서 선분까지의 최단 거리.
	//
	// 무한 직선이 아니라 선분이므로, 수선의 발이 선분 밖으로 나가면 가까운
	// 끝점까지의 거리가 답이다. t 를 [0,1] 로 자르는 것이 그 처리다.
	inline float DistanceToSegment(const Core::ShapeType::Point2f& point,
		const Core::ShapeType::Point2f& start, const Core::ShapeType::Point2f& end)
	{
		const float dx = end.x - start.x;
		const float dy = end.y - start.y;
		const float lengthSq = dx * dx + dy * dy;

		// 두 끝점이 같으면 점까지의 거리다(측정 시작 직후가 이 상태다).
		if (lengthSq <= 1e-12f)
		{
			return DistanceToPoint(point, start);
		}

		float t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSq;
		t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

		const Core::ShapeType::Point2f projected = {
			start.x + dx * t,
			start.y + dy * t
		};

		return DistanceToPoint(point, projected);
	}

	inline Core::ShapeType::Point2f RotateVector(const Core::ShapeType::Point2f& point, float angleRad)
	{
		const float cosValue = cosf(angleRad);
		const float sinValue = sinf(angleRad);

		return {
			point.x * cosValue - point.y * sinValue,
			point.x * sinValue + point.y * cosValue
		};
	}

	inline Core::ShapeType::Point2f RotateAround(const Core::ShapeType::Point2f& point, const Core::ShapeType::Point2f& center, float angleRad)
	{
		Core::ShapeType::Point2f relativePoint = point - center;
		relativePoint = RotateVector(relativePoint, angleRad);
		return center + relativePoint;
	}

	inline Core::ShapeType::Point2f ToLocal(const Core::ShapeType::Point2f& point, const Core::ShapeType::Point2f& center, float angleRad)
	{
		return RotateVector(point - center, -angleRad);
	}

	inline void DrawHandle(const ROIRenderContext& context, const Core::ShapeType::Point2f& point, D2D1_COLOR_F outlineColor)
	{
		if (!context.d2dContext || !context.handleFillBrush || !context.handleOutlineBrush)
		{
			return;
		}

		const float handleHalfSize = context.handleHalfSize;
		const D2D1_RECT_F handleRect = {
			point.x - handleHalfSize,
			point.y - handleHalfSize,
			point.x + handleHalfSize,
			point.y + handleHalfSize
		};

		context.handleFillBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
		context.handleOutlineBrush->SetColor(outlineColor);

		context.d2dContext->FillRectangle(handleRect, context.handleFillBrush);
		context.d2dContext->DrawRectangle(handleRect, context.handleOutlineBrush, context.strokeWidth);
	}

	inline Core::ShapeType::Rect2f BuildBounds(const std::vector<Core::ShapeType::Point2f>& points)
	{
		if (points.empty())
		{
			return {};
		}

		float minX = points[0].x;
		float minY = points[0].y;
		float maxX = points[0].x;
		float maxY = points[0].y;

		for (size_t index = 1; index < points.size(); ++index)
		{
			minX = min(minX, points[index].x);
			minY = min(minY, points[index].y);
			maxX = max(maxX, points[index].x);
			maxY = max(maxY, points[index].y);
		}

		return { minX, minY, maxX, maxY };
	}

	inline bool PointInPolygon(const std::vector<Core::ShapeType::Point2f>& points, const Core::ShapeType::Point2f& point)
	{
		if (points.size() < 3)
		{
			return false;
		}

		bool inside = false;
		size_t previousIndex = points.size() - 1;

		for (size_t index = 0; index < points.size(); previousIndex = index++)
		{
			const Core::ShapeType::Point2f& currentPoint = points[index];
			const Core::ShapeType::Point2f& previousPoint = points[previousIndex];

			const bool intersects =
				((currentPoint.y > point.y) != (previousPoint.y > point.y)) &&
				(point.x < (previousPoint.x - currentPoint.x) * (point.y - currentPoint.y) /
					(previousPoint.y - currentPoint.y + 1e-8f) + currentPoint.x);

			if (intersects)
			{
				inside = !inside;
			}
		}

		return inside;
	}
}




