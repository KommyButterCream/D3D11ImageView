#pragma once

#include "OverlayRenderContext.h"
#include "OverlayTypes.h"

#include <d2d1_1.h>

namespace OverlayUtilities
{
	// ImageSpace 에서는 D2D 변환 행렬에 zoom 이 들어가 있으므로
	// 선 두께가 함께 확대된다. 화면상 두께를 유지하려면 scale 로 나눠야 한다.
	// 최소 1px 은 유지해 축소 시 선이 사라지지 않게 한다.
	inline float ResolveStrokeWidth(const OverlayRenderContext& context, const OverlayStyle& style)
	{
		if (context.mode == ImageOverlayMode::ImageSpace && context.scale > 0.0f)
		{
			const float scaled = style.strokeWidth / context.scale;
			return (scaled > 1.0f) ? scaled : 1.0f;
		}

		return style.strokeWidth;
	}

	// 경로 지오메트리를 만들어 점들을 잇는다.
	// 실패하면 nullptr. 성공 시 호출자가 Release 책임을 진다.
	inline ID2D1PathGeometry* CreatePolyGeometry(
		ID2D1Factory* factory,
		const D2D1_POINT_2F* points,
		size_t pointCount,
		bool isClosed)
	{
		if (!factory || !points || pointCount < 2)
		{
			return nullptr;
		}

		ID2D1PathGeometry* geometry = nullptr;
		if (FAILED(factory->CreatePathGeometry(&geometry)))
		{
			return nullptr;
		}

		ID2D1GeometrySink* sink = nullptr;
		if (FAILED(geometry->Open(&sink)))
		{
			geometry->Release();
			return nullptr;
		}

		sink->BeginFigure(points[0],
			isClosed ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);

		for (size_t index = 1; index < pointCount; ++index)
		{
			sink->AddLine(points[index]);
		}

		sink->EndFigure(isClosed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);

		const HRESULT hr = sink->Close();
		sink->Release();

		if (FAILED(hr))
		{
			geometry->Release();
			return nullptr;
		}

		return geometry;
	}
}
