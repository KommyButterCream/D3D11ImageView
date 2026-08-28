#pragma once

#include "IROIObject.h"

class ROIRectangleRenderer : public IROIObject
{
public:
	explicit ROIRectangleRenderer(const wchar_t* key);

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);

	ROIObjectType GetObjectType() const override;
	const std::wstring& GetKey() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;

	bool IsMovable() const override;
	bool IsResizable() const override;

	const std::wstring& GetName() const override;
	uint32_t GetColorRGB() const override;
	int32_t GetFontSize() const override;
	void GetShape(ROIShapeData& outShape) const override;
	uint32_t GetVertices(Core::ShapeType::Point2f* buffer, uint32_t capacity, uint32_t segmentsPerCurve) const override;

	void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const override;
	ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const override;

	void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) override;
	void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) override;
	void EndDrag() override;

private:
	std::wstring m_key;
	std::wstring m_name;
	Core::ShapeType::Rect2f m_rect = {};

	uint32_t m_colorRGB = 0;
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	int32_t m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	Core::ShapeType::Rect2f m_dragStartRect = {};
};

