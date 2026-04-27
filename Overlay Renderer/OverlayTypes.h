#pragma once

#include <stdint.h>
#include <d2d1helper.h> 

#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Resource/ColorRGBA8.h"
#include "../../../Module/Core/ShapeType/Point2f.h"


using Core::ShapeType::Point2f;

enum class OverlayShapeType : uint8_t
{
	Point,
	Line,
	Rectangle,
	Ellipse,
	Polygon,
	Text
};

struct OverlayStyle
{
	ColorRGBA8 fillColor;
	ColorRGBA8 strokeColor;

	D2D1_COLOR_F fillColorD2D = { 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F strokeColorD2D = { 0.0f, 0.0f, 0.0f, 0.0f };

	float strokeWidth = 1.f;
	bool transparentFill = false;

	void UpdateD2DColors() noexcept
	{
		constexpr float inv = 1.0f / 255.0f;

		fillColorD2D = fillColor.ToD2DColor();
		strokeColorD2D = strokeColor.ToD2DColor();
	}
};

struct OverlayPoint
{
	float x = 0.0f;
	float y = 0.0f;
	OverlayStyle style;
};

struct OverlayLine
{
	float x1 = 0.0f, y1 = 0.0f;
	float x2 = 0.0f, y2 = 0.0f;
	OverlayStyle style;
};

struct OverlayRect
{
	float cx = 0.0f, cy = 0.0f; // center
	float hx = 0.0f, hy = 0.0f; // half width, half height
	float angleRad = 0.0f; // rotation(rad)

	// draw points
	Point2f p1 = { 0.f, 0.f };
	Point2f p2 = { 0.f, 0.f };
	Point2f p3 = { 0.f, 0.f };
	Point2f p4 = { 0.f, 0.f };

	OverlayStyle style;
};

struct OverlayEllipse
{
	float cx = 0.0f, cy = 0.0f; // center
	float rx = 0.0f, ry = 0.0f; // half width, half height
	float angleRad = 0.0f; // rotation(rad)

	OverlayStyle style;
};

struct OverlayPolyShape
{
	bool isClosed = true; // polyline -> false, polygon -> true
	D2D1_POINT_2F* points = nullptr;
	size_t pointCount = 0;
	OverlayStyle style;

	OverlayPolyShape() = default;

	OverlayPolyShape(const OverlayPolyShape&) = delete;
	OverlayPolyShape& operator=(const OverlayPolyShape&) = delete;

	OverlayPolyShape(OverlayPolyShape&& rhs) noexcept
	{
		isClosed = rhs.isClosed;
		points = rhs.points;
		pointCount = rhs.pointCount;
		style = rhs.style;

		rhs.points = nullptr;
		rhs.pointCount = 0;
	}

	OverlayPolyShape& operator=(OverlayPolyShape&& rhs) noexcept
	{
		if (this != &rhs)
		{
			isClosed = rhs.isClosed;
			points = rhs.points;
			pointCount = rhs.pointCount;
			style = rhs.style;

			rhs.points = nullptr;
			rhs.pointCount = 0;
		}

		return *this;
	}

	~OverlayPolyShape()
	{
		if (points)
		{
			delete[] points;
			points = nullptr;
		}

		pointCount = 0;
	}
};

struct OverlayText
{
	float x = 0.0f;
	float y = 0.0f;

	const wchar_t* text = nullptr;
	uint32_t length = 0;

	float fontSize = 0;
	ColorRGBA8 color;
};




