#pragma once

#include "IROIObject.h"

class ROIRectangleRenderer : public IROIObject
{
public:
	explicit ROIRectangleRenderer(const wchar_t* key);

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);

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
	static float DistanceToPoint(const Core::ShapeType::Point2f& point1, const Core::ShapeType::Point2f& point2);
	static D2D1_COLOR_F ConvertColor(COLORREF rgb);
	void DrawHandle(const ROIRenderContext& context, const Core::ShapeType::Point2f& point, D2D1_COLOR_F outlineColor) const;

private:
	std::wstring m_key;
	std::wstring m_name;
	Core::ShapeType::Rect2f m_rect = {};
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	long m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	Core::ShapeType::Rect2f m_dragStartRect = {};
};

