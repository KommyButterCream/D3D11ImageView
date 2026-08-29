#pragma once

#include "IROIObject.h"

#include "../../../Module/Core/ShapeType/Line2f.h"

#include <string>

struct ID2D1SolidColorBrush;

// 두 점을 잇는 ROI.
//
// 거리 측정 도구가 이걸 만들지만, 호스트가 ROISet 으로 직접 넣을 수도 있다.
// 둘 다 만들어진 뒤에는 동작이 같다 — 끝점 두 개를 잡아 옮기거나 몸통을 잡아
// 통째로 이동한다.
class ROILineRenderer : public IROIObject
{
public:
	explicit ROILineRenderer(const wchar_t* key);
	~ROILineRenderer() override;

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Line2f& line,
		COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);

	// 측정 중 두 번째 점만 갱신한다. 이름/색은 건드리지 않는다.
	void SetEndPoint(const Core::ShapeType::Point2f& point);

	const Core::ShapeType::Line2f& GetLine() const { return m_line; }

	// IROIObject
	ROIObjectType GetObjectType() const override;
	const std::wstring& GetKey() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;

	bool IsMovable() const override;
	bool IsResizable() const override;

	const std::wstring& GetName() const override;
	uint32_t GetColorRGB() const override;
	int32_t GetFontSize() const override;
	void GetShape(ROIShapeData& outShape) const override;
	uint32_t GetVertices(Core::ShapeType::Point2f* buffer, uint32_t capacity,
		uint32_t segmentsPerCurve) const override;

	void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const override;
	void OnDeviceLost() override;
	ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const override;

	void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) override;
	void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) override;
	void EndDrag() override;

private:
	void UpdateBounds();

private:
	std::wstring m_key;
	std::wstring m_name;
	Core::ShapeType::Line2f m_line = {};
	Core::ShapeType::Rect2f m_bounds = {};

	uint32_t m_colorRGB = 0;
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	int32_t m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	Core::ShapeType::Line2f m_dragStartLine = {};
};
