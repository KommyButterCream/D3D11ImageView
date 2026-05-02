# D3D11ImageView
Windows D3D11-based image viewer DLL for rendering images, textures, overlays, and editable ROI objects

# Info
Modular C++ image view component for Windows built on Direct3D 11.
Provides an embeddable child-window viewer that can display raw image buffers, D3D11 textures, or shared D3D11 textures, with interactive zooming, panning, selection, overlay drawing, ROI rendering, and status UI support.

This project is designed as a DLL-based viewer component and integrates shared sibling modules from `Core`, `D3D11Engine`, `D3D11EngineInterface`, and `D3D11UIFramework`.

# Features
- Direct3D 11 device-backed image rendering
- Embeddable HWND-based viewer initialization
- Raw image buffer update support
- D3D11 texture and shared texture update support
- Image-space and window-space overlay rendering
- Overlay support for points, lines, rectangles, rotated rectangles, circles, ellipses, polylines, and polygons
- Editable ROI support for rectangle, ellipse, circle, and polygon objects
- Interactive zoom, zoom fit, 1:1 zoom, and mouse panning
- Selection rectangle and image center cross-line rendering
- Toolbar, context menu, and status bar UI integration
- Pixel coordinate, pixel value, zoom, and image size status display
- Render thread and frame invalidation support
- Modular integration with shared sibling libraries

# Dependencies
- Core
- D3D11Engine
- D3D11EngineInterface
- D3D11UIFramework
- Windows Direct3D 11
- Windows Direct2D / DirectWrite-related UI rendering support
- C++20
- MSVC (Visual Studio 2022)

# Build Environment
- C++20
- MSVC (Visual Studio 2022)
- Windows 10/11 x64
- Visual Studio platform toolset v143

# Project Structure
- `D3D11ImageView/` : main DLL project sources and headers
- `D3D11ImageView/D3D11ImageView.h` : public viewer interface
- `D3D11ImageView/D3D11ImageView.cpp` : exported wrapper implementation
- `D3D11ImageView/D3D11ImageView_Impl.*` : internal viewer window, render, input, image update, overlay, and ROI control logic
- `D3D11ImageView/RenderThread.*` : render worker thread implementation
- `D3D11ImageView/HighResolutionTimer.*` : frame timing helper
- `Image Tile/` : image tile management and pooling
- `Render Layer/` : image, overlay, ROI, selection, center-line, and UI render layers
- `Overlay Renderer/` : overlay shape renderer implementations and overlay types
- `ROI Renderer/` : ROI object renderer implementations and ROI utilities
- `D3D11ImageView.sln` : Visual Studio solution

# Repository Layout
This project expects `D3D11ImageView`, `Core`, `D3D11Engine`, `D3D11EngineInterface`, and `D3D11UIFramework` to be placed under the same parent directory.

Example:
```text
Module/
+-- Core/
+-- D3D11Engine/
+-- D3D11EngineInterface/
+-- D3D11UIFramework/
+-- D3D11ImageView/
```

The Visual Studio solution references shared projects by sibling paths:
- `../Core/Core/Core.vcxproj`
- `../D3D11Engine/D3D11Engine/D3D11Engine.vcxproj`
- `../D3D11UIFramework/D3D11UIFramework/D3D11UIFramework.vcxproj`

The viewer also includes shared render-layer interfaces from:
- `../D3D11EngineInterface/`

# Notes
- Shared libraries are managed as sibling repositories/projects, not as Git submodules.
- Open `D3D11ImageView.sln` with Visual Studio 2022.
- Build the x64 configuration to produce the D3D11ImageView DLL.
- The main target is a DLL for embedding a D3D11 image viewer into a parent window.
- The public interface is exposed through `D3D11ImageView`.
- The viewer can render images from raw memory, an existing `ID3D11Texture2D`, or a shared texture handle.
- Overlays can be drawn either in image coordinates or in window coordinates.
- ROI objects are identified by string keys and can be configured as movable and/or resizable.
