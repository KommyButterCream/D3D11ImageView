# 타일링/LOD 리팩터링 체크리스트

최종 갱신: 2026-08-24

---

## 완료

### 기반
- [x] GLViewer_2DEngine ↔ D3D11ImageView 구조 비교 분석
- [x] 근본 원인 규명 — `maxLOD = 4` 하드코딩 (fit 줌에서 1,369타일 vs 풀 20칸)
- [x] TestHost 앱 작성 (합성 이미지 6크기 × 3채널, 단축키 구동)

### 1단계 — Single 모드
- [x] `MipLevels = 0` 전체 밉 체인 + `GenerateMips()` (3개 업로드 경로)
- [x] 샘플러 분리 (Single = `MIN_LINEAR_MAG_POINT_MIP_LINEAR`, Tiled = POINT 유지)
- [x] raw 업로드 버퍼 268MB 선할당 제거 → 지연 생성
- [x] `m_maxByteSize` 미대입 버그 수정

### 2·3단계 — 판정 + 포맷
- [x] `TileFormat.h` 신규 — 포맷 결정 + Single/Tiled 판정
- [x] `CanUseSingleTexture()` (변 16384 + 상주 예산 128MB/뷰어)
- [x] R8/R16 포맷 파라미터화 (`TilePool`, `CreateSingleBuffer`)
- [x] `ImageGrayPS.hlsl` 신규 + PS 선택 로직
- [x] `ImagePS.hlsl` 알파 1.0 고정 (D2D PREMULTIPLIED 블렌딩 오염 수정)
- [x] `ReleaseUnusedModeResources()` — 모드 전환 시 반대쪽 해제
- [x] `UpdateTexture`/`UpdateSharedTexture` 제한 8192 → 16384

### 4단계 — CS 경로 제거
- [x] `TileSamplingCS.hlsl` 삭제, `UploadMode` / `CachedRegion` / raw 버퍼 제거
- [x] `TilePool` UAV 바인딩 제거 (R8/R16 typed UAV store 문제 소멸)
- [x] `SingleConvertCS` 는 유지 (Single 모드 BGR 확장용)

### 5단계 — maxLOD + 용량 ★
- [x] `Impl::Initialize` 의 하드코딩 `TileSystemDesc` 삭제
- [x] `TileManager::Configure()` — 이미지마다 풀 재구성
- [x] `maxLOD = ceil(log2(imageMax / viewMin))`
- [x] `capacity[L] = min(whole_L, workingSet × 2)`
- [x] `NeedsReconfigure()` — 뷰포트 변화 시에만 재구성

### 6단계 — 프라이밍
- [x] `PrimeCoarsestLevel()` — maxLOD 전체를 예산 무시하고 채움
- [x] `RenderTiled()` 의 `IsPrimed()` 게이트
- [x] fallback 실패(`continue` = 구멍) 경로 제거

### 7단계 — 예산/게이팅/프리페치
- [x] `MAX_UPLOAD_PER_FRAME = 4` → 시간 예산 2ms
- [x] 모션 게이팅 (`Camera2D::IsSettled()` 신규)
- [x] 프리페치 마진 1타일
- [x] 뷰포트 중심 거리순 로드

### 8단계 — 축출/상태
- [x] `EvictLRU()` 현재 프레임 가시 타일 보호
- [~] ~~주석 처리돼 있던 `Evict()` 정기 호출 복구~~ → **2026-08-24 재제거** (오판이었음)
- [x] `TileState` 4상태 → 2상태 (`None`/`Resident`)

### 계획 외 (작업 중 발견)
- [x] `Camera2D::GetMinZoom()` — 큰 이미지에서 fit 배율까지 축소 허용
- [x] `DetachImage()` API — `Attach` 수명 계약의 안전한 해제 경로
- [x] `ImagePS.hlsl` CP949 깨진 주석 복구
- [x] `OverlayTypes.h` 미사용 변수 (C4189)
- [x] 와이어프레임 기본 OFF
- [x] `IsImageRenderDirty()` null 역참조 방어
- [x] `CalcVisibleKeys` 음수 → unsigned 변환 방어
- [x] 경계 타일만 scratch clear (내부 타일 작업량 절반)

### 마무리
- [x] Debug/Release x64 빌드 — `/W4` 경고 0
- [x] 커밋 + push (D3D11ImageView `8ffb456`, D3D11Engine `3133426`)
- [x] `.gitignore` 에 `/Shaders/` 추가

---

## 롤백

- [x] ~~10단계 — 타일 워커 스레드 풀~~ → **구현 후 롤백** (시기상조)
  - 설계·검증은 완료. 문서 §9 에 D1~D7 결정과 재도입 전제 기록
  - `TileSampler.h` 와 `DetachImage()` 는 유지

---

## 미완료 — 다음 세션

### 🔴 육안 검증 (최우선, 코드는 이미 push 됨)
- [ ] 밉맵이 축소 지글거림을 실제로 없앴는지
- [ ] 타일 경계 이음새 유무 (TestHost 의 512px 격자선으로 확인)
- [ ] 색/채널 순서 — Gray(R8 스위즐) / BGR 확장 / BGRA
- [ ] 프라이밍이 "한 번에 나타나는" 체감을 주는지
- [x] **ImageSpace 반픽셀 오프셋** → 사용자 판단으로 **제거**  *(2026-08-24)*

### 🟡 미측정
- [x] 성능 계측 수행 — 아래 2026-08-24 절 참조  *(30만px 실측은 여전히 불가)*
- [ ] 30만px 실측 (90GB RAM 필요, TestHost 로 불가)

### 🟢 후속 (필요해질 때)
- [x] 오버레이 `ID2D1PathGeometry` 프레임마다 재생성 → 캐시  *(2026-08-24)*
- [ ] RAM 피라미드 (이미지 최대 변 > 20,480px 일 때)
- [ ] 워커 스레드 재도입 (라이브 뷰/스트리밍 요구 확정 시)
- [x] `syncInterval = 1` 로 전환 + `SetVSyncEnabled` 토글  *(2026-08-24)*
- [ ] `WarningLevel` 을 Level3 → Level4 로 (현재 W4 에서도 경고 0 확인됨)


---

# 2026-08-24 세션 — 디바이스 로스트 / 스레드 안전 / 프레임 페이싱

커밋: D3D11ImageView `5d8bafa`, D3D11Engine `29004db`, Core `a3e2118` (전부 push 완료)

## 완료

### 디바이스 로스트 / 복구
- [x] `SimulateDeviceLost()` 테스트 API — 실제 로스트 경로를 그대로 실행
- [x] **크래시 수정** — `UIRenderLayer::RebindFontManager()`
      엔진이 `FontManager` 를 delete/재생성하는데 UI 요소가 생성 시점 raw
      포인터를 유지 → 복구 중 use-after-free (상태바 첫 라벨에서 실제 크래시)
- [x] `ImageRenderLayer` / `TileManager` 에 `OnDeviceLost` / `OnDeviceRestored`
- [x] `m_deviceResourcesReady` 게이트 — 로스트~복구 구간 렌더 차단
- [x] `RestoreImageAfterDeviceLoss()` — 원본 CPU 버퍼로 화면 복원
- [x] `TileManager::m_lastConfig` — 복구 시 동일 구성 재생성
- [x] D2D 지오메트리 캐시 무효화 (팩토리 재생성 → `D2DERR_WRONG_FACTORY` 방지)

### 스레드 안전
- [x] 상태바 갱신을 렌더 스레드로 이동
      UI 스레드가 `ImageBase` / `Camera2D` / `UILabel::m_text` 를 만지는 동안
      렌더 스레드가 `Attach()` 로 버퍼를 교체 (`SetText` 는 `delete[]`/`new` 수행)
- [x] `RenderThread::StopThread` lost wakeup — `Join()` 영구 대기 가능성 제거

### 윈도우 메시지
- [x] `WM_CAPTURECHANGED` — 캡처 상실 시 팬/선택/ROI 드래그 정리
- [x] `WM_MOUSELEAVE` + `TrackMouseEvent` — hover/상태바 고착 해소

### 프레임 페이싱
- [x] `SwitchToThread` 스핀 → 고해상도 대기 타이머 (+ 정지 이벤트 동시 대기)
- [x] `SetRenderFPS` 실행 중 반영 (루프 밖 1회 계산 제거)
- [x] `Present(1, 0)` vsync + `SetVSyncEnabled` 토글
- [x] `FLIP_SEQUENTIAL` → `FLIP_DISCARD`
- [x] `SetMaximumFrameLatency(1)` (DXGI 기본 3)
- [x] `SetRenderFPS(240)` — 페이싱은 Present 담당, 이 값은 안전망 상한

### 애니메이션
- [x] 유휴 기상 첫 프레임의 dt 배제 (`RenderContext::resumedFromIdle`)
- [x] 카메라 보간 종료 판정을 "남은 변화 < 화면 1픽셀" 로 변경
- [x] 확대 필터를 배율 기준 + 히스테리시스로 전환 (2.0 / 1.7)

### 이미지 포맷
- [x] `UpdateImage` 에 `bitDepth` 오버로드 — 16bit Gray 경로 개방
- [x] 하드코딩 `kSourceBitDepth = 8` 2곳 제거 (`Render()` 재구성 경로 포함)
- [x] 16bit 다채널 명시적 거절 + stride 하한 검증

### 타일 캐시
- [x] `Configure` 레이아웃 동일 시 풀 재생성 생략 → `ClearPools`
- [x] **시간 기반 `Evict` 제거** — 슬롯은 고정 `ArraySize`, 메모리 이득 없음
- [x] 정점 버퍼 용량 검사 + 2배 성장 (`m_maxTileVertexCount` 미사용 → 오버런)
- [x] `CreateTileVertexBuffer` 실패 시 용량 0 처리

### 오버레이 / ROI
- [x] `OverlayPolyShapeRenderer` / `ROIPolygonRenderer` / `OverlayRectangleRenderer`
      지오메트리 캐시
- [x] `OverlayUtilities.h` — `ResolveStrokeWidth` / `CreatePolyGeometry` 공용화
- [x] `Core/Util/MathUtil.h` — `DegToRad` / `RadToDeg` / `kPi`
- [x] 반픽셀 오프셋 제거

## 실측 결과 (RTX 5080 + Ryzen 9 9950X, 2560x1440 @ 60Hz)

| 항목 | 수치 |
|---|---|
| 유휴 CPU | 0.0% |
| 8192² 렌더 (Gray Single / BGRA Tiled) | 0.40 / 0.54 ms |
| 8192² 라이브 급전 | 117 fps (페이싱 상한) — 60fps 여유 충분 |
| 오버레이 컬링 | 개체당 10 ns |
| 오버레이 D2D 드로우 | **보이는 개체당 420 ns** ← 실질 병목 |
| 60fps 기준 동시 표시 한계 | 프리미티브 약 40,000개 |
| 스핀 → 대기 타이머 | 26.9% → 7.6% (1코어 기준) |
| 줌 애니메이션 꼬리 | 35프레임 → 24프레임 |

**락은 병목이 아니다** — 프레임당 3~4회, 전부 무경쟁.

## 효과가 없던 수정 (정직하게 기록)

의미상 옳지만 이 장비에서 측정 가능한 개선이 없었던 것들:

| 수정 | 전 | 후 |
|---|---|---|
| `Configure` 조기 반환 | 0.148 코어 | 0.211 |
| 사각형 지오메트리 캐시 (10k) | 1.462 코어 | 1.550 |

이유: `CreateTexture2D(pInitialData=nullptr)` 와 소형 D2D 객체 생성은 이 하드웨어에서
프레임당 비용이 되지 않는다. **프레임당 리소스 재생성을 성능 문제로 가정하지 말 것.**

## 재현하지 못한 것

아래 3건은 **코드상 동기화 부재/UB 가 근거**이고 실측 크래시는 재현하지 못했다.

- 상태바 경합 — 100만 회 마우스 이동 + 4,000회 이미지 전환에도 미재현
- `StopThread` lost wakeup — 40회 기동/종료 전부 정상
- 정점 버퍼 오버런 — 검사 제거 빌드로 4~5KB 초과 기록해도 미재현
  (드라이버 스테이징 버퍼 뒤 여유 영역에 기록됨)

앞의 둘은 "불필요한 작업" 이 아니라 **정의되지 않은 동작**이므로 재현 여부와 무관하게
수정이 타당하다.

## 미완료 — 다음 세션

### 확인 필요 (사용자 판단)
- [ ] 확대 필터 임계 2.0 / 1.7 이 검사 워크플로에 맞는지
      (더 드물게 하려면 3.0 / 2.6, 더 빨리 선명해지려면 1.5 / 1.3)
- [ ] vsync 전환 후 줌 애니메이션이 실제로 매끄러운지 (지각 판단)

### 후속 작업
- [ ] **AA-nearest 픽셀 셰이더** — 확대 필터 전환을 원천 제거하는 최종형
      `fwidth` 로 텍셀 경계만 1픽셀 안티에일리어싱. 임계·전환 불필요
      `ImagePS.hlsl` / `ImageGrayPS.hlsl` + 텍스처 크기 상수 버퍼 필요
- [ ] 오버레이 40,000 프리미티브 초과 요구가 생기면 D2D 개체별 드로우 탈피
      (단일 지오메트리 병합 또는 D3D 인스턴싱)
- [ ] 리소스 상대 경로(`../Icons`, `../Shaders`) → DLL 리소스 임베드
      현재 **아이콘은 로드 자체가 실패 중** (`Icons/` 폴더가 이 저장소에 없음)
      셰이더는 디바이스 복구 시 재로드하므로 CWD 변경에 취약
- [ ] `HighResTimer` dt 클램프 0.1초 → 2~3 프레임분 (히칭 시 애니메이션 튐 완화)
- [ ] VRAM 예산 검사 (`QueryVideoMemoryInfo`) — 저용량 GPU 에서 풀 생성 실패 시 원인 불명
- [ ] UI 레이어 스레드 안전 (`Camera2D`, hover 상태) — 스칼라라 크래시는 아님

### 결정 보류
- [ ] `TestHost/` 저장소 포함 여부 — 부하/회귀 테스트 하네스로 실질 가치가 커짐
      (키: 크기/채널/8·16bit/오버레이 부하/화면밖/라이브급전/디바이스로스트)
- [ ] DPI 인식 — 사용자가 불필요로 판단, 보류


---

# 2026-08-25 세션 — 공개 API 재설계 (C++ / C ABI / C#)

## 완료

### 1단계 — C++ 공개 API 확장
- [x] `Camera2D` 프로그래머틱 뷰 제어 (`SetZoom`/`SetCenter`/`ZoomToRect`/`ImageToScreen`)
- [x] `D3D11RenderContext::SetBackgroundColor` / `SetVSyncEnabled`
- [x] `IROIObject` 에 조회 가상함수 5종 (`GetName`/`GetColorRGB`/`GetFontSize`/
      `GetShape`/`GetVertices`) — 4개 렌더러 전부 구현
- [x] `ROIShapeData` — 해석적 형상 union (계측 정확도 + 무손실 왕복)
- [x] `ROIRenderLayer` 조회 API + 이벤트 큐 (`ROIEvent` 6종, 락 밖 디스패치)
- [x] `D3D11ImageView_Impl_Query.cpp` 신규 — 조회/뷰제어/좌표변환/표시옵션 위임
- [x] 마우스 콜백 (`MouseEventData`, `Handled` 로 뷰어 선점 가능)
- [x] `D3D11ImageView` 공개 클래스에 위 전부 노출
      (내부 타입 비노출: `friend struct D3D11ImageViewCallbackBridge`)

### 2단계 — flat C ABI
- [x] `D3D11ImageViewC.h` / `.cpp` — 로직 없는 위임 계층
- [x] 도형 타입 열거형으로 오버로드 42종 → 함수 2개로 축약
- [x] 2회 호출 패턴(호출자 버퍼), `structSize` 버전 관리, 예외 격리
- [x] export 48개 전부 데코레이션 없는 `D3IV_*` 확인 (`dumpbin`)

### 3·4단계 — C# / WPF (저장소 미포함, 별도 관리)
- [x] `D3D11ImageView.cs` P/Invoke 래퍼, `ImageViewHost.cs` WPF HwndHost
- [x] WPF 최소 예제 + README

### 검증
- [x] Debug/Release x64 — 0 error / 0 warning
- [x] C ABI 실행 하네스 32항목 ALL PASS
- [x] C# 래퍼 실행 하네스 34항목 ALL PASS
      (유니코드 ROI 이름 왕복, `char[]` 출력 버퍼, `byte[]` 고정 업로드,
       합성 마우스 메시지로 콜백 도달, `Dispose` 후 예외)
- [x] 네이티브/관리 구조체 10종 크기 일치 (`sizeof` vs `Marshal.SizeOf`)
- [x] 공개 헤더 2종을 `/utf-8` 없이 `/W4` 컴파일 — 0 warning

### 이번에 잡은 버그
- [x] **`Initialize` 전 등록한 ROI 이벤트 핸들러가 조용히 버려짐**
      그 시점엔 `m_roiLayer` 가 없었다. C# 래퍼는 생성자에서 콜백부터 등록하므로
      항상 이 경로를 탄다(마우스는 오는데 ROI 만 0회). `Impl` 이 보관했다가
      레이어 생성 직후 붙이도록 수정
- [x] **공개 헤더에 BOM 이 없어 `/utf-8` 없이 컴파일하면 `error C2447`**
      외부 소비자가 바로 부딪힌다. 3개 헤더에 BOM 추가

## 미완료 — 다음 세션

- [ ] 오버레이 핸들 API — `D3IV_ImageOverlayHitTest` / `GetBounds` / `Remove` 는
      헤더에만 있고 `D3IV_ERR_UNSUPPORTED` 반환
- [ ] `Polyline2f` / `Polygon2f` 오버레이의 C ABI 경로 (힙 소유 타입이라 배열 불가)
- [x] `ImageViewHost` 실행 검증 — .NET SDK 9.0.317 설치 후 14항목 ALL PASS  *(2026-08-25)*
- [ ] `docs/active/plan.md` §6·§7 을 실제 구현 결과와 대조

---

## D3D11UIFramework 구조 정리 (2026-08-29)

- [x] 회귀 하네스 구축 — `Test/build.bat`, 렌더 컨텍스트 없이 39개 단언
- [x] #1 마우스 진입점 단일화 — `HandleMouseEvent` 제거, `OnMouseEvent` 가 `bool` 반환
- [x] #1-a `NotifyChildrenLeave` 가 중첩 손자에게 Leave 를 못 보내던 버그 (하네스가 검출)
- [x] #2 pimpl 제거 — `UIElementBaseImpl` / `UIPanelImpl`
- [x] #3 아이콘 컴포지션 — `UIIconHelper` → `UIIcon` 클래스
- [x] #5 `Update()` 반환값 의미 명시 + 순수 위임 override 2개 제거
- [x] #6 디바이스 리소스 진입점 단일화 — `AcquireDeviceResources(context, reset)`
- [x] #7 공개 헤더 자립성 — `header_selfcheck.bat` 로 15/15 검증
- [x] `SetStyle` 이 다음 상태 전이까지 반영 안 되던 버그 → `OnStyleChanged()` 훅
- [x] WPF 실측 검증 — 아이콘·hover(32→51)·ROI 라벨·거리 측정(2622.99px)·토글 리셋
- [~] #4 상속 재설계 (`UIButton : UILabel` → 컴포지션) — 위험 대비 이득 낮아 보류

## 미완료 — 다음 세션 (UI 프레임워크)

- [ ] `Test/` 를 `.vcxproj` 로 전환해 솔루션 빌드/CI 에 연결
- [ ] `GetStyle()` 비-const 참조가 `OnStyleChanged` 훅을 우회하는 경로 정리
- [ ] 자산 경로(`../Icons`, `../Shaders`) → DLL 리소스 임베드

## D3D11ImageView 기능 확장 (2026-08-29)

앞의 UI 프레임워크 정리에 이어, 같은 날 뷰어 기능을 붙였다. 커밋 6개.

### 컨텍스트 메뉴 / 저장 (`45462b9`, `df59a36`)

- [x] `UIContextMenuPanel` 계단식 하위 메뉴 — 탐색기처럼 hover 0.4초 또는 클릭으로 우측 전개
- [x] 하위 메뉴 라우팅을 패널 자신의 HitTest 보다 **먼저** 처리 (부모 항목 클릭 시 체인이 접히던 문제)
- [x] `Save image` → PNG / JPEG / BMP 하위 메뉴
- [x] `D3D11ImageIO` 재작성 — 채널·비트깊이별 픽셀 포맷, COM 폴백, 실패 시 파일 삭제
- [x] 저장은 **붙어 있는 원본** 이다. 줌·팬·ROI·오버레이가 결과에 안 들어간다
- [x] 픽셀 단위 일치 검증 (33/33)

### 표시용 LUT (`52b34b7`)

- [x] 자동 대비 — 퍼센타일 기반(0.1% 클립). min/max 는 핫픽셀 하나에 무너진다
- [x] 프리셋 5종 (Grayscale / Inverted / Hot / Viridis / Jet), 컨텍스트 메뉴 선택
- [x] 프리셋 재클릭 시 LUT 자체가 꺼지는 토글 (체크 = "지금 적용 중")
- [x] 컬러 이미지 제외, 저장 결과에는 영향 없음
- [x] 16bit 실측 — LUT OFF 2계조 → ON 173계조

### 아이콘 (`b7cda52` 이후)

- [x] SVG 로드 → D2D 직접 그리기 전환. `../Icons` 의존 제거
- [x] `SetIcon(path)` 는 옵션으로 남김
- [x] 토글 버튼 명암 반전 (흰 배경 + 검은 아이콘). 224/28 vs 32/32 — 예전 9계조 차이는 구분이 안 됐다

### 픽셀 값 격자 + 각도 측정 + 단축키 (`8177d11`)

- [x] `PixelGridRenderLayer` — 고배율에서 셀 경계와 값 표시
- [x] 격자선과 값이 **같은 임계**로 함께 등장 (예전엔 6배율/27배율로 따로 나와 어정쩡했다)
- [x] `ValueLabelAtlas` — 값 라벨을 텍스처에 구워 두고 잘라 쓰기
- [x] 폰트 크기 4단계 LOD. 단계 안에서는 축소만, 최대 16%
- [x] 8bit 256값 × 4단계 × 2색 사전 생성(~5MB) / 16bit 지연 생성
- [x] `ROIAngleRenderer` — 3클릭(첫 점 → 꼭짓점 → 둘째 점), 확정 후 핸들 드래그 가능
- [x] 각도는 내적으로 계산 — 바로 0~180 이라 부호/2π 감싸기 손볼 게 없다
- [x] 거리 측정과 상호 배타 (둘 다 좌클릭을 가로챈다)
- [x] `Ctrl +/-`, `Ctrl+1`, `Ctrl+0` 줌 단축키 + 클릭 시 포커스 획득
- [x] `.sln` 에 `D3D11ImageIO` 등록 — `vcxproj` 는 참조하는데 솔루션에 없어 새 클론이 깨졌다
- [x] `.vcxproj.filters` 누락 정리 — 디스크 72 / vcxproj 72 / filters 72 일치

### 성능 — 실측으로 뒤집힌 가설들

격자 버벅임을 잡는 과정이 이번 세션에서 가장 오래 걸렸고, **내 가설이 연달아 틀렸다.**

- [x] WIC 소프트웨어 타깃 벤치는 **경로가 달라 무의미했다** (셀당 1.3us vs 실제 9.4us, 7배)
- [x] 실제 원인은 하드웨어 D2D 의 `DrawTextLayout` 호출 비용 — 416셀에 5.98ms
- [x] `CLIP` + `SetMaxWidth` 가 그중 2.1ms. 브러시 교체는 무관했다
- [x] Release 도 동일 → D3D 디버그 레이어 탓이 아니다
- [x] 값별 비트맵 캐시로 12배 단축. 단, **생성이 개당 0.22ms** 라 줌마다 다시 구우면 도로아미타불
- [x] 4단계 LOD 로 무효화 자체를 제거 → 줌해도 `baked` 가 2048 고정
- [x] 해시맵 → 값 직접 첨자. Prepare 0.415 → 0.012ms

| 단계 | Prepare | Render |
|---|---|---|
| 최초 (DrawTextLayout) | — | 5.98ms / 416셀 |
| 값별 비트맵 | 0.415ms | 1.550ms / 1815셀 |
| 아틀라스 + LOD | **0.012ms** | **1.048ms / 1363셀** |

- [x] 임계를 42 → 25.5px/셀로 **내렸다** (줌 버튼 3.3스텝 이르게). 비용이 줄어 가능해진 것

### 이번에 잡은 함정 (D2D)

- [x] 프레임의 `BeginDraw` 안에서 오프스크린 타깃에 그리면 **내용이 빈 채로 성공한다**
      → 굽기를 `Prepare` 로 이동. 그런데 `m_pixelGridLayer->Prepare()` 가 **아예 호출되지 않고 있었다**
- [x] 오프스크린에 `DrawTextLayout` 은 **HRESULT 전부 S_OK 인데 아무것도 안 그려진다**
      같은 자리에서 `Clear`/`FillRectangle` 은 정상, `DrawText` 로 바꾸면 나온다
- [x] `FillOpacityMask` 경로는 앱을 죽였다 → 값이 색을 결정하므로 색을 비트맵에 굽는 쪽으로 우회

### 검증

- [x] 회귀 하네스 4종 — UIFramework 74/74, 헤더 자립성 17/17, ImageIO 42/42, LUT 48/48
- [x] 각도 실측 — 90.00° / 135.00° / 45.00° (드래그 중 실시간 갱신)
- [x] 격자 값 가독성 — 25.5px/셀부터, 대비 105→흰색 / 134·149→검정
- [x] 8bit·16bit 양쪽 확인 (16bit 5자리 `11308` / `34695`)
- [x] **커밋 상태를 새로 클론해 Release 빌드** — 작업 트리와 `.sln` 이 다르므로 로컬 빌드는 증거가 못 된다

## 미완료 — 다음 세션 (뷰어)

- [ ] **각도에 픽셀 스케일 미적용** — X/Y 배율이 다르면(라인스캔) 화면 각과 실제 각이 어긋난다.
      두 변 벡터에 스케일을 먹여야 하는데 실장비 없이는 검증 불가
- [ ] **LUT 켜짐 상태에서 격자 글자 대비가 어긋난다** — 원본 값으로 판정하므로 `Inverted` 는 정확히 반대.
      `WantsDarkText()` 한 곳만 고치면 되고, 아틀라스가 두 색을 다 들고 있어 준비는 끝났다
- [ ] `PixelGridRenderLayer` / `ValueLabelAtlas` / `ROIAngleRenderer` 회귀 하네스 없음 —
      전부 실앱 계측으로만 검증했다
- [ ] 사용자 보류: ② 히스토그램 + 수동 window/level, ④ 라인 프로파일, ⑦ 화면 저장/클립보드
- [ ] C ABI / C# 바인딩 — 사용자 요청으로 중지 상태
