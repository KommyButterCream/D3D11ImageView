// D3D11ImageView — flat C ABI 구현
//
// 이 파일에는 로직이 없다. 전부 D3D11ImageView_Impl 로 위임한다.
// 로직이 들어가면 C++ API 와 C API 의 동작이 갈라진다.
//
// 규칙(D3D11ImageViewC.h 참조)
//   - 예외를 경계 밖으로 내보내지 않는다. 모든 본문을 try/catch 로 감싼다.
//   - 소유권을 넘기지 않는다. 출력은 호출자 버퍼 + 2회 호출.

#include "pch.h"

#include "D3D11ImageViewC.h"
#include "D3D11ImageView_Impl.h"

#include "../Overlay Renderer/OverlayTypes.h"
#include "../Render Layer/ROIRenderLayer.h"

#include <new>

namespace
{
	// 불투명 핸들 <-> 구현 객체.
	//
	// Impl 포인터를 그대로 노출하지 않고 한 겹 감싼다. 나중에 유효성 표식이나
	// 참조 카운트를 넣을 자리를 남겨두기 위함이다.
	struct ViewerHandle
	{
		static constexpr uint32_t kMagic = 0x44334956u;   // 'D3IV'

		uint32_t magic = kMagic;
		D3D11ImageView_Impl* impl = nullptr;
	};

	inline ViewerHandle* ToHandle(D3IV_Viewer* viewer) noexcept
	{
		ViewerHandle* handle = reinterpret_cast<ViewerHandle*>(viewer);
		if (!handle || handle->magic != ViewerHandle::kMagic || !handle->impl)
			return nullptr;

		return handle;
	}

	inline D3D11ImageView_Impl* ToImpl(D3IV_Viewer* viewer) noexcept
	{
		ViewerHandle* handle = ToHandle(viewer);
		return handle ? handle->impl : nullptr;
	}

	// 2회 호출 패턴의 공통 반환 규칙.
	//   needed == 0            -> 대상이 없다 (NOT_FOUND)
	//   buffer 부족            -> BUFFER_TOO_SMALL, outCount 에 필요 개수
	//   충족                   -> OK
	inline D3IV_Result FinishSizedCall(uint32_t needed, uint32_t capacity,
		uint32_t* outCount) noexcept
	{
		if (outCount)
		{
			*outCount = needed;
		}

		if (needed == 0)
			return D3IV_ERR_NOT_FOUND;

		return (capacity < needed) ? D3IV_ERR_BUFFER_TOO_SMALL : D3IV_OK;
	}

	// OverlayStyle 변환. C 구조체는 structSize 를 갖지만 현재는 필드가 고정이다.
	inline bool ToOverlayStyle(const D3IV_OverlayStyle* src, OverlayStyle& out) noexcept
	{
		if (!src)
			return false;

		out.fillColor = { src->fillColor.r, src->fillColor.g,
						  src->fillColor.b, src->fillColor.a };
		out.strokeColor = { src->strokeColor.r, src->strokeColor.g,
							src->strokeColor.b, src->strokeColor.a };
		out.strokeWidth = src->strokeWidth;
		out.transparentFill = (src->transparentFill != 0);
		out.UpdateD2DColors();

		return true;
	}

	// ROIObjectType -> D3IV_ROIShapeType
	inline int32_t ToCShapeType(ROIObjectType type) noexcept
	{
		switch (type)
		{
		case ROIObjectType::Ellipse:   return D3IV_ROI_ELLIPSE;
		case ROIObjectType::Circle:    return D3IV_ROI_CIRCLE;
		case ROIObjectType::Polygon:   return D3IV_ROI_POLYGON;
		case ROIObjectType::Rectangle:
		default:                       return D3IV_ROI_RECTANGLE;
		}
	}

	// 호스트 콜백을 Impl 시그니처로 이어주는 트램폴린.
	//
	// 콜백 포인터와 userData 를 한 덩어리로 보관해야 하므로 핸들 옆에 둔다.
	struct CallbackSlot
	{
		D3IV_MouseCallback mouseCallback = nullptr;
		void* mouseUserData = nullptr;

		D3IV_ROIEventCallback roiCallback = nullptr;
		void* roiUserData = nullptr;
	};

	// 뷰어 하나당 하나. 핸들에 담기 위해 ViewerHandle 을 확장하는 대신
	// 별도 맵을 두지 않고 핸들 뒤에 붙여 할당한다.
	struct ViewerBlock
	{
		ViewerHandle handle;
		CallbackSlot callbacks;
		D3D11ImageView_Impl impl;
	};

	inline ViewerBlock* ToBlock(D3IV_Viewer* viewer) noexcept
	{
		ViewerHandle* handle = ToHandle(viewer);
		if (!handle)
			return nullptr;

		return reinterpret_cast<ViewerBlock*>(handle);
	}

	bool MouseTrampoline(const D3D11ImageView_Impl::MouseEventData& data, void* userData)
	{
		CallbackSlot* slot = static_cast<CallbackSlot*>(userData);
		if (!slot || !slot->mouseCallback)
			return false;

		D3IV_MouseEvent e = {};
		e.structSize = sizeof(e);
		e.type = static_cast<int32_t>(data.type);
		e.screenX = data.screenX;
		e.screenY = data.screenY;
		e.imageX = data.imageX;
		e.imageY = data.imageY;
		e.isInsideImage = data.isInsideImage ? 1 : 0;
		e.wheelDelta = data.wheelDelta;
		e.modifiers = data.modifiers;
		e.buttons = data.buttons;

		return slot->mouseCallback(&e, slot->mouseUserData) != 0;
	}

	void ROITrampoline(ROIRenderLayer::ROIEvent event, const wchar_t* key, void* userData)
	{
		CallbackSlot* slot = static_cast<CallbackSlot*>(userData);
		if (!slot || !slot->roiCallback)
			return;

		slot->roiCallback(static_cast<int32_t>(event), key, slot->roiUserData);
	}
}

// 모든 함수 본문을 감싸는 매크로.
// C++ 예외가 CLR 로 전파되면 동작이 정의되지 않는다.
#define D3IV_BEGIN(viewerParam)                              \
	D3D11ImageView_Impl* self = ToImpl(viewerParam);          \
	if (!self) return D3IV_ERR_INVALID_HANDLE;                \
	try {

#define D3IV_END                                             \
	} catch (...) { return D3IV_ERR_FAILED; }

	/*=====================================================
		오버레이

		도형 타입 열거형으로 21종 오버로드를 하나로 합친다.
		items 는 해당 타입의 배열이고 count 개를 읽는다.
	=====================================================*/
	namespace
	{
		// image / window 양쪽에서 같은 분기를 쓰므로 헬퍼로 뺀다.
		template <bool IsImageSpace>
		D3IV_Result AddOverlay(D3D11ImageView_Impl* self, int32_t shapeType,
			const void* items, uint32_t count, const OverlayStyle& style)
		{
			// 람다로 감싸면 오버로드 해석이 되지 않으므로 매크로로 분기한다.
#define D3IV_ADD(TypeName)                                                    \
			do {                                                              \
				const TypeName* typed = static_cast<const TypeName*>(items);   \
				if (IsImageSpace) self->ImageOverlayAdd(typed, count, style);  \
				else              self->WindowOverlayAdd(typed, count, style); \
				return D3IV_OK;                                               \
			} while (false)

			switch (shapeType)
			{
			case D3IV_SHAPE_POINT2I:     D3IV_ADD(Point2i);
			case D3IV_SHAPE_POINT2F:     D3IV_ADD(Point2f);
			case D3IV_SHAPE_POINT2D:     D3IV_ADD(Point2d);
			case D3IV_SHAPE_LINE2I:      D3IV_ADD(Line2i);
			case D3IV_SHAPE_LINE2F:      D3IV_ADD(Line2f);
			case D3IV_SHAPE_LINE2D:      D3IV_ADD(Line2d);
			case D3IV_SHAPE_RECT2I:      D3IV_ADD(Rect2i);
			case D3IV_SHAPE_RECT2F:      D3IV_ADD(Rect2f);
			case D3IV_SHAPE_RECT2D:      D3IV_ADD(Rect2d);
			case D3IV_SHAPE_QUADRECT2I:  D3IV_ADD(QuadRect2i);
			case D3IV_SHAPE_QUADRECT2F:  D3IV_ADD(QuadRect2f);
			case D3IV_SHAPE_QUADRECT2D:  D3IV_ADD(QuadRect2d);
			case D3IV_SHAPE_ROTRECT2I:   D3IV_ADD(RotatedRect2i);
			case D3IV_SHAPE_ROTRECT2F:   D3IV_ADD(RotatedRect2f);
			case D3IV_SHAPE_ROTRECT2D:   D3IV_ADD(RotatedRect2d);
			case D3IV_SHAPE_CIRCLE2F:    D3IV_ADD(Circle2f);
			case D3IV_SHAPE_CIRCLE2D:    D3IV_ADD(Circle2d);
			case D3IV_SHAPE_ELLIPSE2F:   D3IV_ADD(Ellipse2f);
			case D3IV_SHAPE_ELLIPSE2D:   D3IV_ADD(Ellipse2d);

				// Polyline2f / Polygon2f 는 힙을 소유하는 타입이라 C 경계에서
				// 배열로 받을 수 없다. 전용 함수를 따로 두어야 한다.
			case D3IV_SHAPE_POLYLINE2F:
			case D3IV_SHAPE_POLYGON2F:
				return D3IV_ERR_UNSUPPORTED;

			default:
				return D3IV_ERR_INVALID_ARG;
			}

#undef D3IV_ADD
		}
	}

extern "C" {

	/*=====================================================
		버전 / 생성
	=====================================================*/
	void D3IV_CALL D3IV_GetVersion(int32_t* outMajor, int32_t* outMinor, int32_t* outPatch)
	{
		if (outMajor) *outMajor = 1;
		if (outMinor) *outMinor = 0;
		if (outPatch) *outPatch = 0;
	}

	D3IV_Result D3IV_CALL D3IV_Create(D3IV_Viewer** outViewer)
	{
		if (!outViewer)
			return D3IV_ERR_INVALID_ARG;

		*outViewer = nullptr;

		try
		{
			ViewerBlock* block = new ViewerBlock();
			block->handle.magic = ViewerHandle::kMagic;
			block->handle.impl = &block->impl;

			*outViewer = reinterpret_cast<D3IV_Viewer*>(&block->handle);

			return D3IV_OK;
		}
		catch (...)
		{
			return D3IV_ERR_FAILED;
		}
	}

	D3IV_Result D3IV_CALL D3IV_Destroy(D3IV_Viewer* viewer)
	{
		ViewerBlock* block = ToBlock(viewer);
		if (!block)
			return D3IV_ERR_INVALID_HANDLE;

		try
		{
			// 콜백을 먼저 끊는다. 소멸 중에 호스트로 올라가지 않게.
			block->impl.SetMouseHandler(nullptr, nullptr);
			block->impl.SetROIEventHandler(nullptr, nullptr);

			block->handle.magic = 0;
			block->handle.impl = nullptr;

			delete block;

			return D3IV_OK;
		}
		catch (...)
		{
			return D3IV_ERR_FAILED;
		}
	}

	D3IV_Result D3IV_CALL D3IV_Initialize(D3IV_Viewer* viewer, void* parentHwnd,
		const D3IV_Rect2i* rect, uint32_t style)
	{
		D3IV_BEGIN(viewer)

		if (!parentHwnd || !rect)
			return D3IV_ERR_INVALID_ARG;

		RECT winRect = {};
		winRect.left = rect->left;
		winRect.top = rect->top;
		winRect.right = rect->right;
		winRect.bottom = rect->bottom;

		const bool ok = self->Initialize(reinterpret_cast<HWND>(parentHwnd),
			winRect, static_cast<DWORD>(style), nullptr);

		return ok ? D3IV_OK : D3IV_ERR_FAILED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetHwnd(D3IV_Viewer* viewer, void** outHwnd)
	{
		D3IV_BEGIN(viewer)

		if (!outHwnd)
			return D3IV_ERR_INVALID_ARG;

		*outHwnd = self->GetHWND();

		return (*outHwnd != nullptr) ? D3IV_OK : D3IV_ERR_NOT_INITIALIZED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_InvalidateFrame(D3IV_Viewer* viewer)
	{
		D3IV_BEGIN(viewer)
		self->InvalidateFrame();
		return D3IV_OK;
		D3IV_END
	}

	/*=====================================================
		이미지
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_UpdateImage(D3IV_Viewer* viewer, const void* data,
		uint32_t width, uint32_t height, uint32_t stride, int32_t pixelFormat)
	{
		D3IV_BEGIN(viewer)

		if (!data)
			return D3IV_ERR_INVALID_ARG;

		uint32_t channel = 0;
		uint32_t bitDepth = 8;

		switch (pixelFormat)
		{
		case D3IV_FMT_GRAY8:  channel = 1; bitDepth = 8;  break;
		case D3IV_FMT_GRAY16: channel = 1; bitDepth = 16; break;
		case D3IV_FMT_BGR8:   channel = 3; bitDepth = 8;  break;
		case D3IV_FMT_BGRA8:  channel = 4; bitDepth = 8;  break;
		default:
			return D3IV_ERR_UNSUPPORTED;
		}

		const bool ok = self->UpdateImage(static_cast<const uint8_t*>(data),
			width, height, stride, channel, bitDepth);

		return ok ? D3IV_OK : D3IV_ERR_UNSUPPORTED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_DetachImage(D3IV_Viewer* viewer)
	{
		D3IV_BEGIN(viewer)
		self->DetachImage();
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetImageSize(D3IV_Viewer* viewer,
		uint32_t* outWidth, uint32_t* outHeight)
	{
		D3IV_BEGIN(viewer)

		if (!outWidth || !outHeight)
			return D3IV_ERR_INVALID_ARG;

		uint32_t width = 0;
		uint32_t height = 0;
		if (!self->GetImageSize(width, height))
			return D3IV_ERR_NOT_FOUND;

		*outWidth = width;
		*outHeight = height;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetImageFormat(D3IV_Viewer* viewer, int32_t* outPixelFormat)
	{
		D3IV_BEGIN(viewer)

		if (!outPixelFormat)
			return D3IV_ERR_INVALID_ARG;

		uint32_t channel = 0;
		uint32_t bitDepth = 0;
		if (!self->GetImageChannelInfo(channel, bitDepth))
			return D3IV_ERR_NOT_FOUND;

		if (channel == 1)
			*outPixelFormat = (bitDepth == 16) ? D3IV_FMT_GRAY16 : D3IV_FMT_GRAY8;
		else if (channel == 3)
			*outPixelFormat = D3IV_FMT_BGR8;
		else if (channel == 4)
			*outPixelFormat = D3IV_FMT_BGRA8;
		else
			return D3IV_ERR_UNSUPPORTED;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetPixelValue(D3IV_Viewer* viewer,
		int32_t imageX, int32_t imageY, double* outValues,
		uint32_t valueCapacity, uint32_t* outChannelCount)
	{
		D3IV_BEGIN(viewer)

		if (!outValues || valueCapacity == 0)
			return D3IV_ERR_INVALID_ARG;

		uint32_t count = 0;
		if (!self->GetPixelValueAt(imageX, imageY, outValues, valueCapacity, count))
			return D3IV_ERR_NOT_FOUND;

		if (outChannelCount)
		{
			*outChannelCount = count;
		}

		return D3IV_OK;

		D3IV_END
	}


	D3IV_Result D3IV_CALL D3IV_ImageOverlayAdd(D3IV_Viewer* viewer, int32_t shapeType,
		const void* items, uint32_t count, const D3IV_OverlayStyle* style,
		D3IV_OverlayHandle* outHandles)
	{
		D3IV_BEGIN(viewer)

		if (!items || count == 0 || !style)
			return D3IV_ERR_INVALID_ARG;

		// 핸들 추적은 아직 구현하지 않았다. 요청되면 명시적으로 알린다.
		if (outHandles)
			return D3IV_ERR_UNSUPPORTED;

		OverlayStyle nativeStyle = {};
		if (!ToOverlayStyle(style, nativeStyle))
			return D3IV_ERR_INVALID_ARG;

		return AddOverlay<true>(self, shapeType, items, count, nativeStyle);

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_WindowOverlayAdd(D3IV_Viewer* viewer, int32_t shapeType,
		const void* items, uint32_t count, const D3IV_OverlayStyle* style,
		D3IV_OverlayHandle* outHandles)
	{
		D3IV_BEGIN(viewer)

		if (!items || count == 0 || !style)
			return D3IV_ERR_INVALID_ARG;

		if (outHandles)
			return D3IV_ERR_UNSUPPORTED;

		OverlayStyle nativeStyle = {};
		if (!ToOverlayStyle(style, nativeStyle))
			return D3IV_ERR_INVALID_ARG;

		return AddOverlay<false>(self, shapeType, items, count, nativeStyle);

		D3IV_END
	}

	// 정점 배열로 폴리곤/폴리라인을 추가한다.
	// Polygon2f 가 힙 소유 타입이라 배열로 받을 수 없어 전용 경로를 둔다.
	D3IV_Result D3IV_CALL D3IV_ImageOverlayAddPolygon(D3IV_Viewer* viewer,
		const D3IV_Point2f* vertices, uint32_t vertexCount,
		int32_t isClosed, const D3IV_OverlayStyle* style)
	{
		D3IV_BEGIN(viewer)

		if (!vertices || vertexCount < 2 || !style)
			return D3IV_ERR_INVALID_ARG;

		OverlayStyle nativeStyle = {};
		if (!ToOverlayStyle(style, nativeStyle))
			return D3IV_ERR_INVALID_ARG;

		if (isClosed != 0)
		{
			Polygon2f polygon;
			if (!polygon.Reserve(vertexCount))
				return D3IV_ERR_FAILED;

			for (uint32_t i = 0; i < vertexCount; ++i)
			{
				polygon.AddVertex(Point2f{ vertices[i].x, vertices[i].y });
			}

			self->ImageOverlayAdd(&polygon, 1, nativeStyle);
		}
		else
		{
			Polyline2f polyline;
			if (!polyline.Reserve(vertexCount))
				return D3IV_ERR_FAILED;

			for (uint32_t i = 0; i < vertexCount; ++i)
			{
				polyline.AddVertex(Point2f{ vertices[i].x, vertices[i].y });
			}

			self->ImageOverlayAdd(&polyline, 1, nativeStyle);
		}

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ImageOverlayClear(D3IV_Viewer* viewer)
	{
		D3IV_BEGIN(viewer)
		self->ImageOverlayClear();
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ImageOverlayShow(D3IV_Viewer* viewer, int32_t show)
	{
		D3IV_BEGIN(viewer)
		self->ImageOverlayShow(show != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_WindowOverlayClear(D3IV_Viewer* viewer)
	{
		D3IV_BEGIN(viewer)
		self->WindowOverlayClear();
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_WindowOverlayShow(D3IV_Viewer* viewer, int32_t show)
	{
		D3IV_BEGIN(viewer)
		self->WindowOverlayShow(show != 0);
		return D3IV_OK;
		D3IV_END
	}

	/*=====================================================
		ROI — 설정
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_ROISetRect(D3IV_Viewer* viewer,
		const wchar_t* key, const wchar_t* name, const D3IV_Rect2f* rect,
		uint32_t colorRGB, int32_t isMovable, int32_t isResizable, int32_t fontSize)
	{
		D3IV_BEGIN(viewer)

		if (!key || !rect)
			return D3IV_ERR_INVALID_ARG;

		const Rect2f nativeRect{ rect->left, rect->top, rect->right, rect->bottom };

		const bool ok = self->ROISet(key, name ? name : L"", nativeRect,
			static_cast<COLORREF>(colorRGB),
			isMovable != 0, isResizable != 0, fontSize);

		return ok ? D3IV_OK : D3IV_ERR_FAILED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROISetEllipse(D3IV_Viewer* viewer,
		const wchar_t* key, const wchar_t* name, const D3IV_Ellipse2f* ellipse,
		uint32_t colorRGB, int32_t isMovable, int32_t isResizable, int32_t fontSize)
	{
		D3IV_BEGIN(viewer)

		if (!key || !ellipse)
			return D3IV_ERR_INVALID_ARG;

		const Ellipse2f nativeEllipse(ellipse->cx, ellipse->cy,
			ellipse->rx, ellipse->ry, ellipse->angleRad);

		const bool ok = self->ROISet(key, name ? name : L"", nativeEllipse,
			static_cast<COLORREF>(colorRGB),
			isMovable != 0, isResizable != 0, fontSize);

		return ok ? D3IV_OK : D3IV_ERR_FAILED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROISetCircle(D3IV_Viewer* viewer,
		const wchar_t* key, const wchar_t* name, const D3IV_Circle2f* circle,
		uint32_t colorRGB, int32_t isMovable, int32_t isResizable, int32_t fontSize)
	{
		D3IV_BEGIN(viewer)

		if (!key || !circle)
			return D3IV_ERR_INVALID_ARG;

		const Circle2f nativeCircle(circle->cx, circle->cy, circle->radius);

		const bool ok = self->ROISet(key, name ? name : L"", nativeCircle,
			static_cast<COLORREF>(colorRGB),
			isMovable != 0, isResizable != 0, fontSize);

		return ok ? D3IV_OK : D3IV_ERR_FAILED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROISetPolygon(D3IV_Viewer* viewer,
		const wchar_t* key, const wchar_t* name,
		const D3IV_Point2f* vertices, uint32_t vertexCount,
		uint32_t colorRGB, int32_t isMovable, int32_t isResizable, int32_t fontSize)
	{
		D3IV_BEGIN(viewer)

		if (!key || !vertices || vertexCount < 3)
			return D3IV_ERR_INVALID_ARG;

		Polygon2f polygon;
		if (!polygon.Reserve(vertexCount))
			return D3IV_ERR_FAILED;

		for (uint32_t i = 0; i < vertexCount; ++i)
		{
			polygon.AddVertex(Point2f{ vertices[i].x, vertices[i].y });
		}

		const bool ok = self->ROISet(key, name ? name : L"", polygon,
			static_cast<COLORREF>(colorRGB),
			isMovable != 0, isResizable != 0, fontSize);

		return ok ? D3IV_OK : D3IV_ERR_FAILED;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIClear(D3IV_Viewer* viewer)
	{
		D3IV_BEGIN(viewer)
		self->ROIClear();
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIRemove(D3IV_Viewer* viewer, const wchar_t* key)
	{
		D3IV_BEGIN(viewer)

		if (!key)
			return D3IV_ERR_INVALID_ARG;

		return self->ROIRemove(key) ? D3IV_OK : D3IV_ERR_NOT_FOUND;

		D3IV_END
	}

	/*=====================================================
		ROI — 조회
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_ROIGetShape(D3IV_Viewer* viewer,
		const wchar_t* key, D3IV_ROIShape* outShape)
	{
		D3IV_BEGIN(viewer)

		if (!key || !outShape)
			return D3IV_ERR_INVALID_ARG;

		ROIShapeData shape = {};
		if (!self->ROIGetShape(key, shape))
			return D3IV_ERR_NOT_FOUND;

		const uint32_t structSize = outShape->structSize;

		*outShape = {};
		outShape->structSize = (structSize != 0) ? structSize : sizeof(D3IV_ROIShape);
		outShape->type = ToCShapeType(shape.type);
		outShape->vertexCount = shape.vertexCount;

		switch (shape.type)
		{
		case ROIObjectType::Rectangle:
			outShape->u.rect.left = shape.u.rect.left;
			outShape->u.rect.top = shape.u.rect.top;
			outShape->u.rect.right = shape.u.rect.right;
			outShape->u.rect.bottom = shape.u.rect.bottom;
			break;

		case ROIObjectType::Ellipse:
			outShape->u.ellipse.cx = shape.u.ellipse.cx;
			outShape->u.ellipse.cy = shape.u.ellipse.cy;
			outShape->u.ellipse.rx = shape.u.ellipse.rx;
			outShape->u.ellipse.ry = shape.u.ellipse.ry;
			outShape->u.ellipse.angleRad = shape.u.ellipse.angleRad;
			break;

		case ROIObjectType::Circle:
			outShape->u.circle.cx = shape.u.circle.cx;
			outShape->u.circle.cy = shape.u.circle.cy;
			outShape->u.circle.radius = shape.u.circle.radius;
			break;

		case ROIObjectType::Polygon:
		default:
			// 좌표는 D3IV_ROIGetVertices 로 받는다.
			break;
		}

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetVertices(D3IV_Viewer* viewer,
		const wchar_t* key, D3IV_Point2f* buffer, uint32_t bufferCount,
		uint32_t* outCount, uint32_t segmentsPerCurve)
	{
		D3IV_BEGIN(viewer)

		if (!key)
			return D3IV_ERR_INVALID_ARG;

		// D3IV_Point2f 와 Core 의 Point2f 는 레이아웃이 같다(float x, y).
		// 그래도 형변환을 명시해 의도를 남긴다.
		Point2f* native = reinterpret_cast<Point2f*>(buffer);

		const uint32_t needed = self->ROIGetVertices(key, native,
			buffer ? bufferCount : 0u, segmentsPerCurve);

		return FinishSizedCall(needed, bufferCount, outCount);

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetInfo(D3IV_Viewer* viewer,
		const wchar_t* key, D3IV_ROIInfo* outInfo)
	{
		D3IV_BEGIN(viewer)

		if (!key || !outInfo)
			return D3IV_ERR_INVALID_ARG;

		ROIRenderLayer::ROIInfoData info = {};
		if (!self->ROIGetInfo(key, info))
			return D3IV_ERR_NOT_FOUND;

		const uint32_t structSize = outInfo->structSize;

		*outInfo = {};
		outInfo->structSize = (structSize != 0) ? structSize : sizeof(D3IV_ROIInfo);
		outInfo->type = ToCShapeType(info.type);
		outInfo->colorRGB = info.colorRGB;
		outInfo->isMovable = info.isMovable ? 1 : 0;
		outInfo->isResizable = info.isResizable ? 1 : 0;
		outInfo->isSelected = info.isSelected ? 1 : 0;
		outInfo->isHovered = info.isHovered ? 1 : 0;
		outInfo->fontSize = info.fontSize;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetName(D3IV_Viewer* viewer, const wchar_t* key,
		wchar_t* buffer, uint32_t bufferChars, uint32_t* outChars)
	{
		D3IV_BEGIN(viewer)

		if (!key)
			return D3IV_ERR_INVALID_ARG;

		const uint32_t needed = self->ROIGetName(key, buffer, buffer ? bufferChars : 0u);

		return FinishSizedCall(needed, bufferChars, outChars);

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetBounds(D3IV_Viewer* viewer,
		const wchar_t* key, D3IV_Rect2f* outBounds)
	{
		D3IV_BEGIN(viewer)

		if (!key || !outBounds)
			return D3IV_ERR_INVALID_ARG;

		Rect2f bounds = {};
		if (!self->ROIGetBounds(key, bounds))
			return D3IV_ERR_NOT_FOUND;

		outBounds->left = bounds.left;
		outBounds->top = bounds.top;
		outBounds->right = bounds.right;
		outBounds->bottom = bounds.bottom;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetCount(D3IV_Viewer* viewer, uint32_t* outCount)
	{
		D3IV_BEGIN(viewer)

		if (!outCount)
			return D3IV_ERR_INVALID_ARG;

		*outCount = self->ROIGetCount();

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetKeys(D3IV_Viewer* viewer, wchar_t* buffer,
		uint32_t charsPerKey, uint32_t bufferCount, uint32_t* outCount)
	{
		D3IV_BEGIN(viewer)

		const uint32_t total = self->ROIGetCount();

		if (outCount)
		{
			*outCount = total;
		}

		if (total == 0)
			return D3IV_OK;

		if (!buffer || charsPerKey == 0 || bufferCount < total)
			return D3IV_ERR_BUFFER_TOO_SMALL;

		// buffer 를 [bufferCount][charsPerKey] 로 취급한다.
		bool truncated = false;

		for (uint32_t i = 0; i < total; ++i)
		{
			wchar_t* slot = buffer + static_cast<size_t>(i) * charsPerKey;

			const uint32_t needed = self->ROIGetKeyAt(i, slot, charsPerKey);
			if (needed > charsPerKey)
			{
				// 키가 슬롯보다 길다. 빈 문자열로 두고 알린다.
				slot[0] = L'\0';
				truncated = true;
			}
		}

		return truncated ? D3IV_ERR_BUFFER_TOO_SMALL : D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIGetSelectedKey(D3IV_Viewer* viewer,
		wchar_t* buffer, uint32_t bufferChars, uint32_t* outChars)
	{
		D3IV_BEGIN(viewer)

		const uint32_t needed = self->ROIGetSelectedKey(buffer, buffer ? bufferChars : 0u);

		return FinishSizedCall(needed, bufferChars, outChars);

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ROIHitTest(D3IV_Viewer* viewer,
		float imageX, float imageY, float tolerance,
		wchar_t* keyBuffer, uint32_t bufferChars, uint32_t* outChars)
	{
		D3IV_BEGIN(viewer)

		const uint32_t needed = self->ROIHitTestKey(imageX, imageY, tolerance,
			keyBuffer, keyBuffer ? bufferChars : 0u);

		return FinishSizedCall(needed, bufferChars, outChars);

		D3IV_END
	}

	/*=====================================================
		콜백
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_SetMouseCallback(D3IV_Viewer* viewer,
		D3IV_MouseCallback callback, void* userData)
	{
		ViewerBlock* block = ToBlock(viewer);
		if (!block)
			return D3IV_ERR_INVALID_HANDLE;

		try
		{
			block->callbacks.mouseCallback = callback;
			block->callbacks.mouseUserData = userData;

			block->impl.SetMouseHandler(
				callback ? &MouseTrampoline : nullptr,
				callback ? &block->callbacks : nullptr);

			return D3IV_OK;
		}
		catch (...)
		{
			return D3IV_ERR_FAILED;
		}
	}

	D3IV_Result D3IV_CALL D3IV_SetROIEventCallback(D3IV_Viewer* viewer,
		D3IV_ROIEventCallback callback, void* userData)
	{
		ViewerBlock* block = ToBlock(viewer);
		if (!block)
			return D3IV_ERR_INVALID_HANDLE;

		try
		{
			block->callbacks.roiCallback = callback;
			block->callbacks.roiUserData = userData;

			block->impl.SetROIEventHandler(
				callback ? &ROITrampoline : nullptr,
				callback ? &block->callbacks : nullptr);

			return D3IV_OK;
		}
		catch (...)
		{
			return D3IV_ERR_FAILED;
		}
	}

	/*=====================================================
		뷰 제어 / 좌표 변환
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_SetZoom(D3IV_Viewer* viewer, float zoom, int32_t animate)
	{
		D3IV_BEGIN(viewer)

		if (zoom <= 0.0f)
			return D3IV_ERR_INVALID_ARG;

		self->SetZoomLevel(zoom, animate != 0);
		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetZoom(D3IV_Viewer* viewer, float* outZoom)
	{
		D3IV_BEGIN(viewer)

		if (!outZoom)
			return D3IV_ERR_INVALID_ARG;

		*outZoom = self->GetZoomLevel();
		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ZoomFit(D3IV_Viewer* viewer, int32_t animate)
	{
		D3IV_BEGIN(viewer)
		self->ZoomFitProgrammatic(animate != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_Zoom1To1(D3IV_Viewer* viewer, int32_t animate)
	{
		D3IV_BEGIN(viewer)
		self->Zoom1To1Programmatic(animate != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_SetCenter(D3IV_Viewer* viewer,
		float imageX, float imageY, int32_t animate)
	{
		D3IV_BEGIN(viewer)
		self->SetViewCenter(imageX, imageY, animate != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetCenter(D3IV_Viewer* viewer, float* outX, float* outY)
	{
		D3IV_BEGIN(viewer)

		if (!outX || !outY)
			return D3IV_ERR_INVALID_ARG;

		self->GetViewCenter(*outX, *outY);
		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ZoomToRect(D3IV_Viewer* viewer,
		const D3IV_Rect2f* imageRect, float marginRatio, int32_t animate)
	{
		D3IV_BEGIN(viewer)

		if (!imageRect)
			return D3IV_ERR_INVALID_ARG;

		const Rect2f rect{ imageRect->left, imageRect->top,
						   imageRect->right, imageRect->bottom };

		self->ZoomToRect(rect, marginRatio, animate != 0);
		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_GetVisibleImageRect(D3IV_Viewer* viewer, D3IV_Rect2f* outRect)
	{
		D3IV_BEGIN(viewer)

		if (!outRect)
			return D3IV_ERR_INVALID_ARG;

		Rect2f rect = {};
		if (!self->GetVisibleImageRect(rect))
			return D3IV_ERR_NOT_INITIALIZED;

		outRect->left = rect.left;
		outRect->top = rect.top;
		outRect->right = rect.right;
		outRect->bottom = rect.bottom;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ScreenToImage(D3IV_Viewer* viewer,
		int32_t screenX, int32_t screenY, float* outImageX, float* outImageY)
	{
		D3IV_BEGIN(viewer)

		if (!outImageX || !outImageY)
			return D3IV_ERR_INVALID_ARG;

		float imageX = 0.0f;
		float imageY = 0.0f;
		if (!self->ScreenToImage(screenX, screenY, imageX, imageY))
			return D3IV_ERR_NOT_FOUND;   // 이미지 밖

		*outImageX = imageX;
		*outImageY = imageY;

		return D3IV_OK;

		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_ImageToScreen(D3IV_Viewer* viewer,
		float imageX, float imageY, int32_t* outScreenX, int32_t* outScreenY)
	{
		D3IV_BEGIN(viewer)

		if (!outScreenX || !outScreenY)
			return D3IV_ERR_INVALID_ARG;

		int32_t screenX = 0;
		int32_t screenY = 0;
		if (!self->ImageToScreen(imageX, imageY, screenX, screenY))
			return D3IV_ERR_NOT_INITIALIZED;

		*outScreenX = screenX;
		*outScreenY = screenY;

		return D3IV_OK;

		D3IV_END
	}

	/*=====================================================
		표시 옵션
	=====================================================*/
	D3IV_Result D3IV_CALL D3IV_SetToolbarVisible(D3IV_Viewer* viewer, int32_t visible)
	{
		D3IV_BEGIN(viewer)
		self->SetToolbarVisible(visible != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_SetStatusBarVisible(D3IV_Viewer* viewer, int32_t visible)
	{
		D3IV_BEGIN(viewer)
		self->SetStatusBarVisible(visible != 0);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_SetBackgroundColor(D3IV_Viewer* viewer, uint32_t colorRGB)
	{
		D3IV_BEGIN(viewer)
		self->SetBackgroundColor(colorRGB);
		return D3IV_OK;
		D3IV_END
	}

	D3IV_Result D3IV_CALL D3IV_SetVSyncEnabled(D3IV_Viewer* viewer, int32_t enable)
	{
		D3IV_BEGIN(viewer)
		self->SetVSyncEnabled(enable != 0);
		return D3IV_OK;
		D3IV_END
	}

} // extern "C"
