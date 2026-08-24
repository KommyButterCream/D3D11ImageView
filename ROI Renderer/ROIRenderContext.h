#pragma once

#include <d2d1_1.h>

struct ROIRenderContext
{
	ID2D1DeviceContext* d2dContext = nullptr;

	// 경로 지오메트리 생성용. 예전에는 렌더러가 매 프레임
	// d2dContext->GetFactory() 로 얻어 쓰고 Release 했다(COM 참조 증감 왕복).
	// OverlayRenderContext 와 동일하게 여기서 넘겨준다.
	ID2D1Factory* d2dFactory = nullptr;

	ID2D1SolidColorBrush* strokeBrush = nullptr;
	ID2D1SolidColorBrush* fillBrush = nullptr;
	ID2D1SolidColorBrush* handleFillBrush = nullptr;
	ID2D1SolidColorBrush* handleOutlineBrush = nullptr;

	float zoom = 1.0f;
	float strokeWidth = 1.0f;
	float handleHalfSize = 4.0f;
};
