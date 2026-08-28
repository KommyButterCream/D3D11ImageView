#pragma once

#include "IOverlayObject.h"

struct ID2D1PathGeometry;

class OverlayPolyShapeRenderer : public IOverlayObject
{
public:
	explicit OverlayPolyShapeRenderer(OverlayPolyShape&& polyShape);
	~OverlayPolyShapeRenderer() override;

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;
	void OnDeviceLost() override;

private:
	OverlayPolyShape m_polyShape = {};
	Core::ShapeType::Rect2f m_bounds = {};

	mutable ID2D1PathGeometry* m_geometry = nullptr;
	mutable bool m_geometryBuildFailed = false;
};
