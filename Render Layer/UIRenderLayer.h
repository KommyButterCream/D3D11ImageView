#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"
#include "../../../Module/D3D11EngineInterface/IResizeEventListener.h"
#include "../../../Module/D3D11EngineInterface/IUIRenderLayer.h"
#include "../../../Module/Core/ImageType/ImageBase.h"

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


private:
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

	bool InitializeLeftToolbar(IRenderContext* context, float toolbarWidth);
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

	std::unique_ptr<UIContextMenuPanel> m_contextMenuPanel = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomInContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomOutContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoom1To1ContextMenuButton = nullptr;
	std::shared_ptr<UIContextMenuButton> m_zoomFitContextMenuButton = nullptr;
	std::shared_ptr<UISplitBar> m_contextMenuSplitBar1 = nullptr;
	std::shared_ptr<UIContextMenuButton> m_imageCenterLineContextMenuButton = nullptr;

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





