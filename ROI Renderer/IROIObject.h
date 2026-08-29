#pragma once

#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"
#include "../../../Module/Core/Util/MathUtil.h"

#include <float.h>
#include <stdint.h>
#include <string>

struct ROIRenderContext;

enum class ROIObjectType : uint8_t
{
	Rectangle,
	Ellipse,
	Circle,
	Polygon,
	Line
};

enum class ROIHitType : uint8_t
{
	None,
	Body,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	Left,
	Top,
	Right,
	Bottom,
	Vertex
};

struct ROIHitResult
{
	ROIHitType type = ROIHitType::None;
	float distance = FLT_MAX;
	int handleIndex = -1;

	bool IsHit() const noexcept
	{
		return type != ROIHitType::None;
	}
};

// 해석적(analytic) 형상. 조회 API 가 계측용으로 그대로 내보낸다.
//
// 정점 배열로만 돌려주면 원을 받아 다시 Set 할 때 다각형이 되어 편집 핸들과
// 계측 정확도(원 면적 = pi*r^2)를 잃는다. 무손실 왕복을 위해 이쪽이 1차다.
// Polygon 은 가변 길이라 union 에 담지 않고 vertexCount 만 알려준다.
struct ROIShapeData
{
	ROIObjectType type = ROIObjectType::Rectangle;
	uint32_t vertexCount = 0;      // Polygon 일 때만 유효

	union
	{
		struct { float left, top, right, bottom; } rect;
		struct { float cx, cy, rx, ry, angleRad; } ellipse;
		struct { float cx, cy, radius; }           circle;
		struct { float x1, y1, x2, y2; }     line;
	} u = {};
};

class IROIObject
{
public:
	virtual ~IROIObject() = default;

	virtual ROIObjectType GetObjectType() const = 0;
	virtual const std::wstring& GetKey() const = 0;
	virtual const Core::ShapeType::Rect2f& GetBounds() const = 0;

	virtual bool IsMovable() const = 0;
	virtual bool IsResizable() const = 0;

	// ── 조회용
	virtual const std::wstring& GetName() const = 0;
	virtual uint32_t GetColorRGB() const = 0;   // COLORREF 배치 (0x00BBGGRR)
	virtual int32_t GetFontSize() const = 0;

	// 해석적 형상
	virtual void GetShape(ROIShapeData& outShape) const = 0;

	// 정점 배열 (이미지 좌표). 반환값은 필요 개수.
	//   buffer 가 nullptr 이거나 capacity 가 부족하면 채우지 않고 개수만 반환한다.
	//   Rectangle -> 4점, Polygon -> 원본 정점
	//   Circle / Ellipse -> segmentsPerCurve 개로 근사
	virtual uint32_t GetVertices(Core::ShapeType::Point2f* buffer,
		uint32_t capacity,
		uint32_t segmentsPerCurve) const = 0;

	virtual void Render(const ROIRenderContext& context, bool isSelected, bool isHovered) const = 0;

	// D2D 팩토리가 재생성되면 이전 팩토리의 지오메트리는 쓸 수 없다
	// (D2DERR_WRONG_FACTORY). 디바이스 로스트 시 캐시를 버리기 위한 훅.
	virtual void OnDeviceLost() {}
	virtual ROIHitResult HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance) const = 0;

	virtual void BeginDrag(const Core::ShapeType::Point2f& imagePoint, const ROIHitResult& hitResult) = 0;
	virtual void UpdateDrag(const Core::ShapeType::Point2f& imagePoint) = 0;
	virtual void EndDrag() = 0;
};




