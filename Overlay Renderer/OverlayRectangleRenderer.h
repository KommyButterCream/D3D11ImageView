#pragma once

#include "IOverlayObject.h"

struct ID2D1PathGeometry;

class OverlayRectangleRenderer : public IOverlayObject
{
public:
	explicit OverlayRectangleRenderer(const OverlayRect& rect);
	~OverlayRectangleRenderer() override;

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;
	void OnDeviceLost() override;

private:
	OverlayRect m_rect = {};
	Core::ShapeType::Rect2f m_bounds = {};

	mutable ID2D1PathGeometry* m_geometry = nullptr;
	mutable bool m_geometryBuildFailed = false;
};

