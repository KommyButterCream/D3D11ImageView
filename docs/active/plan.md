# 공개 API 재설계 — 검사/의료 장비 뷰어용

작성: 2026-08-24 · 갱신: 2026-08-24 (C# 방향 확정)

> **결정됨**: 의료 영상 제외 · C# (WPF) 소비자 포함 · ROI/Overlay getter 는
> 해석적/정점 별도 메서드 · 마우스 콜백 도입(가로채기 가능)

목표: `D3D11ImageView.dll` + `D3D11ImageView.h` 만으로 검사장비·의료장비의
이미지 뷰어를 구성할 수 있게 한다.

---

## 0. 먼저 짚어야 할 것 — 현재 API 로는 목표 달성이 불가능한 지점

설계 논의 전에, 조사 중 발견한 **기능적 차단 요소**부터 확인이 필요하다.

### 0-1. 12/16bit 영상이 거의 검게 나온다 (차단)

현재 16bit Gray 는 `R16_UNORM` 텍스처로 올라가고 셰이더가 `value / 65535` 로
정규화한다. 그런데 실제 장비 데이터는 컨테이너를 다 쓰지 않는다.

| 소스 | 실제 값 범위 | 현재 표시 밝기 |
|---|---|---|
| 12bit 산업용 센서 | 0 ~ 4,095 | 최대 **6.3%** |
| 14bit 센서 | 0 ~ 16,383 | 최대 25% |
| CT (Hounsfield) | **−1,024 ~ 3,071** | 음수 → 0 으로 클램프 |

즉 12bit 카메라 영상은 거의 검은 화면으로, CT 는 대부분 검게 나온다.
**Window/Level(표시 범위 매핑) 없이는 이 용도로 쓸 수 없다.**

### 0-2. 부호 있는 16bit 을 표현할 수 없다 (의료)

CT 의 HU 는 음수를 포함한다. `R16_UNORM` 은 부호가 없다. `R16_SNORM` 은
−1..1 로만 정규화되어 HU 를 그대로 담지 못한다. 실무 해법은 `R16_UINT` 또는
`R16_SINT` 로 올리고 셰이더에서 정수값을 읽어 window/level 을 적용하는 것이다.

### 0-3. 줌/팬을 프로그램에서 제어할 수 없다

현재 공개 API 에 줌·팬 관련 함수가 **하나도 없다.** 마우스와 툴바 버튼으로만
조작 가능하다. 호스트가 "이 ROI 로 이동해서 4배 확대" 같은 동작을 할 수 없다.

### 0-4. 화면 캡처 수단이 없다

검사/의료 모두 리포트에 현재 화면을 첨부한다. 뷰어에서 이미지를 꺼낼 방법이 없다.

**→ 이 4가지는 "API 를 다양하게" 이전에 결정이 필요하다. §6 질문 참조.**

---

## 1. DLL 경계 규칙 (모든 설계의 전제)

### 현재 상태

- `Core` = **StaticLibrary**, `D3D11ImageView` = DLL
- 양쪽 다 `RuntimeLibrary` 미지정 → MSVC 기본값 `/MDd`(Debug), `/MD`(Release)
- 즉 **동적 CRT 라 힙이 공유된다**. 지금은 우연히 동작한다.

### 위험

`Polygon2f` 는 힙을 소유한다(`Reserve` 에서 할당, 소멸자에서 해제).
`IROIObject::GetKey()` 는 `std::wstring&` 를 반환한다.

이걸 반환 타입에 쓰면 **DLL 에서 할당하고 호스트에서 해제**하게 된다.
Debug 호스트 + Release DLL 조합(현장에서 흔하다)에서는 힙이 달라 즉시 깨진다.
툴셋이 다른 경우도 마찬가지다.

### 규칙

공개 시그니처에 **소유권 이전 없음, STL 없음, 소유 타입 없음.**

```cpp
// 나쁨 — DLL 이 할당, 호스트가 해제
Polygon2f ROIGetPolygon(const wchar_t* key);

// 좋음 — 호출자 버퍼, 2회 호출 패턴 (Win32 관례)
//   1회차: buffer = nullptr  -> 필요 개수를 outCount 에 반환
//   2회차: 버퍼 할당 후 재호출
bool ROIGetVertices(const wchar_t* key,
                    Point2f* buffer, uint32_t bufferCount,
                    uint32_t* outCount) const;
```

문자열도 동일하게 `wchar_t* buffer, uint32_t bufferChars, uint32_t* outChars`.

---

## 2. ROI 조회 — 무엇을 반환할 것인가

### 2-1. 결론: 해석적(analytic) 형태가 1차, 정점 배열은 명시적 옵션

원의 면적은 `πr²` 이지 다각형 근사의 면적이 아니다. 검사·의료의 계측값은
해석적 파라미터에서 나와야 한다. 반대로 마스킹·픽셀 순회에는 정점이 필요하다.
**둘 다 제공하되 역할을 분리한다.**

```cpp
enum class ROIShapeType : uint32_t { Rectangle = 0, Ellipse, Circle, Polygon };

// POD. union 이므로 가변 길이인 Polygon 은 여기 담지 않는다.
struct ROIShape
{
    ROIShapeType type;
    uint32_t     vertexCount;      // Polygon 일 때만 유효

    union
    {
        struct { float left, top, right, bottom; }     rect;
        struct { float cx, cy, rx, ry, angleRad; }     ellipse;
        struct { float cx, cy, radius; }               circle;
    };
};

// 해석적 형태 — 계측용
bool ROIGetShape(const wchar_t* key, ROIShape* outShape) const;

// 정점 배열 — 마스킹/순회용. 곡선은 segmentsPerCurve 로 근사한다.
//   Rectangle -> 4점, Polygon -> 원본 정점, Circle/Ellipse -> 근사
bool ROIGetVertices(const wchar_t* key,
                    Point2f* buffer, uint32_t bufferCount,
                    uint32_t* outCount,
                    uint32_t segmentsPerCurve = 64) const;
```

**왜 정점만 반환하지 않는가**: 원을 정점으로 받아 다시 `ROISet` 하면 다각형이
되어 편집 핸들과 계측 정확도를 잃는다. 왕복이 손실 없이 되어야 한다.

### 2-2. 부가 정보

```cpp
struct ROIInfo
{
    ROIShapeType type;
    COLORREF     color;
    bool         isMovable;
    bool         isResizable;
    bool         isSelected;
    bool         isHovered;
    long         fontSize;
    // 이름/키는 별도 버퍼 호출 (문자열이므로)
};

bool ROIGetInfo(const wchar_t* key, ROIInfo* outInfo) const;
bool ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars,
                uint32_t* outChars) const;
```

### 2-3. 목록 조회

```cpp
uint32_t ROIGetCount() const;

// 키 목록. 키 길이가 가변이라 고정 폭 2차원 배열로 받는 게 가장 단순하다.
//   buffer 는 [count][charsPerKey] 로 취급
bool ROIGetKeys(wchar_t* buffer, uint32_t charsPerKey, uint32_t bufferCount,
                uint32_t* outCount) const;
```

### 2-4. 클릭한 ROI 조회 — 두 가지 경로

**(a) 폴링** — 호스트가 원할 때 현재 선택을 묻는다.

```cpp
bool ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars,
                       uint32_t* outChars) const;

// 임의 좌표에서 히트 테스트 (이미지 좌표계)
bool ROIHitTest(float imageX, float imageY, float tolerance,
                wchar_t* keyBuffer, uint32_t bufferChars,
                uint32_t* outChars) const;
```

**(b) 이벤트 콜백** — 사용자가 조작한 순간을 통지한다. 이쪽이 실사용 형태다.

```cpp
enum class ROIEventType : uint32_t
{
    Selected = 0,   // 클릭으로 선택됨
    Deselected,     // 빈 곳 클릭 등으로 해제
    EditBegin,      // 드래그 시작 (이동/리사이즈)
    EditChanged,    // 드래그 중 (매 프레임 아님, 형상 변화 시)
    EditEnd,        // 드래그 종료 <- 결과 확정은 보통 여기서
    DoubleClicked
};

using ROIEventCallback = void (*)(ROIEventType type,
                                  const wchar_t* key,
                                  void* userData);

void SetROIEventCallback(ROIEventCallback callback, void* userData);
```

> **주의 — 데드락 위험**
> ROI 마우스 처리는 `m_roiLock` 을 잡은 채로 실행된다. 콜백을 락 안에서
> 호출하면 호스트가 콜백에서 `ROISet`/`ROIGetShape` 를 부르는 순간 자기
> 자신을 기다리게 된다.
> **→ 이벤트를 큐에 넣고 락을 푼 뒤 디스패치하도록 구현한다.**
> (이 제약은 문서화만으로 넘기면 반드시 사고가 난다)

---

## 3. 오버레이 조회

오버레이는 현재 "넣고 지우는" 용도라 식별자가 없다. 검사에서는 **결함 마커를
클릭해서 그 결함 정보를 띄우는** 시나리오가 흔하므로 식별이 필요하다.

```cpp
using OverlayHandle = uint64_t;   // 0 = 무효

// 기존 Add 는 그대로 두고, 핸들을 받고 싶을 때만 out 배열을 넘긴다.
// nullptr 이면 핸들을 만들지 않는다(대량 추가 시 부기 비용 회피).
void ImageOverlayAdd(const Point2f* points, size_t count,
                     const OverlayStyle& style,
                     OverlayHandle* outHandles = nullptr);

OverlayHandle ImageOverlayHitTest(float imageX, float imageY,
                                  float tolerance) const;

bool ImageOverlayGetBounds(OverlayHandle handle, Rect2f* outBounds) const;
bool ImageOverlayRemove(OverlayHandle handle);
```

**성능 메모**: 실측상 오버레이 병목은 컬링(10ns/개체)이 아니라 D2D 드로우
(420ns/보이는 개체)다. 핸들 부기는 기존 vector 의 인덱스라 비용이 사실상 없다.
히트 테스트는 컬링과 같은 선형 순회라 10만 개에서도 1ms 다.

---

## 4. UpdateImage — 타입 확장

### 4-1. `channel + bitDepth` → 픽셀 포맷 열거형

지금 조합은 표현력이 낮고 확장할수록 인자가 는다. 열거형이 명확하다.

```cpp
enum class PixelFormat : uint32_t
{
    Gray8 = 0,      // R8_UNORM
    Gray16,         // R16_UNORM   (현재 유일한 16bit 경로)
    Gray16S,        // R16_SINT    <- CT/HU. 신규
    GrayF32,        // R32_FLOAT   <- 신규
    BGR8,           // 3채널 -> BGRA 확장
    BGRA8,
    RGB8,           // 채널 순서 반대 (많은 카메라 SDK 가 RGB)
    RGBA8,
};

bool UpdateImage(const void* data, uint32_t width, uint32_t height,
                 uint32_t stride, PixelFormat format);
```

기존 두 오버로드는 **호환을 위해 유지**하고 내부에서 위 함수로 위임한다.

### 4-2. Window / Level (§0-1, §0-2 해결)

```cpp
// 표시 범위. 이 구간이 0..1 로 매핑된다.
//   12bit 센서: SetDisplayRange(0, 4095)
//   CT 폐 창:   SetDisplayRange(-1000, 200)
void SetDisplayRange(double minValue, double maxValue);

// 현재 이미지의 실제 값 분포에서 자동 결정
//   lowPercentile/highPercentile 로 이상치 제외 (예: 0.5, 99.5)
void SetDisplayRangeAuto(double lowPercentile = 0.0,
                         double highPercentile = 100.0);

void GetDisplayRange(double* outMin, double* outMax) const;

void SetInvert(bool invert);   // 의료 영상에서 흔함
```

구현은 픽셀 셰이더 상수 버퍼에 `(min, scale)` 을 넘기는 정도라 비용이 없다.
**8bit 경로에도 적용하면 검사장비의 밝기/대비 조정에 그대로 쓸 수 있다.**

### 4-3. 보류 후보

- 부분 갱신 `UpdateImageRegion(...)` — 라인스캔 누적에 유용하나 요구 확인 필요
- LUT / 의사컬러(heatmap) — 검사 결과 시각화용
- Bayer 디모자이킹 — 카메라 SDK 가 처리하는 게 보통

---

## 5. 그 밖의 공개 API 공백

목표를 "뷰어로 사용 가능" 으로 잡으면 아래도 필요하다.

### 5-1. 뷰 제어 (§0-3)

```cpp
void  SetZoom(float zoom, bool animate = true);
float GetZoom() const;
void  ZoomFit(bool animate = true);
void  Zoom1To1(bool animate = true);
void  SetCenter(float imageX, float imageY, bool animate = true);
void  GetCenter(float* outX, float* outY) const;

// 특정 영역이 화면에 꽉 차도록 (ROI 로 이동 시나리오)
void  ZoomToRect(const Rect2f& imageRect, float marginRatio = 0.1f,
                 bool animate = true);

// 현재 화면에 보이는 이미지 영역
bool  GetVisibleImageRect(Rect2f* outRect) const;
```

### 5-2. 좌표 변환

```cpp
bool ScreenToImage(int32_t screenX, int32_t screenY,
                   float* outImageX, float* outImageY) const;
bool ImageToScreen(float imageX, float imageY,
                   int32_t* outScreenX, int32_t* outScreenY) const;
```

### 5-3. 이미지 정보 / 픽셀 조회

```cpp
bool GetImageSize(uint32_t* outWidth, uint32_t* outHeight) const;
bool GetImageFormat(PixelFormat* outFormat) const;

// 상태바를 쓰지 않고 호스트가 직접 픽셀값을 읽는 경로
bool GetPixelValue(int32_t imageX, int32_t imageY,
                   double* outValues, uint32_t valueCount,
                   uint32_t* outChannelCount) const;
```

### 5-4. 화면 캡처 (§0-4)

```cpp
// 현재 렌더 결과를 BGRA8 로 받는다. 오버레이/ROI 포함 여부 선택.
bool CaptureView(uint8_t* buffer, uint32_t bufferBytes,
                 uint32_t* outWidth, uint32_t* outHeight,
                 uint32_t* outStride,
                 bool includeOverlay = true) const;
```

### 5-5. 표시 옵션

```cpp
void SetToolbarVisible(bool visible);      // 호스트가 자체 UI 를 쓰는 경우
void SetStatusBarVisible(bool visible);
void SetBackgroundColor(COLORREF color);
void SetVSyncEnabled(bool enable);         // 이미 엔진에 있음, 노출만
```

---

## 6. 확인이 필요한 결정 사항

| # | 질문 | 선택지 | 제안 |
|---|---|---|---|
| Q1 | **소비자 언어** | C++ 전용 / C# · Python 등 포함 | C++ 전용이면 현행 클래스 유지. 타 언어가 있으면 **flat C ABI 계층**을 별도로 얹어야 함 (현재 C++ 클래스는 P/Invoke 불가) |
| Q2 | **Window/Level** | 필수 / 나중 | **필수.** 없으면 12bit·CT 가 검게 나옴 (§0-1) |
| Q3 | **부호 있는 16bit** | 필요 / 불필요 | 의료가 목표면 필요. 검사만이면 불필요 |
| Q4 | **ROI 반환 형식** | 해석적+정점 / 정점만 / 해석적만 | **해석적 1차 + 정점 옵션** (§2-1) |
| Q5 | **ROI 이벤트** | 콜백 / 폴링 / 둘 다 | **둘 다.** 콜백이 주, 폴링은 보조 |
| Q6 | **오버레이 식별** | 핸들 도입 / 현행 유지 | 결함 클릭 시나리오가 있으면 핸들 필요 |
| Q7 | **뷰 제어·캡처** | 이번 범위 / 다음 | 뷰어 목표면 이번 범위 |
| Q8 | **기존 API 호환** | 유지 / 정리 | **유지 권장.** 신규는 추가만, 기존은 위임 |

---

## 7. 제안 순서

승인 후 아래 순서를 제안한다. 각 단계마다 빌드·회귀 검증.

1. **DLL 경계 정리 + 포맷 열거형** — `PixelFormat`, `UpdateImage` 신규 오버로드
2. **Window/Level** — 셰이더 상수 + 자동 범위. §0-1/0-2 해소
3. **ROI 조회** — `ROIGetShape` / `ROIGetVertices` / `ROIGetInfo` / 목록
4. **ROI 이벤트** — 큐 기반 디스패치 (락 밖에서 호출)
5. **뷰 제어 + 좌표 변환**
6. **오버레이 핸들 + 히트 테스트**
7. **캡처 + 표시 옵션**

1~2 는 §0 의 차단 요소라 먼저 하는 것이 맞다고 본다.


---

# 8. C# / WPF 연동 (2026-08-24 확정)

## 8-1. 필요한 산출물 3개

`.cs` 파일만으로는 안 된다. DLL 이 맹글링된 C++ 심볼만 내보내고 있어
P/Invoke 대상이 없다.

| # | 산출물 | 위치 | 단계 |
|---|---|---|---|
| 1 | `D3D11ImageViewC.h` / `.cpp` | 이 저장소 (DLL) | 1 |
| 2 | `D3D11ImageView.cs` — P/Invoke + `IDisposable` 래퍼 | C# 쪽 | 2 |
| 3 | `ImageViewHost.cs` — WPF `HwndHost` 파생 | C# 쪽 | 3 |

C 계층은 **위임만** 한다. 로직이 들어가면 C++/C API 동작이 갈라진다.

## 8-2. WPF 는 HwndHost — Airspace 제약을 먼저 합의해야 한다

WPF 는 컨트롤마다 HWND 가 없다. 창 전체가 HWND 하나다. 반면 뷰어는 부모 HWND
아래 `WS_CHILD` 로 자기 창을 만든다. 그래서 `HwndHost` 로 다리를 놓는다.

**HWND 자식은 자기 사각형 안에서 항상 WPF 콘텐츠보다 위에 그려진다.**

| 하려던 것 | 결과 |
|---|---|
| 이미지 위에 WPF 버튼/라벨 | 가려짐 |
| 이미지 위에 툴팁/팝업/ContextMenu | 잘림 |
| `Opacity` / `RotateTransform` | 무시됨 |
| 부모 클리핑 / ScrollViewer | 무시됨 |

**→ 이미지 위 표시물은 전부 뷰어의 오버레이/ROI API 로 그려야 한다.**
주변 UI(툴바·사이드패널·결과리스트)는 이미지 바깥이므로 문제없다.

대안인 `D3DImage` 는 D3D9Ex 표면만 받으므로 D3D11→D3D9 공유 브리지 + 오프스크린
렌더 경로 분기가 필요하다. **투명 오버레이 요구가 실제로 생기면** 그때 검토한다.

부수적으로 확인된 것:
- **DPI**: `HwndHost` 크기는 DIP 단위. `TransformToDevice` 로 곱해야 한다.
  뷰어에 DPI 인식이 없으므로 이 변환은 C# 책임.
- **키보드**: WPF Tab 이동이 자식 HWND 로 넘어가지 않는다. 필요하면
  `HwndHost.TabIntoCore` 구현.
- **델리게이트 수명**: 콜백 델리게이트를 필드로 잡지 않으면 GC 수거 후 프로세스
  사망. 래퍼 클래스가 감춘다.

## 8-3. 오버레이 Add 는 열거형으로 합친다

C++ 쪽 `ImageOverlayAdd` 오버로드가 21개 × 2(image/window) = 42개다. C 계층에서
42개를 그대로 내보내면 C# 쪽이 지옥이 된다. **도형 타입 열거형 + `const void*`**
로 하나로 합친다.

```c
D3IV_Result D3IV_ImageOverlayAdd(D3IV_Viewer* v, int32_t shapeType,
                                 const void* items, uint32_t count,
                                 const D3IV_OverlayStyle* style,
                                 uint64_t* outHandles /* nullable */);
```

C 계층이 `shapeType` 으로 분기해 해당 C++ 오버로드를 부른다. C# 은 P/Invoke
하나 + enum 하나로 끝난다.

## 8-4. 마우스 콜백 — 가로채기 가능해야 한다

단순 통지만으로는 부족하다. 뷰어가 이미 마우스를 소비하므로(팬/줌/ROI 편집/
선택 사각형), 호스트가 선점할 수 없으면 측정 도구나 대화형 ROI 생성을 만들 수 없다.

```c
// 반환값: 0 = 뷰어가 계속 처리, 1 = 호스트가 처리 완료 -> 뷰어는 무시
typedef int32_t (*D3IV_MouseCallback)(const D3IV_MouseEvent* e, void* userData);
```

**발생 위치**: UI 처리(툴바/컨텍스트메뉴) 다음, ROI 처리 앞.

```
WndProc -> HandleMouseEventUI -> [호스트 콜백] -> ROI 편집 -> 선택/팬
```

- 툴바 클릭이 호스트로 쏟아지지 않는다
- 호스트가 ROI 편집·팬·선택을 선점할 수 있다
- ROI 히트 여부가 필요하면 콜백 안에서 `D3IV_ROIHitTest` 를 직접 부른다

**스레드**: UI 스레드(WndProc). ROI 이벤트도 같은 스레드로 통일한다.

**고수준 모드(`SetInteractionMode`)는 지금 만들지 않는다.** 실제 워크플로를 모르는
상태에서 정하면 틀린다. 원시 콜백 + 가로채기를 주면 호스트가 모드를 올려 만들 수
있고, 패턴이 굳으면 편의 API 로 승격한다.

## 8-5. ROI 이벤트 콜백의 데드락 제약 (재확인)

ROI 마우스 처리는 `m_roiLock` 을 쥔 채 실행된다. 콜백을 락 안에서 호출하면
호스트가 콜백에서 `ROISet`/`ROIGetShape` 를 부르는 순간 자기 자신을 기다린다.

**→ 이벤트를 큐에 넣고 락을 푼 뒤 디스패치한다.** 문서화만으로 넘기면 사고 난다.

마우스 콜백도 동일하다. `m_roiLock` 밖에서 호출되도록 삽입 지점을 잡는다
(§8-4 의 위치는 ROI 처리 *앞* 이므로 락 밖이다 — 조건 충족).

## 8-6. 수정된 작업 순서

1. **C ABI 계층** — 기존 공개 API 전체 + 오버레이 열거형 통합
2. **ROI / Overlay getter** — 해석적(`GetShape`) / 정점(`GetVertices`) 분리
3. **마우스 · ROI 이벤트 콜백** — 큐 기반 디스패치
4. **`D3D11ImageView.cs`** — P/Invoke + `IDisposable` 래퍼
5. **`ImageViewHost.cs`** — WPF `HwndHost` + DPI 보정
6. **WPF 최소 예제로 실제 연동 검증** (이미지 표시 + 휠 줌 + ROI 1개 + 좌표 표시)
7. 그 다음 뷰 제어 / 캡처 / Window·Level

6번까지가 "붙는지" 검증이다. 신규 API 를 다 설계하고 나중에 붙이면, 붙이는
단계에서 나오는 제약이 설계를 되돌린다.

## 8-7. 미결

- **센서 비트수** — 12bit 이상이면 Window/Level 이 필수(§0-1). 8bit 만이면
  밝기/대비 편의 기능으로 뒤로 미룰 수 있다. **답 필요.**
- 키보드 콜백 — Delete 로 ROI 삭제, 방향키 미세 이동 등. 필요해지면 추가
- `D3D11ImageView.cs` 를 이 저장소에 둘지, C# 솔루션 쪽에 둘지
