#pragma once

#include "IROIObject.h"

#include "../../../Module/Core/ShapeType/Polygon2f.h"

#include <vector>

class ROIPolygonRenderer : public IROIObject
{
public:
	explicit ROIPolygonRenderer(const wchar_t* key);
	~ROIPolygonRenderer() override;

	bool UpdateDefinition(const wchar_t* name, const Core::ShapeType::Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, long fontSize);

	ROIObjectType GetObjectType() const override;
	const std::wstring& GetKey() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;

	bool IsMovable() const override;
	bool IsResizable() const override;

	void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const override;
	void OnDeviceLost() override;
	ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const override;

	void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) override;
	void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) override;
	void EndDrag() override;

private:
	void UpdateBounds();
	void InvalidateGeometry();

private:
	// 경로 지오메트리 캐시.
	//
	// 예전에는 Render() 마다 CreatePathGeometry + Open + AddLine*N + Close 를
	// 다시 돌렸다. 정점이 많은 폴리곤에서 비용이 크다.
	//
	// 오버레이와 달리 ROI 는 드래그로 좌표가 바뀌므로 무효화가 필요하다.
	// 좌표를 건드리는 곳은 UpdateDefinition / UpdateDrag 두 군데뿐이다.
	// ID2D1PathGeometry 는 디바이스 독립 리소스라 디바이스 로스트에도 살아남는다.
	mutable ID2D1PathGeometry* m_geometry = nullptr;
	mutable bool m_geometryDirty = true;

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




