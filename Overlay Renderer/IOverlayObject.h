#pragma once

#include "OverlayTypes.h"

#include "../../../Module/Core/ShapeType/Rect2f.h"
#include "../../../Module/Core/Util/MathUtil.h"

struct OverlayRenderContext;

class IOverlayObject
{
public:
	virtual ~IOverlayObject() = default;

	virtual OverlayShapeType GetShapeType() const = 0;
	virtual const Core::ShapeType::Rect2f& GetBounds() const = 0;
	virtual void Render(const OverlayRenderContext& context) const = 0;

	// D2D 팩토리가 재생성되면 이전 팩토리의 지오메트리는 쓸 수 없다
	// (D2DERR_WRONG_FACTORY). 디바이스 로스트 시 캐시를 버리기 위한 훅.
	virtual void OnDeviceLost() {}
};