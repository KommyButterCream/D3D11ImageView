# 타일링/LOD 리팩터링 변경 내역

작성일: 2026-08-21 (최종 갱신 2026-08-23)
상태: **코드 수정 완료, 커밋 안 함.** Debug/Release(x64) 모두 경고 0으로 빌드됨.

> 2026-08-23: **10단계 타일 워커 스레드 풀은 롤백됨.** 시기상조라는 판단.
> 타일 생산은 다시 렌더 스레드 동기 방식(memcpy)이다. 자세한 내용은 §9.
> 다만 그 과정에서 드러난 `Attach` 수명 결함을 고친 **`DetachImage()` API 는 유지**한다.

---

## 1. 목표

30만px급 이미지까지 대응하면서, 작은 이미지는 더 단순한 경로로 처리한다.
GLViewer_2DEngine 분석에서 확인한 핵심 원인은 **maxLOD 하드코딩**이었다.

| | 이전 | 이후 |
|---|---|---|
| maxLOD | `4` 고정 | `ceil(log2(imageMax / viewMin))` |
| LOD4 타일이 덮는 원본 | 512 × 2⁴ = 8,192 px | — |
| 30만px fit 줌에 필요한 타일 | **1,369 장** (풀 용량 20칸) | **9~25 장** |
| 결과 | 영구 스래싱, 수렴 안 함 | 정상 |

두 프로젝트의 샘플링 방식은 동일했다(둘 다 stride 점 샘플링). 차이는 **읽는 양이 38배**였다.

---

## 2. 검증 상태

### 빌드
- MSBuild (VS 2022 Professional), x64 Debug/Release **경고 0**
- 산출물: `x64\Debug\D3D11ImageView.dll`, `TestHost.exe`

### 런타임 (TestHost 로 실제 실행 확인)

| 입력 | 판정 | 결과 |
|---|---|---|
| 1024² Gray | Single | 정상 |
| 8192² Gray | **Single** | R8 전환으로 승격됨 (이전엔 BGRA 358MB → Tiled) |
| 16300² Gray (253 MB) | Tiled | 정상, 크래시 없음 |
| 16300² BGRA (1,013 MB) | Tiled | 정상 |
| 30000×2000 Gray | Tiled | 변 초과 판정 정상 |
| 40000² Gray (1,526 MB) | Tiled | 정상 |
| 40000² → 1024² 복귀 | Tiled → Single | RAM 1,589 → 64 MiB (리소스 해제 확인) |

### 검증되지 않은 것 (반드시 육안 확인 필요)
- 밉맵이 실제로 축소 지글거림을 없앴는지
- 타일 경계 이음새 유무
- 색/채널 순서 (R8 스위즐, BGR 확장, BGRA 알파)
- 프라이밍이 "한 번에 나타나는" 체감을 주는지
- ROI/오버레이 좌표 정렬 (특히 아래 §5 의 반픽셀 오프셋)
- **30만px 실측** — 90GB RAM 필요, TestHost 로 불가

---

## 3. 변경 파일 목록

### D3D11ImageView (주 저장소)

| 파일 | 구분 | 내용 |
|---|---|---|
| `Image Tile/TileFormat.h` | **신규** | 포맷 결정 + Single/Tiled 판정 |
| `Image Tile/Tile.h` | 수정 | `UploadMode` 제거, `TileState` 2상태로 축약 |
| `Image Tile/TileManager.h/.cpp` | 수정 | 전면 재작성 (§4 참조) |
| `Image Tile/TilePool.h/.cpp` | 수정 | 포맷 파라미터화, UAV 제거, 축출 보호 |
| `Render Layer/ImageRenderLayer.h/.cpp` | 수정 | 밉맵, 샘플러 분리, 판정 도입, gray PS |
| `Overlay Renderer/OverlayTypes.h` | 수정 | 미사용 변수 제거 (C4189) |
| `D3D11ImageView/D3D11ImageView_Impl.cpp` | 수정 | 하드코딩 `TileSystemDesc` 삭제 |
| `D3D11ImageView/D3D11ImageView.vcxproj` | 수정 | `TileFormat.h` 등록 |
| `D3D11ImageView.sln` | 수정 | `TestHost` 프로젝트 등록 |
| `TestHost/` | **신규** | 테스트 호스트 앱 (§6) |
| `Shaders/*.cso` | 빌드 산출물 | 추적 여부 확인 필요 |

### D3D11Engine (공유 저장소 — 허가받아 수정)

| 파일 | 구분 | 내용 |
|---|---|---|
| `Shader/ImageGrayPS.hlsl` | **신규** | 단일 채널(R8/R16) 전용 픽셀 셰이더 |
| `Shader/ImagePS.hlsl` | 수정 | 알파 1.0 고정 + 깨진 주석 복구 |
| `Shader/TileSamplingCS.hlsl` | **삭제** | 타일 CS 경로 폐지 |
| `D3D11Engine/D3D11Engine.vcxproj` | 수정 | FxCompile 항목 추가/제거 |
| `Camera/Camera2D.h/.cpp` | 수정 | `IsSettled()`, 동적 `GetMinZoom()` |

### D3D11UIFramework (부수 영향 — 확인 필요)

| 파일 | 구분 | 비고 |
|---|---|---|
| `Shaders/TileSamplingCS.cso` | 삭제 | 폐지된 셰이더의 빌드 산출물. **이 저장소에서 .cso 가 git 추적 중임** — 의도한 것인지 확인 필요 |

---

## 4. 단계별 상세

### 0단계 — 테스트 호스트

`TestHost/` Win32 앱을 솔루션에 추가. 산출물이 DLL 과 같은 `x64\Debug\` 에 생성된다.

- 합성 이미지: 체커보드(에일리어싱) + 그라디언트(채널 순서) + 512px 격자선(타일 경계/좌표 정렬) + 1024px 간격 1픽셀 점(밉 필터가 미세 결함을 씻는지)
- 크기 프리셋 6종, 채널 3종(Gray/BGR/BGRA)
- 셰이더는 런타임에 `../Shaders/*.cso` 로 로드되므로 시작 시 작업 디렉터리를 `<sln>\x64` 로 맞춘다

### 1단계 — Single 모드 밉맵 / 샘플러 / raw 버퍼

`ImageRenderLayer`

- `CreateSingleBuffer`: `MipLevels = 1` → `0`(전체 체인), `BIND_RENDER_TARGET` 추가, `MiscFlags = GENERATE_MIPS`
- 업로드 직후 `GenerateSingleMips()` 호출 (raw/텍스처/공유텍스처 3개 경로 모두)
- **샘플러를 2개로 분리**
  - `m_samplerLinearMip` (Single): `MIN_LINEAR_MAG_POINT_MIP_LINEAR` — 축소는 부드럽게, 확대는 픽셀 경계 보존
  - `m_samplerPoint` (Tiled): `MIN_MAG_MIP_POINT` 유지
  - **분리 이유**: 타일 텍스처는 `MipLevels=1` 이고 배열 슬라이스 경계에서 CLAMP 되므로, LINEAR 축소를 걸면 타일마다 테두리 텍셀이 번져 **이음새가 보인다**
- `CreateRawUploadBuffer(8192*8192*4)` = **268MB 무조건 선할당** 제거 → `UpdateImage` 에서 실제 크기(`w*h*channel`)로 지연 생성

**부수 발견 (버그 수정)**: `m_maxByteSize` 가 어디에서도 대입되지 않아 early-return 이 동작하지 않았고, **업로드마다 버퍼를 재생성**하고 있었다. 대입 추가 + `>=` 비교로 변경.

### 2·3단계 — Single/Tiled 판정 + R8/R16 포맷

포맷 결정과 판정이 서로 얽혀 있어 함께 진행.

`Image Tile/TileFormat.h` (신규)

```
kMaxTextureDim            = 16384            // FL11 변 단위 하드 한계
kSingleResidentBudgetBytes = 128 MB          // 뷰어 1개당 전량 상주 예산

ResolveTextureFormat(channel, bitDepth)
    1ch  8bit  -> R8_UNORM
    1ch 16bit  -> R16_UNORM
    3ch/4ch    -> B8G8R8A8_UNORM   (24bit DXGI 포맷이 없어 3ch 은 확장 필수)

CanUseSingleTexture(w, h, format, views)
    ① 변 > 16384 -> false                     (면적이 아니라 변 단위!)
    ② w*h*bpp*4/3 > 예산/views -> false        (4/3 = 밉 체인 오버헤드)
```

`ImageRenderLayer::UpdateImage`
- `width > 8192 || height > 8192` 판정을 `CanUseSingleTexture` 로 교체
- Gray/BGRA 는 `UpdateSubresource` 직행 (포맷 일치 → 변환 불필요)
- BGR 만 `SingleConvertCS` 로 확장
- `ReleaseUnusedModeResources()` 추가 — 모드 전환 시 반대쪽 해제 (피크 VRAM 2배 방지)
- `UpdateTexture` / `UpdateSharedTexture` 의 `> 8192` 제한을 `kMaxTextureDim` 으로 완화

`ImageGrayPS.hlsl` (신규): 단일 채널은 `Sample()` 이 `(r,0,0,1)` 을 주므로 `.r` 을 3채널로 복제. `SetCommonShaderStates()` 가 활성 포맷에 따라 PS 를 선택.

**효과 (실측)**: 8192² Gray 가 Single 로 승격됨 (85MB ≤ 128MB). 이전 BGRA 기준으로는 358MB 로 Tiled 였다.

### 4단계 — 타일 컴퓨트 셰이더 경로 제거

제거 이유: **GPU 는 이미 GPU 에 있는 데이터만 샘플링할 수 있다.** 고LOD 타일의 소스 범위(`tileSize × 2^L`)는 어떤 스테이징 윈도우로도 담을 수 없다. 실제로 `m_gpuUploadTextureSize = 4096` 이라 `lod <= 3` 게이트가 걸려 있었고, **정작 도움이 필요한 LOD4 이상에서는 비활성**이었다.

삭제: `UploadMode`, `UploadTileData_GPU`, `InitializeGPUResources`/`ReleaseGPUResources`,
`m_rawUploadBuffer`/`SRV`/`m_rawUploadtileCS`/`m_csConstantBuffer`, `CachedRegion`,
`m_currentGpuCache`, `m_gpuUploadTextureSize`, `lod<=3` 게이트,
`TilePool::m_uavArray` / `BIND_UNORDERED_ACCESS`, `TileSamplingCS.hlsl`

유지: `SingleConvertCS` — Single 모드 BGR 확장용. 이미지 전체 1회 변환이라 CS 가 맞는 도구.

**부수 효과**: UAV 가 사라져 R8/R16 의 **typed UAV store 지원 확인 문제가 소멸**했다. SRV 만 필요하므로 포맷 전환이 무조건 안전.

### 5단계 — maxLOD + 용량 공식, 풀 생성 시점 이동 ★

`D3D11ImageView_Impl.cpp` 의 하드코딩 삭제:
```
maxLOD = 4, lods = {512,200},{512,100},{512,50},{512,30},{512,20}
```

`TileManager` API 변경:
- `Initialize(device, context)` — 디바이스만 보관
- **`Configure(imageW, imageH, channel, bitDepth, viewW, viewH)`** — 이미지마다 호출, 풀 재구성
- `NeedsReconfigure(viewW, viewH)` — 뷰포트 변화가 작업세트/maxLOD 를 바꿀 때만 true

공식:
```
maxLOD    = ceil(log2(imageMax / viewMin))         // GLViewer CalcMaxLODLevel 과 동일 정의
gridW     = ceil(viewW / 512) + 1
gridH     = ceil(viewH / 512) + 1
작업세트  = gridW * gridH
용량[L]   = min(전체타일수[L], 작업세트 * 2)
```

`min(whole, ...)` 이 **maxLOD 를 자동으로 전량 상주**시킨다. maxLOD 정의상 그 레벨의 전체 타일 수가 작업세트 이하로 보장되기 때문이다 → 부모 fallback 이 항상 성공한다.

`Render()` 안에서 `NeedsReconfigure` 검사 후 재구성. 창을 몇 픽셀 끄는 것으로는 재생성하지 않는다.

### 6단계 — maxLOD 프라이밍 + 렌더 게이트

- `PrimeCoarsestLevel()`: maxLOD 레벨 **전체를 예산 무시하고** 한 번에 채움. `UpdateImage` / 재구성 직후 호출
- `RenderTiled()` 진입부에서 `IsPrimed()` 가 false 면 이미지를 그리지 않음
- 결과: 타일이 하나씩 채워지는 과정이 노출되지 않고, 준비되면 한 번에 나타난다. GLViewer 가 캐시 미준비 시 `SwapBuffers` 를 생략하는 것과 같은 효과이면서, **카메라 행렬을 낡은 것으로 되돌리지 않아 위치 밀림이 없다** (GLViewer 의 `m_bPreCameraSet` 이 꺼져 있는 이유)
- 프라이밍 이후 fallback 실패 경로(`continue` = 구멍)가 원리적으로 사라짐

### 7단계 — 시간 예산 / 모션 게이팅 / 프리페치 / 로드 순서

- `MAX_UPLOAD_PER_FRAME = 4` → **시간 예산 2ms** (`HighResolutionTimer`). LOD0 memcpy 와 고LOD 스트라이드는 비용이 수십 배 차이나 장수로는 프레임 시간을 예측할 수 없었다
- **모션 게이팅**: `Camera2D::IsSettled()` 가 false 면 신규 타일 생성 보류, 상주분(부모 fallback 포함)으로만 렌더. 드래그 중 스치는 타일은 업로드가 끝나기 전에 화면을 벗어나므로 그 비용이 순수 낭비
- **프리페치 마진**: `CalcVisibleKeys` 전에 뷰 rect 를 한 타일만큼 확장 (`GetViewImageRect()` 는 마진 0)
- **로드 순서**: 키를 뷰포트 중심 거리로 정렬. GLViewer 의 `ReUseCheck_SingleBuffer_WorkOrder`(이동 방향 우선) 대응이면서 더 단순

### 8단계 — 축출 보호 / Evict 복구 / 상태 머신

- `EvictLRU(frameID)`: `lastFrameUsed == frameID` 인 타일 **스킵**. 이전 구현은 `m_usedList.back()` 을 무조건 가져갔고, `TileManager` 가 이전 프레임 타일들의 `lastFrameUsed` 를 갱신하면서 usedList 순서와 어긋나 **현재 프레임 가시 타일이 축출**될 수 있었다
- 주석 처리되어 있던 `Evict()` 정기 호출 복구 (임계값 600 프레임). maxLOD 풀은 전량 상주가 목적이라 제외
- `m_previousVisibleTiles` 갱신을 `frameID - 1` 로 변경 (현재 프레임 보호와 충돌 방지)
- `TileState`: `None`/`Ready`/`Inactive`/`Active` → **`None`/`Resident`** 로 축약. `Ready` 는 즉시 `Active` 가 되고 `Inactive` 는 미사용이었다
- `FindAvailableParent` 가 `Resident` 검사
- `Evict()` 의 `frameID - frameThreshold` 언더플로 방어 추가

### 9단계 — RAM 피라미드: **미착수 (의도적 보류)**

적용 조건은 이미지 최대 변 > 약 20,480px (그때부터 maxLOD ≥ 3, stride ≥ 8). GLViewer 가 피라미드 없이 실전 사용 중이므로, 5~8단계 효과를 실측한 뒤 필요성을 재평가하는 것이 맞다.

---

## 5. 계획에 없었지만 함께 고친 것

작업 중 발견한 것들. 모두 위 목표와 직접 관련이 있어 함께 처리했다.

| 항목 | 위치 | 내용 |
|---|---|---|
| **`minZoom` 이 큰 이미지에서 fit 을 막음** | `Camera2D` | `minZoom = 0.05f` 고정이라 40000px 이미지(fit zoom 0.027)에서 휠을 굴리면 화면이 튀고 fit 으로 복귀 불가. `GetMinZoom()` = `min(0.05, fitZoom)` 으로 변경 |
| **이미지 알파가 백버퍼로 누출** | `ImagePS.hlsl` | `return color;` 가 4채널 소스의 알파를 그대로 기록. D2D 타깃은 `PREMULTIPLIED` 라 그 위 오버레이 블렌딩이 틀어진다. `float4(color.rgb, 1.0f)` 로 고정 |
| `m_maxByteSize` 미대입 | `ImageRenderLayer` | early-return 무효 → 업로드마다 raw 버퍼 재생성 |
| `ImagePS.hlsl` 주석 인코딩 파손 | `ImagePS.hlsl` | CP949 로 저장돼 깨져 있던 주석을 UTF-8 로 복구 |
| 미사용 지역변수 (C4189) | `OverlayTypes.h` | `UpdateD2DColors()` 의 `inv` |
| 와이어프레임 기본 ON | `ImageRenderLayer.h` | `m_renderWireFrame = true` → `false`. 디버그용인데 매 프레임 드로우가 2배 |
| `IsImageRenderDirty()` null 역참조 | `ImageRenderLayer` | Single 모드/미구성 상태에서 `m_tileManager` 무조건 역참조 |
| `CalcVisibleKeys` 음수 방어 | `TileManager` | `int32_t` → `uint32_t` 변환 시 음수가 거대값이 되는 것 방어 (현재 `GetViewImageRect` 가 clamp 하지만 계약이 바뀔 경우 대비) |

---

## 6. 인코딩 예외 사항

프로젝트 규칙은 "모든 파일 UTF-8 with BOM" 이지만, **`.hlsl` 은 BOM 을 넣으면 FXC 가 거부한다**(`error X3000: Illegal character in shader file`).

→ `.hlsl` 은 **BOM 없는 UTF-8** 로 저장했다. 한글 주석은 정상 동작한다.

---

## 7. 남은 확인 사항

### 반드시 육안 확인
1. **ImageSpace 반픽셀 오프셋** — 오버레이/ROI 렌더러 7개 파일이 `+ 0.5f` 를 이미지 좌표에 더한다. WindowSpace(scale=1)에서는 맞지만 ImageSpace 에서는 `0.5 × zoom` 화면 픽셀이 밀린다(zoom 8 이면 4px). `strokeWidth` 는 `/ scale` 로 보정하는데 좌표는 안 하고 있어 의도된 것으로 보이지 않는다. **zoom 8 에서 ROI 경계가 의도한 픽셀에 맞는지 보면 즉시 판별된다**
2. 타일 경계 이음새 (TestHost 의 512px 격자선으로 확인)
3. 색/채널 순서 (Gray/BGR/BGRA 전환하며 확인)
4. 밉 필터가 1픽셀 결함을 씻어버리는지 — 검사 용도라면 max 필터 밉 체인이 필요할 수 있다(도메인 판단)

### 저장소 정리 판단 필요
- `D3D11UIFramework/Shaders/TileSamplingCS.cso` 삭제됨. 이 저장소가 `.cso` 를 git 추적 중인데 의도한 것인지 확인
- `D3D11ImageView/Shaders/`, `D3D11Engine/D3D11Engine/Shaders/` 가 untracked 로 남아 있음 — `.gitignore` 정리 여부 판단

### 후속 작업
- 9단계 RAM 피라미드 (20,480px 초과 시)
- 워커 스레드 + 스테이징 풀 + epoch 취소 (피라미드 도입 후 필요성 재평가)
- 오버레이 `ID2D1PathGeometry` 프레임마다 재생성 → 렌더러에 캐시
- 동영상 용도라면 `syncInterval = 1` 모드 분기 (현재 0, 티어링)

---

## 9. 10단계 타일 워커 스레드 풀 — **구현 후 롤백** (2026-08-23)

### 결정

워커 스레드 풀을 구현해 동작까지 확인했으나, **시기상조로 판단해 되돌렸다.**
타일 생산은 다시 렌더 스레드에서 동기 `memcpy` 방식으로 동작한다.

되돌린 이유:

- 현재 목표(16300²급 정지 영상)에서는 이득이 얇다. 프라이밍 1회 ~10ms → ~4ms 수준
- 축출 보호·취소·수명 3개 축이 모두 정확해야 하는 **리스크 최고 구간**이었다
- 앞선 5~8단계가 아직 **육안 검증 전**이라, 미검증 코드 위에 고위험 변경을 쌓는 형국이었다
- 값어치는 라이브 뷰/스트리밍 또는 30000px 이상에서 나온다. 그 요구가 실제로 생길 때 다시 올리면 된다

### 제거된 것

| 파일/항목 | 처리 |
|---|---|
| `Image Tile/TileWorkerPool.h/.cpp` | **삭제** |
| `TileManager::StartWorkers/StopWorkers/DrainWorkers/GetWorkerCount` | 제거 |
| `TileManager::RequestTileAsync/UploadCompletedTiles/ParallelForRows` | 제거 |
| `TileState::Pending` | 제거 (다시 `None`/`Resident` 2상태) |
| `TilePool` 의 `Pending` 축출 보호 | 제거 |
| `Impl::Initialize` 의 `StartWorkers()` | 제거 |
| `ApplyPendingImageUpdate` 의 `DrainWorkers()` | 제거 |
| Single 3채널 `ParallelForRows` | 제거 (단일 스레드 행 memcpy 로 복귀) |
| vcxproj 등록 | 해제 |

`ProduceTileSync` 는 `ProduceTile` 로 이름을 되돌렸다.

### 유지한 것

**`Image Tile/TileSampler.h`** — 타일 샘플링을 순수 함수로 분리한 헤더.
스레드와 무관한 단순 추출이고, `TileManager` 에서 150줄이 빠져 읽기 쉬워졌다.
경계 조건부 `clear` 최적화도 여기 들어 있다. 되돌리길 원하시면 `TileManager::ProduceTile`
안으로 인라인할 수 있다.

**`DetachImage()` API** — 워커 풀 작업 중 발견한 **실제 결함**의 수정이라 유지한다.

```
UpdateImage 는 버퍼를 복사하지 않고 포인터를 빌려간다(ImageBase::Attach).
  -> 렌더 스레드가 프레임 중에 그 메모리를 읽는다
  -> 호출자는 "언제까지 살려둬야 하는지" 알 방법이 없었다
```

워커가 없어도 이 위험은 남는다(창은 좁아짐). `DetachImage()` 는

1. `m_renderLock` 획득 → 렌더 스레드가 `Render()` 밖임을 보장
2. 대기 중인 이미지 업데이트 폐기 (그 안의 `rawData` 도 호출자 버퍼)
3. 풀 해제 + `ImageBase::ReleaseBuffer()`

를 수행해, 반환 이후 뷰어가 그 메모리를 읽지 않음을 보장한다.

호스트 사용 패턴:

```cpp
viewer->DetachImage();     // 뷰어 참조 끊기
buffer.Release();          // 이제 안전
buffer.Build(...);
viewer->UpdateImage(buffer.Data(), ...);
```

`TestHost` 가 이 패턴을 쓴다. 원래는 `Release()` 를 먼저 호출해 **0xC0000005 로 죽었다.**
(워커가 아니라 이 수명 위반이 원인이었다)

### 롤백 후 검증

| | 결과 |
|---|---|
| Debug / Release (x64) 빌드 | 경고 0 |
| 워커 잔재 전수 검색 | 없음 |
| Debug 동작 (크기·채널 9회 전환) | OK |
| Release 동작 (동일) | OK |

### 다시 올릴 때

설계는 검증됐으므로 그대로 쓸 수 있다. 핵심 결정 7개:

| # | 결정 |
|---|---|
| D1 | 타일 단위 병렬 (타일 내부 행 분할 아님) |
| D2 | 스테이징 버퍼 풀 = 병렬성 + 메모리 상한 노브 (16개) |
| D3 | `TileState::Pending` — 축출 금지 + 렌더 불가 |
| D4 | 프라이밍은 배리어 (동작 변화 없이 순수 속도 이득) |
| D5 | epoch 카운터로 스테일 요청 폐기 |
| D6 | 워커 수 `clamp(hw/4, 1, 4)` — DLL 이라 보수적으로 |
| D7 | 동기 경로를 폴백으로 상시 유지 |

수명 보호가 필요한 지점 3곳: `ApplyPendingImageUpdate`, `TileManager::Configure`, `DetachImage`.

전제 조건: **5~8단계 육안 검증 완료 + 라이브 뷰/스트리밍 요구 확정.**

---
## 10. 커밋 안 함

사용자 지시에 따라 **커밋하지 않았다.** 모든 변경은 작업 트리에만 존재한다.
`§3` 의 파일 목록으로 검토 후 커밋 단위를 나누는 것을 권한다. 단계별로 나누면
`1단계` / `2·3·4단계` / `5·6단계` / `7·8단계` / `§5 부수 수정` / `TestHost` 정도가 자연스럽다.
