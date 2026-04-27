#pragma once

#include "IOverlayObject.h"

class OverlayEllipseRenderer : public IOverlayObject
{
public:
	explicit OverlayEllipseRenderer(const OverlayEllipse& ellipse);

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;

private:
	OverlayEllipse m_ellipse = {};
	Core::ShapeType::Rect2f m_bounds = {};
};

