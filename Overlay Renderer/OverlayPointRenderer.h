#pragma once

#include "IOverlayObject.h"

class OverlayPointRenderer : public IOverlayObject
{
public:
	explicit OverlayPointRenderer(const OverlayPoint& point);

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;

private:
	static void DrawCross(ID2D1DeviceContext* d2dContext, const D2D1_POINT_2F& point, float strokeWidth, ID2D1SolidColorBrush* brush);
	static void DrawDot(ID2D1DeviceContext* d2dContext, const D2D1_POINT_2F& point, float size, ID2D1SolidColorBrush* brush);

private:
	OverlayPoint m_point = {};
	Core::ShapeType::Rect2f m_bounds = {};
};

