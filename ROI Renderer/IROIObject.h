#pragma once

#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"

#include <float.h>
#include <stdint.h>
#include <string>

struct ROIRenderContext;

enum class ROIObjectType : uint8_t
{
	Rectangle,
	Ellipse,
	Circle,
	Polygon
};

enum class ROIHitType : uint8_t
{
	None,
	Body,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	Left,
	Top,
	Right,
	Bottom,
	Vertex
};

struct ROIHitResult
{
	ROIHitType type = ROIHitType::None;
	float distance = FLT_MAX;
	int handleIndex = -1;

	bool IsHit() const noexcept
	{
		return type != ROIHitType::None;
	}
};

class IROIObject
{
public:
	virtual ~IROIObject() = default;

	virtual ROIObjectType GetObjectType() const = 0;
	virtual const std::wstring& GetKey() const = 0;
	virtual const Core::ShapeType::Rect2f& GetBounds() const = 0;

	virtual bool IsMovable() const = 0;
	virtual bool IsResizable() const = 0;

	virtual void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const = 0;
	virtual ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const = 0;

	virtual void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) = 0;
	virtual void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) = 0;
	virtual void EndDrag() = 0;
};




