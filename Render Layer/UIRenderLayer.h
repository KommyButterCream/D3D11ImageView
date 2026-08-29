#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"
#include "../../../Module/D3D11EngineInterface/IResizeEventListener.h"
#include "../../../Module/D3D11EngineInterface/IUIRenderLayer.h"
#include "../../../Module/Core/ImageType/ImageBase.h"
#include "../Lut/LutTable.h"

// UIStyle 을 값으로 들고 있으므로 전방선언으로는 부족하다.
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Base/UIElementBase.h"

#include <memory>

class Camera2D;
class IRenderContext;
class UIPanel;
class UIContextMenuPanel;
class UIButton;
class UIEventDispatcher;
class UIContextMenuButton;
class UISplitBar;
class UIStatusPanel;
class UILabel;
class UIIconLabel;
class FontManager;

enum class UIEventResult;
//struct PixelValue;

class UIRenderLayer
	: public IRenderLayer
	, public IResizeEventListener
	, public IDeviceEventListener
{
public:
	UIRenderLayer();
	virtual ~UIRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IResizeEventListener override
	void OnResize(uint32_t width, uint32_t height) override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;


public:
	// 호스트가 자체 UI 를 쓰는 경우 뷰어 내장 패널을 숨긴다.
	//
	// WPF 등에서는 이미지 위에 호스트 UI 를 얹을 수 없으므로(HWND airspace),
	// 툴바/상태바를 끄고 호스트가 이미지 바깥에 자기 UI 를 두는 구성을 쓴다.
	void SetToolbarVisible(bool visible);
	void SetStatusBarVisible(bool visible);

	// 거리 측정 버튼의 활성 표시를 켜고 끈다.
	void SetMeasureButtonActive(bool active);

	// 각도 측정 버튼의 활성 표시를 켜고 끈다.
	void SetAngleButtonActive(bool active);

	// LUT 버튼의 활성 표시를 켜고 끈다.
	void SetLutButtonActive(bool active);

	// 픽셀 격자 버튼의 활성 표시.
	void SetPixelGridButtonActive(bool active);

	// LUT 를 쓸 수 있는 이미지인가. 컬러 이미지면 버튼과 메뉴 항목을
	// 비활성으로 만든다 — 눌러도 아무 일 없는 버튼보다 낫다.
	void SetLutAvailable(bool available);

	// 프리셋 체크를 라디오처럼 하나만 켠다.
	//
	// enabled 가 false 면 아무 항목도 체크하지 않는다. 체크 표시는
	// "선택된 프리셋" 이 아니라 "지금 적용 중인 프리셋" 을 뜻한다.
	void SetLutPresetChecked(LutPreset preset, bool enabled);

private:
	void RebindFontManager(FontManager* fontManager);
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

	bool InitializeLeftToolbar(IRenderContext* context, float toolbarWidth, FontManager* fontManager);
	bool InitializeContextMenu(IRenderContext* context, FontManager* fontManager);
	bool InitializeStatusbar(IRenderContext* context, float toolbarWidth, FontManager* fontManager);

public:
	bool Update(float dt);

	void SetCamera2D(Camera2D* camera);

	UIEventResult OnMouseEvent(UIMouseEventType type, float x, float y);

	void SetEventDispatcher(UIEventDispatcher* dispatcher);

	void UpdateStatusbarImagePosition(int32_t x, int32_t y);
	void UpdateStatusbarImagePixelValue(const PixelValue value[4], int32_t channel);
	void UpdateStatusbarImageZoom(const float zoom);
	void UpdateStatusbarImageSize(uint32_t width, uint32_t height);
private:
	// Context
	IRenderContext* m_context = nullptr;

	// Camera
	Camera2D* m_camera = nullptr;

	// UI Event Dispatcher
	UIEventDispatcher* m_uiEventDispatcher = nullptr;

	std::unique_ptr<UIPanel> m_toolbarPanel = nullptr;
	std::shared_ptr<UIButton> m_zoomInButton = nullptr;
	std::shared_ptr<UIButton> m_zoomOutButton = nullptr;
	std::shared_ptr<UIButton> m_zoom1To1Button = nullptr;
	std::shared_ptr<UIButton> m_zoomFitButton = nullptr;
	std::shared_ptr<UIButton> m_measureButton = nullptr;
	std::shared_ptr<UIButton> m_angleButton = nullptr;

	// 측정 버튼은 토글이라 활성 상태를 눈으로 알려야 한다. UIButton 에는
	// 체크 상태가 없으므로 스타일 두 벌을 갈아 끼운다.
	UIStyle m_buttonNormalStyle = {};
	UIStyle m_buttonActiveStyle = {};

	// 토글이 켜지면 배경이 밝아지므로 아이콘도 같이 뒤집는다.
	UIStyle m_iconNormalStyle = {};
	UIStyle m_iconActiveStyle = {};

	std::unique_ptr<UIContextMenuPanel> m_contextMenuPanel = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomInContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomOutContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoom1To1ContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomFitContextMenuButton = nullptr;
	std::shared_ptr<UISplitBar> m_contextMenuSplitBar1 = nullptr;
	std::shared_ptr<UIContextMenuButton> m_imageCenterLineContextMenuButton = nullptr;
	std::shared_ptr<UISplitBar> m_contextMenuSplitBar2 = nullptr;

	// Save image — 하위 메뉴를 여는 항목이라 커맨드를 갖지 않는다.
	// 실제 저장 동작은 아직 붙이지 않았고, 잎 항목도 커맨드가 없다.
	std::shared_ptr<UIContextMenuButton> m_saveImageContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuPanel> m_saveImageSubMenu = nullptr;
	std::shared_ptr<UIContextMenuButton> m_saveImagePngButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_saveImageJpegButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_saveImageBmpButton = nullptr;

	// LUT
	std::shared_ptr<UIButton> m_lutButton = nullptr;
	std::shared_ptr<UIButton> m_pixelGridButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_lutContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuPanel> m_lutSubMenu = nullptr;
	std::shared_ptr<UIContextMenuButton>
		m_lutPresetButtons[static_cast<size_t>(LutPreset::Count)] = {};

	std::unique_ptr<UIStatusPanel> m_statusPanel = nullptr;
	std::shared_ptr<UIIconLabel> m_coordinateLabel = nullptr;
	std::shared_ptr<UISplitBar> m_statusSplitBar1 = nullptr;
	std::shared_ptr<UIIconLabel> m_colorLabel = nullptr;
	std::shared_ptr<UISplitBar> m_statusSplitBar2 = nullptr;
	std::shared_ptr<UILabel> m_zoomLabel = nullptr;
	std::shared_ptr<UISplitBar> m_statusSplitBar3 = nullptr;
	std::shared_ptr<UIIconLabel> m_imageSizeLabel = nullptr;




	// State
	bool m_initialized = false;
};





