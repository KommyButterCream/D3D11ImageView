#pragma once

#include "IOverlayObject.h"

class OverlayPolyShapeRenderer : public IOverlayObject
{
public:
	explicit OverlayPolyShapeRenderer(OverlayPolyShape&& polyShape);

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;

private:
	OverlayPolyShape m_polyShape = {};
	Core::ShapeType::Rect2f m_bounds = {};
};

