#pragma once

#include "IOverlayObject.h"

struct ID2D1PathGeometry;

class OverlayRectangleRenderer : public IOverlayObject
{
public:
	explicit OverlayRectangleRenderer(const OverlayRect& rect);
	~OverlayRectangleRenderer() override;

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;
	void OnDeviceLost() override;

private:
	OverlayRect m_rect = {};
	Core::ShapeType::Rect2f m_bounds = {};

	// 경로 지오메트리 캐시. PolyShape 와 같은 이유다.
	//
	// 이 렌더러는 임의 사각형(회전/사다리꼴 포함)을 4점 경로로 그리므로
	// 채움이 있을 때 지오메트리가 필요하다. 예전에는 Render() 마다
	// CreatePathGeometry + Open + AddLine*4 + Close + Release 를 돌렸다.
	//
	// 도형은 생성자에서 받고 변하지 않으므로 무효화가 필요 없다.
	// 팩토리는 렌더 컨텍스트로만 오기 때문에 최초 Render 에서 지연 생성한다.
	mutable ID2D1PathGeometry* m_geometry = nullptr;
	mutable bool m_geometryBuildFailed = false;
};

