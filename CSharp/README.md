# D3D11ImageView — C# / WPF 바인딩

## 파일

| 파일 | 역할 |
|------|------|
| `D3D11ImageView.cs` | `D3D11ImageViewC.h` 의 flat C ABI 를 P/Invoke 하는 래퍼. WinForms/Win32 는 이것만 있으면 된다. |
| `ImageViewHost.cs` | WPF `HwndHost` 컨트롤. XAML 에 바로 올릴 수 있다. |
| `Example/` | WPF 최소 예제 (이미지 표시, 결함 오버레이, ROI 편집/조회, 마우스 좌표). |

두 파일을 프로젝트에 그대로 넣어도 되고, 별도 어셈블리로 빼서 참조해도 된다.
예제 `.csproj` 는 `<Compile Include="..\*.cs" />` 로 직접 포함하는 쪽을 쓴다.

---

## 필수 조건

1. **x64 고정.** `AnyCPU` 로 두면 32bit 프로세스로 떠서
   `D3D11ImageView.dll`(x64) 로드 시 `BadImageFormatException` 이 난다.
   ```xml
   <PlatformTarget>x64</PlatformTarget>
   ```
2. `D3D11ImageView.dll` 이 실행 파일과 같은 폴더에 있어야 한다.
3. **셰이더는 실행 파일이 아니라 작업 디렉터리(CWD)의 _상위_ 폴더 기준이다.**
   뷰어는 `L"../Shaders/ImageVS.cso"` 처럼 읽으므로, CWD 의 부모에
   `Shaders/` 가 있어야 `Initialize` 가 성공한다. 없으면 아무 로그 없이
   `D3IV_ERR_FAILED` 만 돌아온다.

   예: 실행 파일이 `bin\x64\Debug\app.exe` 이고 CWD 가 `bin\x64\Debug` 라면
   `bin\x64\Shaders\` 를 찾는다. 필요한 파일은
   `ImageVS.cso`, `ImagePS.cso`, `ImageGrayPS.cso`, `WireFramePS.cso`,
   `SingleConvertCS.cso` 다.

   이 상대 경로 규칙은 기술 부채로 기록되어 있다. 배포 시에는 CWD 를
   고정하거나 빌드 후 복사 규칙으로 맞춰 둘 것.

---

## Win32 / MFC 는 기존 방식 그대로

C++ 소비자는 `D3D11ImageView.h` 의 `D3D11ImageView` 클래스를 계속 쓰면 된다.
C ABI 는 같은 DLL 안의 얇은 위임 계층이며, 두 경로가 같은 구현을 공유한다.
C++ 쪽에도 이번에 추가된 기능(뷰 제어, 좌표 변환, ROI 조회, 마우스/ROI 콜백)이
전부 노출되어 있다.

---

## WPF 사용법

```xml
<Window xmlns:viewer="clr-namespace:D3D11ImageViewInterop">
    <Grid>
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto" />
            <RowDefinition Height="*" />
        </Grid.RowDefinitions>

        <!-- 호스트 UI 는 뷰어 "바깥" 에 -->
        <StackPanel Grid.Row="0" Orientation="Horizontal">
            <Button Content="Fit" Click="OnFit" />
        </StackPanel>

        <viewer:ImageViewHost Grid.Row="1" x:Name="Viewer"
                              ViewerCreated="OnViewerCreated" />
    </Grid>
</Window>
```

```csharp
private void OnViewerCreated(object sender, EventArgs e)
{
    // 창이 만들어진 뒤에야 뷰어가 존재한다. 생성자에서 하면 안 된다.
    Viewer.Viewer.SetBackgroundColor(D3D11ImageView.Rgb(32, 32, 32));
}
```

### Airspace — WPF + HWND 의 근본 제약

`HwndHost` 안의 D3D11 자식 창은 WPF 렌더 트리 **밖**에서 그려진다.
따라서 뷰어 영역 위에는

- `Popup` / `ToolTip` / `ContextMenu` / `Adorner`
- `Opacity`, `RenderTransform`, `Clip`, `Effect`

가 **먹지 않는다.** 뷰어 위에 놓은 WPF 요소는 가려진다.
해결책은 두 가지뿐이다.

1. 뷰어 내장 오버레이 / ROI / 툴바 / 상태바를 쓴다 ← 권장
2. 호스트 UI 를 뷰어 바깥(위·아래·옆)에 배치한다

`ImageViewHost` 는 그래서 내장 툴바/상태바를 기본 `true` 로 둔다.
호스트가 자기 UI 로 대체하려면 `ToolbarVisible="False" StatusBarVisible="False"`.

### DPI

`HwndHost` 가 레이아웃 크기를 장치 픽셀로 변환해 자식 창을 배치하므로
크기 계산은 손댈 필요가 없다. 다만 뷰어 콜백이 주는 `ScreenX/Y` 는
**장치 픽셀**이다. WPF DIP 와 섞을 때는 `ImageViewHost.DeviceToDip()` 을 쓴다.

---

## 두 가지 수명 계약

### 1. 델리게이트

네이티브에 넘긴 델리게이트는 GC 루트가 아니다. 필드로 붙들지 않으면 수거된 뒤
다음 콜백에서 프로세스가 죽는다. `D3D11ImageView` 가 내부에서 트램폴린을
`readonly` 필드로 잡고 있으므로 **래퍼를 필드로 들고 있기만 하면** 된다.

### 2. 이미지 버퍼

`UpdateImage` 는 버퍼를 **복사하지 않고 빌려 쓴다.** 렌더 스레드와 타일 워커가
비동기로 읽는다.

- `byte[]` 오버로드: 래퍼가 `GCHandle.Alloc(..., Pinned)` 로 고정하고
  다음 `UpdateImage` / `DetachImage` / `Dispose` 에서 해제한다.
  **배열 참조 자체는 호스트가 유지해야 한다** (지역 변수로 두지 말 것).
- `IntPtr` 오버로드: 수명 관리는 전부 호출자 책임이다.
  카메라 SDK 버퍼를 반납하기 전에 `DetachImage()` 를 부를 것.

---

## ROI 조회 — 해석적 vs 정점

| 용도 | 메서드 | 이유 |
|------|--------|------|
| 계측 (면적·지름) | `TryGetRoiShape` | 원은 `pi*r^2` 이지 다각형 근사 면적이 아니다. 무손실 왕복도 이쪽만 된다. |
| 마스킹 / 픽셀 순회 | `GetRoiVertices` | 곡선은 `segmentsPerCurve` 등분 근사. |

편집 결과 확정은 `RoiEvent` 의 `EditEnd` 에서 읽는다.
`EditChanged` 는 드래그 중 매 프레임 온다.

---

## 마우스 콜백 선점

`ViewerMouseEventArgs.Handled = true` 로 두면 뷰어는 그 이벤트를 처리하지 않는다
(팬·줌·ROI 편집 전부 건너뜀). 호출 시점은 **뷰어 내장 UI 처리 다음, ROI 처리
앞**이다. 즉 툴바 클릭은 콜백까지 오지 않고, 호스트는 ROI 편집을 선점할 수 있다.

콜백은 ROI 락 밖에서 호출되므로 핸들러 안에서 다른 API 를 불러도 데드락이 없다.

---

## 이 환경에서 검증한 것 / 못 한 것

**검증함**

- `D3D11ImageView.cs`, `ImageViewHost.cs`, `Example/*.cs` — Roslyn `csc`
  (`-warn:4`, `-langversion:5`) 로 0 error / 0 warning 컴파일.
  `langversion:5` 로 통과했으므로 .NET Framework 4.x 와 .NET 6/8 양쪽에서 쓸 수 있다.
- 네이티브/관리 구조체 크기 10종 일치 확인 (`cl` 로 찍은 `sizeof` 와
  `Marshal.SizeOf` 비교):
  `OverlayStyle 20`, `ROIShape 32`, `ROIInfo 32`, `MouseEvent 40`,
  `Rect2f/Rect2i 16`, `Point2f 8`, `Circle2f 12`, `Ellipse2f 20`, `ColorRGBA8 4`.
- 한글 문자열 리터럴이 IL 에 정상 인코딩됨.
- 공개 헤더 두 개(`D3D11ImageView.h`, `D3D11ImageViewC.h`)를
  `/utf-8` **없이** `/W4` 로 컴파일 — 0 warning.
- DLL export 48개가 데코레이션 없는 `D3IV_*` 심볼로 나감.
- **C ABI 실행 테스트 (C++ 하네스, 32항목 전부 통과)** — 창 생성, 이미지 업로드,
  픽셀값, ROI 설정/조회 전종, 2회 호출 패턴, 히트 테스트, 뷰 제어, 좌표 왕복,
  오버레이, 잘못된 인자/핸들 거절, 제거, 파괴.
- **C# 래퍼 실행 테스트 (콘솔 하네스, 34항목 전부 통과)** — 위 항목 전체에 더해
  유니코드 ROI 이름 왕복(`검사영역`, `원형게이지`), `char[]` 출력 버퍼,
  `byte[]` 고정 후 업로드, 합성 마우스 메시지로 마우스/ROI 이벤트 콜백 도달 확인,
  `Dispose` 후 `ObjectDisposedException`.

  이 테스트에서 실제 버그 하나를 잡았다. `Initialize` **전에** 등록한 ROI 이벤트
  핸들러가 조용히 버려지고 있었다(`m_roiLayer` 가 아직 없었다). C# 래퍼는 생성자에서
  콜백부터 등록하므로 항상 이 경로를 탄다. `D3D11ImageView_Impl` 이 핸들러를 보관했다가
  레이어 생성 직후 붙이도록 고쳤다.

**검증 못 함 (이 PC 에 .NET SDK 가 없고 WPF 타게팅 팩도 없음)**

- `WpfViewerExample.csproj` 빌드. XAML 컴파일(`InitializeComponent`, `x:Name`
  필드 생성)은 돌려보지 못했다. 위 타입 검사는 그 필드를 스텁으로 대체한 것이다.
- `ImageViewHost` 실행 동작. `HwndHost.BuildWindowCore` / `DestroyWindowCore`
  경로, WPF 리사이즈 연동, DPI 배율은 실행해 보지 못했다.
  (P/Invoke 계층 자체는 WinForms 창을 부모로 삼아 위와 같이 검증했다.)

따라서 `.NET SDK` 가 설치된 환경에서 아래를 먼저 확인할 것.

```bash
dotnet build CSharp/Example/WpfViewerExample.csproj -c Debug -p:Platform=x64
```
