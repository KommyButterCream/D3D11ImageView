// D3D11ImageView — flat C ABI
//
// C++ 클래스(D3D11ImageView.h)는 맹글링된 심볼과 C++ 타입을 쓰므로 P/Invoke 로
// 호출할 수 없다. 이 헤더는 같은 DLL 안의 얇은 위임 계층이며 로직이 없다.
// C++ 소비자는 기존 헤더를 계속 쓰면 된다.
//
// 규칙
//   - 소유권을 경계 밖으로 넘기지 않는다. 출력은 호출자 버퍼 + 2회 호출.
//   - STL / 소유 타입(Polygon2f, std::wstring)을 시그니처에 쓰지 않는다.
//   - bool 대신 int32_t. C++ bool 크기는 처리계 정의이고 C# 마샬링과 어긋난다.
//   - enum 은 int32_t 로 주고받는다.
//   - 예외를 경계 밖으로 내보내지 않는다. 모든 함수가 결과 코드를 반환한다.
//   - 구조체는 첫 멤버가 structSize 다. 필드 추가 시 구버전 호출자가 깨지지 않는다.
//
// 2회 호출 패턴
//   1회차: buffer = nullptr, bufferCount = 0  -> 필요 개수를 out*Count 에 반환
//   2회차: 버퍼를 확보해 재호출
//   버퍼가 부족하면 D3IV_ERR_BUFFER_TOO_SMALL 과 함께 필요 개수를 채운다.

#pragma once

#ifdef BUILD_D3D11_IMAGE_VIEW_DLL
#define D3IV_API __declspec(dllexport)
#else
#define D3IV_API __declspec(dllimport)
#endif

// x64 에는 호출 규약이 하나뿐이라 무의미하지만, x86 빌드가 생길 여지를 위해
// 명시한다. C# 쪽은 CallingConvention.StdCall 로 선언한다.
#define D3IV_CALL __stdcall

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    // ─────────────────────────────────────────────────────────────────────
    // 결과 코드
    // ─────────────────────────────────────────────────────────────────────
    typedef enum D3IV_Result
    {
        D3IV_OK = 0,
        D3IV_ERR_INVALID_ARG,        // 인자가 null 이거나 범위 밖
        D3IV_ERR_INVALID_HANDLE,     // viewer 핸들이 유효하지 않음
        D3IV_ERR_NOT_FOUND,          // 해당 key / handle 의 대상이 없음
        D3IV_ERR_BUFFER_TOO_SMALL,   // out*Count 에 필요 개수가 채워짐
        D3IV_ERR_UNSUPPORTED,        // 포맷/조합을 지원하지 않음
        D3IV_ERR_NOT_INITIALIZED,    // Initialize 전에 호출
        D3IV_ERR_FAILED              // 그 외 (내부 예외 포함)
    } D3IV_Result;

    // ─────────────────────────────────────────────────────────────────────
    // 핸들 / 버전
    // ─────────────────────────────────────────────────────────────────────
    typedef struct D3IV_Viewer_ D3IV_Viewer;

    // major/minor/patch. 호스트가 헤더와 DLL 버전을 맞추는 데 쓴다.
    D3IV_API void D3IV_CALL D3IV_GetVersion(int32_t* outMajor,
                                            int32_t* outMinor,
                                            int32_t* outPatch);

    // ─────────────────────────────────────────────────────────────────────
    // POD 타입
    //
    // Core 의 Point2f 등은 생성자를 가져 C 구조체가 아니다. C 전용 미러를 둔다.
    // C# 은 [StructLayout(LayoutKind.Sequential)] 로 그대로 선언하면 된다.
    // ─────────────────────────────────────────────────────────────────────
    typedef struct D3IV_Point2i { int32_t x, y; }                        D3IV_Point2i;
    typedef struct D3IV_Point2f { float   x, y; }                        D3IV_Point2f;
    typedef struct D3IV_Point2d { double  x, y; }                        D3IV_Point2d;

    typedef struct D3IV_Rect2i { int32_t left, top, right, bottom; }     D3IV_Rect2i;
    typedef struct D3IV_Rect2f { float   left, top, right, bottom; }     D3IV_Rect2f;
    typedef struct D3IV_Rect2d { double  left, top, right, bottom; }     D3IV_Rect2d;

    typedef struct D3IV_Line2f { float x1, y1, x2, y2; }                 D3IV_Line2f;

    typedef struct D3IV_Circle2f { float cx, cy, radius; }               D3IV_Circle2f;
    typedef struct D3IV_Ellipse2f { float cx, cy, rx, ry, angleRad; }    D3IV_Ellipse2f;

    // ─────────────────────────────────────────────────────────────────────
    // 생성 / 초기화
    // ─────────────────────────────────────────────────────────────────────
    D3IV_API D3IV_Result D3IV_CALL D3IV_Create(D3IV_Viewer** outViewer);
    D3IV_API D3IV_Result D3IV_CALL D3IV_Destroy(D3IV_Viewer* viewer);

    // parentHwnd 아래에 자식 창을 만든다. style 은 보통 WS_CHILD | WS_VISIBLE.
    // rect 는 부모 클라이언트 좌표, 픽셀 단위(DIP 아님).
    D3IV_API D3IV_Result D3IV_CALL D3IV_Initialize(D3IV_Viewer* viewer,
                                                   void* parentHwnd,
                                                   const D3IV_Rect2i* rect,
                                                   uint32_t style);

    D3IV_API D3IV_Result D3IV_CALL D3IV_GetHwnd(D3IV_Viewer* viewer,
                                                 void** outHwnd);

    D3IV_API D3IV_Result D3IV_CALL D3IV_InvalidateFrame(D3IV_Viewer* viewer);

    // ─────────────────────────────────────────────────────────────────────
    // 이미지 입력
    // ─────────────────────────────────────────────────────────────────────
    typedef enum D3IV_PixelFormat
    {
        D3IV_FMT_GRAY8 = 0,   // 1ch  8bit
        D3IV_FMT_GRAY16,      // 1ch 16bit (부호 없음)
        D3IV_FMT_BGR8,        // 3ch  8bit — 내부에서 BGRA 로 확장
        D3IV_FMT_BGRA8,       // 4ch  8bit
    } D3IV_PixelFormat;

    // stride 는 바이트 단위. width * channel * (bitDepth/8) 이상이어야 한다.
    //
    // ★ 수명 계약: 뷰어는 이 포인터를 복사하지 않고 빌려 쓴다. 렌더 스레드가
    //   비동기로 읽으므로, 버퍼를 해제/재할당하기 전에 반드시 D3IV_DetachImage
    //   를 호출해야 한다. C# 이라면 GCHandle.Alloc(..., Pinned) 로 고정해야 한다.
    D3IV_API D3IV_Result D3IV_CALL D3IV_UpdateImage(D3IV_Viewer* viewer,
                                                     const void* data,
                                                     uint32_t width,
                                                     uint32_t height,
                                                     uint32_t stride,
                                                     int32_t pixelFormat);

    D3IV_API D3IV_Result D3IV_CALL D3IV_DetachImage(D3IV_Viewer* viewer);

    D3IV_API D3IV_Result D3IV_CALL D3IV_GetImageSize(D3IV_Viewer* viewer,
                                                      uint32_t* outWidth,
                                                      uint32_t* outHeight);

    D3IV_API D3IV_Result D3IV_CALL D3IV_GetImageFormat(D3IV_Viewer* viewer,
                                                        int32_t* outPixelFormat);

    // 이미지 좌표의 픽셀값. 채널 수만큼 outValues 에 채운다.
    D3IV_API D3IV_Result D3IV_CALL D3IV_GetPixelValue(D3IV_Viewer* viewer,
                                                       int32_t imageX, int32_t imageY,
                                                       double* outValues,
                                                       uint32_t valueCapacity,
                                                       uint32_t* outChannelCount);

    // ─────────────────────────────────────────────────────────────────────
    // 오버레이
    //
    // C++ 쪽 오버로드 21종 × 2(image/window) = 42개를 그대로 내보내면 C# 이
    // 지옥이 된다. 도형 타입 열거형으로 합친다.
    // items 는 해당 타입의 배열이고 count 개를 읽는다.
    // ─────────────────────────────────────────────────────────────────────
    typedef enum D3IV_ShapeType
    {
        D3IV_SHAPE_POINT2I = 0, D3IV_SHAPE_POINT2F, D3IV_SHAPE_POINT2D,
        D3IV_SHAPE_LINE2I,      D3IV_SHAPE_LINE2F,  D3IV_SHAPE_LINE2D,
        D3IV_SHAPE_RECT2I,      D3IV_SHAPE_RECT2F,  D3IV_SHAPE_RECT2D,
        D3IV_SHAPE_QUADRECT2I,  D3IV_SHAPE_QUADRECT2F, D3IV_SHAPE_QUADRECT2D,
        D3IV_SHAPE_ROTRECT2I,   D3IV_SHAPE_ROTRECT2F,  D3IV_SHAPE_ROTRECT2D,
        D3IV_SHAPE_CIRCLE2F,    D3IV_SHAPE_CIRCLE2D,
        D3IV_SHAPE_ELLIPSE2F,   D3IV_SHAPE_ELLIPSE2D,
        D3IV_SHAPE_POLYLINE2F,  D3IV_SHAPE_POLYGON2F,
    } D3IV_ShapeType;

    typedef struct D3IV_ColorRGBA8 { uint8_t r, g, b, a; } D3IV_ColorRGBA8;

    typedef struct D3IV_OverlayStyle
    {
        uint32_t        structSize;
        D3IV_ColorRGBA8 fillColor;
        D3IV_ColorRGBA8 strokeColor;
        float           strokeWidth;
        int32_t         transparentFill;   // 0 / 1
    } D3IV_OverlayStyle;

    typedef uint64_t D3IV_OverlayHandle;   // 0 = 무효

    // outHandles 가 nullptr 이면 핸들을 만들지 않는다.
    // 대량 추가(10만 개) 시 부기를 피하려면 nullptr 을 넘긴다.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayAdd(D3IV_Viewer* viewer,
                                                         int32_t shapeType,
                                                         const void* items,
                                                         uint32_t count,
                                                         const D3IV_OverlayStyle* style,
                                                         D3IV_OverlayHandle* outHandles);

    D3IV_API D3IV_Result D3IV_CALL D3IV_WindowOverlayAdd(D3IV_Viewer* viewer,
                                                          int32_t shapeType,
                                                          const void* items,
                                                          uint32_t count,
                                                          const D3IV_OverlayStyle* style,
                                                          D3IV_OverlayHandle* outHandles);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayClear(D3IV_Viewer* viewer);
    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayShow(D3IV_Viewer* viewer, int32_t show);
    D3IV_API D3IV_Result D3IV_CALL D3IV_WindowOverlayClear(D3IV_Viewer* viewer);
    D3IV_API D3IV_Result D3IV_CALL D3IV_WindowOverlayShow(D3IV_Viewer* viewer, int32_t show);

    // 결함 마커 클릭 -> 어느 마커인가.
    // 히트가 없으면 D3IV_ERR_NOT_FOUND 와 outHandle = 0.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayHitTest(D3IV_Viewer* viewer,
                                                             float imageX, float imageY,
                                                             float tolerance,
                                                             D3IV_OverlayHandle* outHandle);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayGetBounds(D3IV_Viewer* viewer,
                                                               D3IV_OverlayHandle handle,
                                                               D3IV_Rect2f* outBounds);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageOverlayRemove(D3IV_Viewer* viewer,
                                                            D3IV_OverlayHandle handle);

    // ─────────────────────────────────────────────────────────────────────
    // ROI — 설정
    // ─────────────────────────────────────────────────────────────────────
    typedef enum D3IV_ROIShapeType
    {
        D3IV_ROI_RECTANGLE = 0,
        D3IV_ROI_ELLIPSE,
        D3IV_ROI_CIRCLE,
        D3IV_ROI_POLYGON,
    } D3IV_ROIShapeType;

    // colorRGB 는 COLORREF 와 같은 0x00BBGGRR 배치.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROISetRect(D3IV_Viewer* viewer,
                                                    const wchar_t* key,
                                                    const wchar_t* name,
                                                    const D3IV_Rect2f* rect,
                                                    uint32_t colorRGB,
                                                    int32_t isMovable,
                                                    int32_t isResizable,
                                                    int32_t fontSize);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROISetEllipse(D3IV_Viewer* viewer,
                                                       const wchar_t* key,
                                                       const wchar_t* name,
                                                       const D3IV_Ellipse2f* ellipse,
                                                       uint32_t colorRGB,
                                                       int32_t isMovable,
                                                       int32_t isResizable,
                                                       int32_t fontSize);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROISetCircle(D3IV_Viewer* viewer,
                                                      const wchar_t* key,
                                                      const wchar_t* name,
                                                      const D3IV_Circle2f* circle,
                                                      uint32_t colorRGB,
                                                      int32_t isMovable,
                                                      int32_t isResizable,
                                                      int32_t fontSize);

    // vertices 는 이미지 좌표. 3점 이상.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROISetPolygon(D3IV_Viewer* viewer,
                                                       const wchar_t* key,
                                                       const wchar_t* name,
                                                       const D3IV_Point2f* vertices,
                                                       uint32_t vertexCount,
                                                       uint32_t colorRGB,
                                                       int32_t isMovable,
                                                       int32_t isResizable,
                                                       int32_t fontSize);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIClear(D3IV_Viewer* viewer);
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIRemove(D3IV_Viewer* viewer, const wchar_t* key);

    // ─────────────────────────────────────────────────────────────────────
    // ROI — 조회
    //
    // 해석적 형태와 정점 배열을 별도 메서드로 나눈다.
    //   해석적 : 계측용. 원의 면적은 pi*r^2 이지 다각형 근사의 면적이 아니다.
    //   정점   : 마스킹/픽셀 순회용. 곡선은 근사된다.
    // 정점만 제공하면 원을 받아 다시 Set 할 때 다각형이 되어 편집 핸들과 계측
    // 정확도를 잃는다. 무손실 왕복을 위해 해석적 형태가 1차다.
    // ─────────────────────────────────────────────────────────────────────
    typedef struct D3IV_ROIShape
    {
        uint32_t structSize;
        int32_t  type;          // D3IV_ROIShapeType
        uint32_t vertexCount;   // POLYGON 일 때만 유효

        union
        {
            struct { float left, top, right, bottom; } rect;
            struct { float cx, cy, rx, ry, angleRad; } ellipse;
            struct { float cx, cy, radius; }           circle;
        } u;
    } D3IV_ROIShape;

    // 해석적 형태
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetShape(D3IV_Viewer* viewer,
                                                     const wchar_t* key,
                                                     D3IV_ROIShape* outShape);

    // 정점 배열 (이미지 좌표)
    //   Rectangle -> 4점, Polygon -> 원본 정점
    //   Circle / Ellipse -> segmentsPerCurve 개로 근사 (0 이면 기본 64)
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetVertices(D3IV_Viewer* viewer,
                                                        const wchar_t* key,
                                                        D3IV_Point2f* buffer,
                                                        uint32_t bufferCount,
                                                        uint32_t* outCount,
                                                        uint32_t segmentsPerCurve);

    typedef struct D3IV_ROIInfo
    {
        uint32_t structSize;
        int32_t  type;            // D3IV_ROIShapeType
        uint32_t colorRGB;
        int32_t  isMovable;
        int32_t  isResizable;
        int32_t  isSelected;
        int32_t  isHovered;
        int32_t  fontSize;
    } D3IV_ROIInfo;

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetInfo(D3IV_Viewer* viewer,
                                                    const wchar_t* key,
                                                    D3IV_ROIInfo* outInfo);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetName(D3IV_Viewer* viewer,
                                                    const wchar_t* key,
                                                    wchar_t* buffer,
                                                    uint32_t bufferChars,
                                                    uint32_t* outChars);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetBounds(D3IV_Viewer* viewer,
                                                      const wchar_t* key,
                                                      D3IV_Rect2f* outBounds);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetCount(D3IV_Viewer* viewer,
                                                     uint32_t* outCount);

    // 키 목록. buffer 를 [bufferCount][charsPerKey] 로 취급한다.
    // 키가 charsPerKey 보다 길면 잘리고 D3IV_ERR_BUFFER_TOO_SMALL 을 반환한다.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetKeys(D3IV_Viewer* viewer,
                                                    wchar_t* buffer,
                                                    uint32_t charsPerKey,
                                                    uint32_t bufferCount,
                                                    uint32_t* outCount);

    // 현재 선택된 ROI. 없으면 D3IV_ERR_NOT_FOUND.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIGetSelectedKey(D3IV_Viewer* viewer,
                                                           wchar_t* buffer,
                                                           uint32_t bufferChars,
                                                           uint32_t* outChars);

    // 임의 좌표 히트 테스트. 없으면 D3IV_ERR_NOT_FOUND.
    D3IV_API D3IV_Result D3IV_CALL D3IV_ROIHitTest(D3IV_Viewer* viewer,
                                                    float imageX, float imageY,
                                                    float tolerance,
                                                    wchar_t* keyBuffer,
                                                    uint32_t bufferChars,
                                                    uint32_t* outChars);

    // ─────────────────────────────────────────────────────────────────────
    // 콜백
    //
    // 전부 UI 스레드(WndProc)에서 호출된다. C# WinForms/WPF 핸들러에서 컨트롤을
    // 직접 만져도 된다.
    //
    // ★ C# 주의: 델리게이트를 필드로 잡아두지 않으면 GC 가 수거해서 다음 콜백에
    //   프로세스가 죽는다. 래퍼 클래스가 반드시 참조를 유지해야 한다.
    //
    // ★ 뷰어는 콜백을 ROI 락 밖에서 호출한다. 콜백 안에서 다른 D3IV_* 함수를
    //   불러도 데드락이 없다.
    // ─────────────────────────────────────────────────────────────────────
    typedef enum D3IV_MouseEventType
    {
        D3IV_MOUSE_MOVE = 0,
        D3IV_MOUSE_LBUTTON_DOWN,
        D3IV_MOUSE_LBUTTON_UP,
        D3IV_MOUSE_LBUTTON_DBLCLK,
        D3IV_MOUSE_RBUTTON_DOWN,
        D3IV_MOUSE_RBUTTON_UP,
        D3IV_MOUSE_MBUTTON_DOWN,
        D3IV_MOUSE_MBUTTON_UP,
        D3IV_MOUSE_WHEEL,
        D3IV_MOUSE_LEAVE,
    } D3IV_MouseEventType;

    // 비트 플래그
    enum
    {
        D3IV_MOD_CTRL  = 0x0001,
        D3IV_MOD_SHIFT = 0x0002,
        D3IV_MOD_ALT   = 0x0004,

        D3IV_BTN_LEFT   = 0x0001,
        D3IV_BTN_RIGHT  = 0x0002,
        D3IV_BTN_MIDDLE = 0x0004,
    };

    typedef struct D3IV_MouseEvent
    {
        uint32_t structSize;
        int32_t  type;            // D3IV_MouseEventType
        int32_t  screenX, screenY; // 뷰어 클라이언트 좌표, 픽셀
        float    imageX, imageY;   // 이미지 좌표
        int32_t  isInsideImage;    // 0 이면 imageX/Y 는 무의미
        int32_t  wheelDelta;       // WHEEL 일 때만
        int32_t  modifiers;        // D3IV_MOD_*
        int32_t  buttons;          // D3IV_BTN_*
    } D3IV_MouseEvent;

    // 반환값: 0 = 뷰어가 계속 처리, 1 = 호스트가 처리 완료 -> 뷰어는 무시
    //
    // 호출 시점은 UI 처리(툴바/컨텍스트메뉴) 다음, ROI 편집 앞이다.
    // 즉 툴바 클릭은 전달되지 않고, 호스트가 ROI 편집/팬/선택을 선점할 수 있다.
    typedef int32_t (D3IV_CALL* D3IV_MouseCallback)(const D3IV_MouseEvent* e,
                                                    void* userData);

    D3IV_API D3IV_Result D3IV_CALL D3IV_SetMouseCallback(D3IV_Viewer* viewer,
                                                          D3IV_MouseCallback callback,
                                                          void* userData);

    typedef enum D3IV_ROIEventType
    {
        D3IV_ROI_SELECTED = 0,
        D3IV_ROI_DESELECTED,
        D3IV_ROI_EDIT_BEGIN,
        D3IV_ROI_EDIT_CHANGED,
        D3IV_ROI_EDIT_END,      // 결과 확정은 보통 여기서
        D3IV_ROI_DOUBLECLICKED,
    } D3IV_ROIEventType;

    typedef void (D3IV_CALL* D3IV_ROIEventCallback)(int32_t eventType,
                                                    const wchar_t* key,
                                                    void* userData);

    D3IV_API D3IV_Result D3IV_CALL D3IV_SetROIEventCallback(D3IV_Viewer* viewer,
                                                             D3IV_ROIEventCallback callback,
                                                             void* userData);

    // ─────────────────────────────────────────────────────────────────────
    // 뷰 제어 / 좌표 변환
    // ─────────────────────────────────────────────────────────────────────
    D3IV_API D3IV_Result D3IV_CALL D3IV_SetZoom(D3IV_Viewer* viewer, float zoom, int32_t animate);
    D3IV_API D3IV_Result D3IV_CALL D3IV_GetZoom(D3IV_Viewer* viewer, float* outZoom);
    D3IV_API D3IV_Result D3IV_CALL D3IV_ZoomFit(D3IV_Viewer* viewer, int32_t animate);
    D3IV_API D3IV_Result D3IV_CALL D3IV_Zoom1To1(D3IV_Viewer* viewer, int32_t animate);

    D3IV_API D3IV_Result D3IV_CALL D3IV_SetCenter(D3IV_Viewer* viewer,
                                                   float imageX, float imageY,
                                                   int32_t animate);
    D3IV_API D3IV_Result D3IV_CALL D3IV_GetCenter(D3IV_Viewer* viewer,
                                                   float* outX, float* outY);

    // imageRect 가 화면에 꽉 차도록. marginRatio 는 여백 비율(0.1 = 10%).
    D3IV_API D3IV_Result D3IV_CALL D3IV_ZoomToRect(D3IV_Viewer* viewer,
                                                    const D3IV_Rect2f* imageRect,
                                                    float marginRatio,
                                                    int32_t animate);

    D3IV_API D3IV_Result D3IV_CALL D3IV_GetVisibleImageRect(D3IV_Viewer* viewer,
                                                             D3IV_Rect2f* outRect);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ScreenToImage(D3IV_Viewer* viewer,
                                                       int32_t screenX, int32_t screenY,
                                                       float* outImageX, float* outImageY);

    D3IV_API D3IV_Result D3IV_CALL D3IV_ImageToScreen(D3IV_Viewer* viewer,
                                                       float imageX, float imageY,
                                                       int32_t* outScreenX, int32_t* outScreenY);

    // ─────────────────────────────────────────────────────────────────────
    // 표시 옵션
    // ─────────────────────────────────────────────────────────────────────
    D3IV_API D3IV_Result D3IV_CALL D3IV_SetToolbarVisible(D3IV_Viewer* viewer, int32_t visible);
    D3IV_API D3IV_Result D3IV_CALL D3IV_SetStatusBarVisible(D3IV_Viewer* viewer, int32_t visible);
    D3IV_API D3IV_Result D3IV_CALL D3IV_SetBackgroundColor(D3IV_Viewer* viewer, uint32_t colorRGB);
    D3IV_API D3IV_Result D3IV_CALL D3IV_SetVSyncEnabled(D3IV_Viewer* viewer, int32_t enable);

#ifdef __cplusplus
} // extern "C"
#endif
