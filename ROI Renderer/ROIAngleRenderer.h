#pragma once

#include "IROIObject.h"

#include <string>

struct ID2D1SolidColorBrush;

// 세 점이 이루는 각을 재는 ROI.
//
// 가운데 점이 꼭짓점이고, 거기서 두 점으로 변이 뻗는다. 각도 측정 도구가
// 세 번의 클릭으로 만들지만, 만들어진 뒤에는 평범한 ROI 다 — 세 점을 각각
// 잡아 옮기거나 변을 잡아 통째로 이동할 수 있다.
//
// 각도는 항상 0~180도로 나온다. 두 변 사이의 사잇각이라 방향(시계/반시계)은
// 구분하지 않는다. 검사에서 묻는 것은 "몇 도로 꺾였나" 지 "어느 쪽으로
// 돌았나" 가 아니다.
class ROIAngleRenderer : public IROIObject
{
public:
	explicit ROIAngleRenderer(const wchar_t* key);
	~ROIAngleRenderer() override;

	bool UpdateDefinition(const wchar_t* name,
		const Core::ShapeType::Point2f& first,
		const Core::ShapeType::Point2f& vertex,
		const Core::ShapeType::Point2f& second,
		COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);

	// 측정 중에만 쓴다. 아직 안 찍은 점을 마우스 위치로 끌고 다닌다.
	void SetVertex(const Core::ShapeType::Point2f& point);
	void SetSecond(const Core::ShapeType::Point2f& point);

	// 두 변 사이의 사잇각(도). 변 하나라도 길이가 0이면 0을 준다.
	float GetAngleDegrees() const;

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

	Core::ShapeType::Point2f m_first = {};
	Core::ShapeType::Point2f m_vertex = {};
	Core::ShapeType::Point2f m_second = {};

	Core::ShapeType::Rect2f m_bounds = {};

	uint32_t m_colorRGB = 0;
	D2D1_COLOR_F m_strokeColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	bool m_isMovable = true;
	bool m_isResizable = true;
	int32_t m_fontSize = 14;

	ROIHitResult m_activeHit = {};
	Core::ShapeType::Point2f m_dragStartImagePoint = {};
	Core::ShapeType::Point2f m_dragStartFirst = {};
	Core::ShapeType::Point2f m_dragStartVertex = {};
	Core::ShapeType::Point2f m_dragStartSecond = {};
};
