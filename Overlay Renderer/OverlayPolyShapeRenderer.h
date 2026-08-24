#pragma once

#include "IOverlayObject.h"

struct ID2D1PathGeometry;

class OverlayPolyShapeRenderer : public IOverlayObject
{
public:
	explicit OverlayPolyShapeRenderer(OverlayPolyShape&& polyShape);
	~OverlayPolyShapeRenderer() override;

	OverlayShapeType GetShapeType() const override;
	const Core::ShapeType::Rect2f& GetBounds() const override;
	void Render(const OverlayRenderContext& context) const override;
	void OnDeviceLost() override;

private:
	OverlayPolyShape m_polyShape = {};
	Core::ShapeType::Rect2f m_bounds = {};

	// 경로 지오메트리 캐시.
	//
	// 예전에는 Render() 마다 CreatePathGeometry + Open + AddLine*N + Close 를
	// 다시 돌렸다. 점이 많은 폴리곤에서 비용이 크다.
	//
	// 오버레이 도형은 생성자에서 받고 이후 변하지 않으므로 무효화가 필요 없다.
	// 팩토리는 렌더 컨텍스트로만 오기 때문에 최초 Render 에서 지연 생성한다.
	// ID2D1PathGeometry 는 디바이스 독립 리소스라 디바이스 로스트에도 살아남는다.
	mutable ID2D1PathGeometry* m_geometry = nullptr;
	mutable bool m_geometryBuildFailed = false;
};
