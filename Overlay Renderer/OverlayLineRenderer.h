#pragma once

#include "IOverlayObject.h"

class OverlayLineRenderer : public IOverlayObject
{
public:
	explicit OverlayLineRenderer(const OverlayLine& line);

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;

private:
	OverlayLine m_line = {};
	Core::ShapeType::Rect2f m_bounds = {};
};

