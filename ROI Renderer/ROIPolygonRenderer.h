#pragma once

#include "IROIObject.h"

#include "../../../Module/Core/ShapeType/Polygon2f.h"

#include <vector>

class ROIPolygonRenderer : public IROIObject
{
public:
	explicit ROIPolygonRenderer(const wchar_t* key);

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);

	ROIObjectType GetObjectType() const override;
	const std::wstring& GetKey() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;

	bool IsMovable() const override;
	bool IsResizable() const override;

	void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const override;
	ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const override;

	void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) override;
	void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) override;
	void EndDrag() override;

private:
	void UpdateBounds();

private:
	std::wstring m_key;
	std::wstring m_name;
	std::vector<Core::ShapeType::Point2f> m_points;
	Core::ShapeType::Rect2f m_bounds = {};
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	long m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	std::vector<Core::ShapeType::Point2f> m_dragStartPoints;
};




