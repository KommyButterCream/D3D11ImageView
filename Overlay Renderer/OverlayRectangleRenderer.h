#pragma once

#include "IOverlayObject.h"

class OverlayRectangleRenderer : public IOverlayObject
{
public:
	explicit OverlayRectangleRenderer(const OverlayRect& rect);

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;

private:
	OverlayRect m_rect = {};
	Core::ShapeType::Rect2f m_bounds = {};
};

