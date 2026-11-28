# D3D11ImageView

## 프로젝트 설명

Direct3D 11 기반 이미지 뷰어 DLL이다. 자식 HWND에서 이미지 버퍼, D3D11 텍스처, 공유 텍스처를 표시하며 확대·이동, 오버레이, ROI 편집을 제공한다.

## 빌드 환경

- Windows 10 또는 11 x64 및 Windows SDK
- Visual Studio 2026의 **C++를 사용한 데스크톱 개발** 워크로드와 MSVC v145
- C++20

## 저장소 배치와 빌드

다음 폴더를 같은 상위 폴더에 배치한다.

```text
Module/
├─ Core/
├─ D3D11Engine/
├─ D3D11EngineInterface/
├─ D3D11ImageIO/
├─ D3D11UIFramework/
└─ D3D11ImageView/
```

Visual Studio 2026에서 `D3D11ImageView.sln`을 열고 `Debug|x64` 또는 `Release|x64`로 빌드한다. 결과 DLL은 솔루션의 `x64/Debug/` 또는 `x64/Release/`에 생성된다.

실행 시 작업 디렉터리의 상위에 `Shaders/`가 있어야 한다.
