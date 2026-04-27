#pragma once

#include <d2d1_1.h>

enum class ImageOverlayMode
{
	ImageSpace,
	WindowSpace
};

struct OverlayRenderContext
{
	ID2D1DeviceContext* d2dContext = nullptr;
	ID2D1Factory1* d2dFactory = nullptr;
	ID2D1SolidColorBrush* strokeBrush = nullptr;
	ID2D1SolidColorBrush* fillBrush = nullptr;

	float scale = 0.0f;
	D2D1::Matrix3x2F imageTransform = D2D1::Matrix3x2F::Identity();
	ImageOverlayMode mode = ImageOverlayMode::ImageSpace;
};

