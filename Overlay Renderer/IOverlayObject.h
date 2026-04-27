#pragma once

#include "OverlayTypes.h"

#include "../../../Module/Core/ShapeType/Rect2f.h"

struct OverlayRenderContext;

class IOverlayObject
{
public:
	virtual ~IOverlayObject() = default;

	virtual OverlayShapeType GetShapeType() const = 0;
	virtual const Core::ShapeType::Rect2f& GetBounds() const = 0;
	virtual void Render(const OverlayRenderContext& context) const = 0;
};




