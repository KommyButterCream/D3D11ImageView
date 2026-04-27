#pragma once

#include <d2d1_1.h>

struct ROIRenderContext
{
	ID2D1DeviceContext* d2dContext = nullptr;
	ID2D1SolidColorBrush* strokeBrush = nullptr;
	ID2D1SolidColorBrush* fillBrush = nullptr;
	ID2D1SolidColorBrush* handleFillBrush = nullptr;
	ID2D1SolidColorBrush* handleOutlineBrush = nullptr;

	float zoom = 1.0f;
	float strokeWidth = 1.0f;
	float handleHalfSize = 4.0f;
};
