#pragma once

#include "IROIObject.h"

#include "../../../Module/Core/ShapeType/Ellipse2f.h"

class ROIEllipseRenderer : public IROIObject
{
public:
	explicit ROIEllipseRenderer(const wchar_t* key);

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);

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
	Core::ShapeType::Point2f GetCenter() const;
	Core::ShapeType::Point2f GetHandlePoint(ROIHitType hitType) const;
	void UpdateBounds();

private:
	std::wstring m_key;
	std::wstring m_name;
	Core::ShapeType::Ellipse2f m_ellipse = {};
	Core::ShapeType::Rect2f m_bounds = {};
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	long m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	Core::ShapeType::Ellipse2f m_dragStartEllipse = {};
};




