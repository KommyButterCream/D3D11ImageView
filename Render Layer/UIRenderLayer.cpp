#include "pch.h"
#include "UIRenderLayer.h"

#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Resource/UIDefaultColor.h"
#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"
#include "../../../Module/D3D11Engine/Camera/Camera2D.h"

#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Panel/UIPanel.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Panel/UIContextMenuPanel.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Button/UIButton.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Button/UIContextMenuButton.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Event/UIEventResult.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Splitbar/UISplitBar.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Panel/UIStatusPanel.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Label/UILabel.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Label/UIIconLabel.h"

#include "../../../Module/Core/ShapeType/Rect2f.h"
//#include "../../../Module/Core/ImageType/ImageBase.h"

#include "../Overlay Renderer/OverlayTypes.h"

using Core::ShapeType::Rect2f;

UIRenderLayer::UIRenderLayer()
{

}

UIRenderLayer::~UIRenderLayer()
{
	Shutdown();
}

bool UIRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
		return false;

	m_context = context;
	if (!m_context)
		return false;

	if (!AcquireDeviceResources())
		return false;

	FontManager* fontManager = m_context->GetEngine()->GetFontManager();
	if (!fontManager)
		return false;

	constexpr float toolbarWidth = 50.f;

	if (!InitializeLeftToolbar(context, toolbarWidth, fontManager))
		return false;

	if (!InitializeContextMenu(context, fontManager))
		return false;

	if (!InitializeStatusbar(context, toolbarWidth, fontManager))
		return false;

	m_context->AddResizeListener(this);
	m_context->AddDeviceListener(this);

	m_initialized = true;

	return true;
}

void UIRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveResizeListener(this);
		m_context->RemoveDeviceListener(this);
	}

	if (m_toolbarPanel)
	{
		m_toolbarPanel->Shutdown();
	}

	if (m_contextMenuPanel)
	{
		m_contextMenuPanel->Shutdown();
	}

	if (m_statusPanel)
	{
		m_statusPanel->Shutdown();
	}

	m_uiEventDispatcher = nullptr;

	m_initialized = false;
}

bool UIRenderLayer::Prepare()
{
	if (m_contextMenuPanel)
	{
		m_contextMenuPanel->Prepare();
	}

	return true;
}

bool UIRenderLayer::Render()
{
	if (!m_initialized)
		return true;

	if (!m_context)
		return false;

	//m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
	m_toolbarPanel->Render();
	m_contextMenuPanel->Render();
	m_statusPanel->Render();

	return true;
}

void UIRenderLayer::OnResize(uint32_t width, uint32_t height)
{
	if (m_toolbarPanel)
	{
		m_toolbarPanel->Resize(static_cast<float>(width), static_cast<float>(height));
	}

	if (m_statusPanel)
	{
		m_statusPanel->Resize(static_cast<float>(width), static_cast<float>(height));
	}
}

void UIRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void UIRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

// UI 요소들이 들고 있는 FontManager 포인터를 현재 것으로 다시 맞춘다.
//
// 엔진은 디바이스 재생성 시 ReleaseDeviceResources() 에서 FontManager 를
// delete 하고 CreateDeviceResources() 에서 새로 만든다. 요소들은 생성 시점에
// 받은 raw 포인터를 그대로 들고 있으므로, 복구 후 그대로 두면 해제된 객체를
// 참조해 use-after-free 가 된다(상태바 첫 라벨에서 실제로 크래시했다).
void UIRenderLayer::RebindFontManager(FontManager* fontManager)
{
	if (!fontManager)
		return;

	// 컨텍스트 메뉴 버튼
	if (m_zoomInContextMenuButton)          m_zoomInContextMenuButton->SetFontManager(fontManager);
	if (m_zoomOutContextMenuButton)         m_zoomOutContextMenuButton->SetFontManager(fontManager);
	if (m_zoom1To1ContextMenuButton)        m_zoom1To1ContextMenuButton->SetFontManager(fontManager);
	if (m_zoomFitContextMenuButton)         m_zoomFitContextMenuButton->SetFontManager(fontManager);
	if (m_imageCenterLineContextMenuButton) m_imageCenterLineContextMenuButton->SetFontManager(fontManager);

	// 툴바 — 측정 버튼만 텍스트를 쓴다.
	if (m_measureButton) m_measureButton->SetFontManager(fontManager);

	// 상태바 라벨
	if (m_coordinateLabel) m_coordinateLabel->SetFontManager(fontManager);
	if (m_colorLabel)      m_colorLabel->SetFontManager(fontManager);
	if (m_zoomLabel)       m_zoomLabel->SetFontManager(fontManager);
	if (m_imageSizeLabel)  m_imageSizeLabel->SetFontManager(fontManager);
}

bool UIRenderLayer::AcquireDeviceResources()
{
	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return false;

	// 패널을 복구하기 전에 폰트 매니저부터 갱신해야 한다.
	// 복구 과정에서 텍스트 포맷을 다시 받아오기 때문이다.
	RebindFontManager(engine->GetFontManager());

	bool arePanelsRestored = true;

	if (m_toolbarPanel)
		arePanelsRestored &= m_toolbarPanel->RestoreDeviceResources(m_context);

	if (m_contextMenuPanel)
		arePanelsRestored &= m_contextMenuPanel->RestoreDeviceResources(m_context);

	if (m_statusPanel)
		arePanelsRestored &= m_statusPanel->RestoreDeviceResources(m_context);

	return arePanelsRestored;
}

void UIRenderLayer::ReleaseDeviceResources()
{
	if (m_toolbarPanel)
	{
		m_toolbarPanel->DiscardDeviceResources();
	}

	if (m_contextMenuPanel)
	{
		m_contextMenuPanel->DiscardDeviceResources();
	}

	if (m_statusPanel)
	{
		m_statusPanel->DiscardDeviceResources();
	}
}

bool UIRenderLayer::InitializeLeftToolbar(IRenderContext* context, float toolbarWidth, FontManager* fontManager)
{
	m_toolbarPanel = std::make_unique<UIPanel>();

	// style
	UIStyle& style = m_toolbarPanel->GetStyle();
	style.borderThickness = 0.0f;
	style.normal.fill = UIDefaultColor::panelNormalColor.ToD2DColor();
	style.normal.border = UIDefaultColor::transparent.ToD2DColor();

	// layout
	uint32_t viewWidth = m_context->GetWidth();
	uint32_t viewHeight = m_context->GetHeight();
	Rect2f rect = { 0, 0, toolbarWidth, static_cast<float>(viewHeight) };
	m_toolbarPanel->SetLayout(rect);

	// Alignment
	constexpr float padding = 5.0f;
	constexpr float spacing = 8.0f;
	float buttonSize = toolbarWidth - (padding * 2.0f);

	m_toolbarPanel->SetPadding(padding);
	m_toolbarPanel->SetSpacing(spacing);
	m_toolbarPanel->SetLayoutType(UILayoutType::Vertical);

	UIStyle buttonDefaultStyle, buttonIconDefaultStyle;

	{
		buttonDefaultStyle.borderThickness = 1.0f;

		buttonDefaultStyle.normal.fill = UIDefaultColor::buttonNormalColor.ToD2DColor();
		buttonDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		buttonDefaultStyle.hover.fill = UIDefaultColor::buttonHoverColor.ToD2DColor();
		buttonDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		buttonDefaultStyle.pressed.fill = UIDefaultColor::buttonPressedColor.ToD2DColor();
		buttonDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		buttonDefaultStyle.disabled.fill = UIDefaultColor::buttonDisabledColor.ToD2DColor();
		buttonDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	{
		buttonIconDefaultStyle.borderThickness = 0.0f;

		buttonIconDefaultStyle.normal.fill = UIDefaultColor::iconNormalColor.ToD2DColor();
		buttonIconDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		buttonIconDefaultStyle.hover.fill = UIDefaultColor::iconHoverColor.ToD2DColor();
		buttonIconDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		buttonIconDefaultStyle.pressed.fill = UIDefaultColor::iconPressedColor.ToD2DColor();
		buttonIconDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		buttonIconDefaultStyle.disabled.fill = UIDefaultColor::iconDisabledColor.ToD2DColor();
		buttonIconDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	// Add Button
	{
		m_zoomInButton = std::make_unique<UIButton>();

		m_zoomInButton->SetStyle(buttonDefaultStyle);
		m_zoomInButton->SetLayout({ 0.f, 0.f, buttonSize, buttonSize });
		m_zoomInButton->SetRounded(true);
		m_zoomInButton->SetCornerRadius(4.f);
		m_zoomInButton->SetCommand(UICommand::ZoomIn);
		m_zoomInButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomInButton->SetIcon(L"../Icons/icon_zoom_in.svg");
		m_zoomInButton->SetIconStyle(buttonIconDefaultStyle);

		m_toolbarPanel->AddChild(m_zoomInButton);
	}

	{
		m_zoomOutButton = std::make_unique<UIButton>();

		m_zoomOutButton->SetStyle(buttonDefaultStyle);
		m_zoomOutButton->SetLayout({ 0.f, 0.f, buttonSize, buttonSize });
		m_zoomOutButton->SetRounded(true);
		m_zoomOutButton->SetCornerRadius(4.f);
		m_zoomOutButton->SetCommand(UICommand::ZoomOut);
		m_zoomOutButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomOutButton->SetIcon(L"../Icons/icon_zoom_out.svg");
		m_zoomOutButton->SetIconStyle(buttonIconDefaultStyle);

		m_toolbarPanel->AddChild(m_zoomOutButton);
	}

	{
		m_zoom1To1Button = std::make_unique<UIButton>();

		m_zoom1To1Button->SetStyle(buttonDefaultStyle);
		m_zoom1To1Button->SetLayout({ 0.f, 0.f, buttonSize, buttonSize });
		m_zoom1To1Button->SetRounded(true);
		m_zoom1To1Button->SetCornerRadius(4.f);
		m_zoom1To1Button->SetCommand(UICommand::Zoom1to1);
		m_zoom1To1Button->SetEventDispatcher(m_uiEventDispatcher);
		m_zoom1To1Button->SetIcon(L"../Icons/icon_zoom_1on1.svg");
		m_zoom1To1Button->SetIconStyle(buttonIconDefaultStyle);

		m_toolbarPanel->AddChild(m_zoom1To1Button);
	}

	{
		m_zoomFitButton = std::make_unique<UIButton>();

		m_zoomFitButton->SetStyle(buttonDefaultStyle);
		m_zoomFitButton->SetLayout({ 0.f, 0.f, buttonSize, buttonSize });
		m_zoomFitButton->SetRounded(true);
		m_zoomFitButton->SetCornerRadius(4.f);
		m_zoomFitButton->SetCommand(UICommand::ZoomFit);
		m_zoomFitButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomFitButton->SetIcon(L"../Icons/icon_zoom_fit_image.svg");
		m_zoomFitButton->SetIconStyle(buttonIconDefaultStyle);

		m_toolbarPanel->AddChild(m_zoomFitButton);
	}

	// 거리 측정 (토글)
	{
		// 활성 상태용 스타일. UIButton 에는 체크 상태가 없어서 스타일을
		// 갈아 끼워 표시한다. 눌린 색을 normal 로 올려 계속 눌린 것처럼 보이게 한다.
		m_buttonNormalStyle = buttonDefaultStyle;

		m_buttonActiveStyle = buttonDefaultStyle;
		m_buttonActiveStyle.normal.fill = UIDefaultColor::buttonPressedColor.ToD2DColor();
		m_buttonActiveStyle.hover.fill = UIDefaultColor::buttonPressedColor.ToD2DColor();

		m_measureButton = std::make_unique<UIButton>();

		m_measureButton->SetStyle(m_buttonNormalStyle);
		m_measureButton->SetLayout({ 0.f, 0.f, buttonSize, buttonSize });
		m_measureButton->SetRounded(true);
		m_measureButton->SetCornerRadius(4.f);
		m_measureButton->SetCommand(UICommand::MeasureDistance);
		m_measureButton->SetEventDispatcher(m_uiEventDispatcher);

		// 아이콘 대신 글자를 쓴다. UIButton 은 UILabel 을 상속하므로 그대로 된다.
		// (이 저장소에는 ../Icons 폴더가 없어 기존 버튼들도 아이콘이 비어 있다)
		//
		// 텍스트를 쓰는 유일한 툴바 버튼이라 FontManager 를 직접 연결한다.
		// RebindFontManager 에도 넣어두어야 디바이스 복구 후 해제된 매니저를
		// 참조하지 않는다.
		UITextStyle measureTextStyle;
		wcscpy_s(measureTextStyle.fontName, 50, L"Segoe UI Symbol");
		measureTextStyle.fontSize = 18.0f;
		measureTextStyle.weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_BOLD;
		measureTextStyle.hAlign = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_CENTER;
		measureTextStyle.vAlign = DWRITE_PARAGRAPH_ALIGNMENT::DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		measureTextStyle.normal.fill = UIDefaultColor::iconNormalColor.ToD2DColor();
		measureTextStyle.hover.fill = UIDefaultColor::iconHoverColor.ToD2DColor();
		measureTextStyle.pressed.fill = UIDefaultColor::iconPressedColor.ToD2DColor();
		measureTextStyle.disabled.fill = UIDefaultColor::iconDisabledColor.ToD2DColor();

		m_measureButton->SetFontManager(fontManager);
		m_measureButton->SetTextStyle(measureTextStyle);
		m_measureButton->SetText(L"↔");
		m_measureButton->SetIconStyle(buttonIconDefaultStyle);

		m_toolbarPanel->AddChild(m_measureButton);
	}

	// Toolbar Initialize
	if (!m_toolbarPanel->Initialize(context))
		return false;

	return true;
}

bool UIRenderLayer::InitializeContextMenu(IRenderContext* context, FontManager* fontManager)
{
	m_contextMenuPanel = std::make_unique<UIContextMenuPanel>();

	// style
	UIStyle& style = m_contextMenuPanel->GetStyle();
	style.borderThickness = 0.0f;
	style.normal.fill = UIDefaultColor::contextMenuPanelNormalColor.ToD2DColor();
	style.normal.border = UIDefaultColor::contextMenuPanelBorderColor.ToD2DColor();

	// layout
	constexpr float menuWidth = 300.0f;
	m_contextMenuPanel->SetMenuWidth(menuWidth);

	// Alignment
	constexpr float padding = 3.0f;
	constexpr float spacing = 1.0f;
	constexpr float menuButtonWidth = menuWidth - (padding * 2.0);
	constexpr float menuButtonHeight = 24.0f;

	m_contextMenuPanel->SetPadding(padding);
	m_contextMenuPanel->SetSpacing(spacing);
	m_contextMenuPanel->SetLayoutType(UILayoutType::Vertical);
	m_contextMenuPanel->SetRounded(true);
	m_contextMenuPanel->SetCornerRadius(4.f);

	// ContextMenu Initialize
	if (!m_contextMenuPanel->Initialize(context))
		return false;

	UIStyle contextMenuButtonDefaultStyle, contextMenuButtonIconDefaultStyle;
	UITextStyle contextMenuButtonTextDefaultStyle;

	{
		contextMenuButtonDefaultStyle.borderThickness = 1.0f;

		contextMenuButtonDefaultStyle.normal.fill = UIDefaultColor::contextButtonNormalColor.ToD2DColor();
		contextMenuButtonDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonDefaultStyle.hover.fill = UIDefaultColor::contextButtonHoverColor.ToD2DColor();
		contextMenuButtonDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonDefaultStyle.pressed.fill = UIDefaultColor::contextButtonPressedColor.ToD2DColor();
		contextMenuButtonDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonDefaultStyle.disabled.fill = UIDefaultColor::contextButtonDisabledColor.ToD2DColor();
		contextMenuButtonDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	{
		contextMenuButtonIconDefaultStyle.borderThickness = 0.0f;

		contextMenuButtonIconDefaultStyle.normal.fill = UIDefaultColor::iconNormalColor.ToD2DColor();
		contextMenuButtonIconDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonIconDefaultStyle.hover.fill = UIDefaultColor::iconHoverColor.ToD2DColor();
		contextMenuButtonIconDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonIconDefaultStyle.pressed.fill = UIDefaultColor::iconPressedColor.ToD2DColor();
		contextMenuButtonIconDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonIconDefaultStyle.disabled.fill = UIDefaultColor::iconDisabledColor.ToD2DColor();
		contextMenuButtonIconDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	{
		wcscpy_s(contextMenuButtonTextDefaultStyle.fontName, 50, L"Consolas");
		contextMenuButtonTextDefaultStyle.fontSize = 15.0f;

		contextMenuButtonTextDefaultStyle.weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
		contextMenuButtonTextDefaultStyle.hAlign = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_CENTER;
		contextMenuButtonTextDefaultStyle.vAlign = DWRITE_PARAGRAPH_ALIGNMENT::DWRITE_PARAGRAPH_ALIGNMENT_CENTER;

		contextMenuButtonTextDefaultStyle.normal.fill = UIDefaultColor::textNormalColor.ToD2DColor();
		contextMenuButtonTextDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonTextDefaultStyle.hover.fill = UIDefaultColor::textHoverColor.ToD2DColor();
		contextMenuButtonTextDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonTextDefaultStyle.pressed.fill = UIDefaultColor::textPressedColor.ToD2DColor();
		contextMenuButtonTextDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		contextMenuButtonTextDefaultStyle.disabled.fill = UIDefaultColor::textDisabledColor.ToD2DColor();
		contextMenuButtonTextDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	// Add Context Button
	{
		m_zoomInContextMenuButton = std::make_unique<UIContextMenuButton>();

		m_zoomInContextMenuButton->SetFontManager(fontManager);
		m_zoomInContextMenuButton->SetStyle(contextMenuButtonDefaultStyle);
		m_zoomInContextMenuButton->SetLayout({ 0.f, 0.f, menuButtonWidth, menuButtonHeight });
		m_zoomInContextMenuButton->SetRounded(true);
		m_zoomInContextMenuButton->SetCornerRadius(2.f);
		m_zoomInContextMenuButton->SetIconAreaWidth(30.0f);
		m_zoomInContextMenuButton->SetExtraAreaWidth(50.0f);
		m_zoomInContextMenuButton->SetCommand(UICommand::ZoomIn);
		m_zoomInContextMenuButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomInContextMenuButton->SetIcon(L"../Icons/icon_zoom_in.svg");
		m_zoomInContextMenuButton->SetIconStyle(contextMenuButtonIconDefaultStyle);
		m_zoomInContextMenuButton->SetText(L"Zoom In");
		m_zoomInContextMenuButton->SetExtraText(L"Ctrl +");
		m_zoomInContextMenuButton->SetTextStyle(contextMenuButtonTextDefaultStyle);
		m_contextMenuPanel->AddChild(m_zoomInContextMenuButton);
	}

	{
		m_zoomOutContextMenuButton = std::make_unique<UIContextMenuButton>();

		m_zoomOutContextMenuButton->SetFontManager(fontManager);
		m_zoomOutContextMenuButton->SetStyle(contextMenuButtonDefaultStyle);
		m_zoomOutContextMenuButton->SetLayout({ 0.f, 0.f, menuButtonWidth, menuButtonHeight });
		m_zoomOutContextMenuButton->SetRounded(true);
		m_zoomOutContextMenuButton->SetCornerRadius(2.f);
		m_zoomOutContextMenuButton->SetIconAreaWidth(30.0f);
		m_zoomOutContextMenuButton->SetExtraAreaWidth(50.0f);
		m_zoomOutContextMenuButton->SetCommand(UICommand::ZoomOut);
		m_zoomOutContextMenuButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomOutContextMenuButton->SetIcon(L"../Icons/icon_zoom_out.svg");
		m_zoomOutContextMenuButton->SetIconStyle(contextMenuButtonIconDefaultStyle);
		m_zoomOutContextMenuButton->SetText(L"Zoom Out");
		m_zoomOutContextMenuButton->SetExtraText(L"Ctrl -");
		m_zoomOutContextMenuButton->SetTextStyle(contextMenuButtonTextDefaultStyle);

		m_contextMenuPanel->AddChild(m_zoomOutContextMenuButton);
	}

	{
		m_zoom1To1ContextMenuButton = std::make_unique<UIContextMenuButton>();

		m_zoom1To1ContextMenuButton->SetFontManager(fontManager);
		m_zoom1To1ContextMenuButton->SetStyle(contextMenuButtonDefaultStyle);
		m_zoom1To1ContextMenuButton->SetLayout({ 0.f, 0.f, menuButtonWidth, menuButtonHeight });
		m_zoom1To1ContextMenuButton->SetRounded(true);
		m_zoom1To1ContextMenuButton->SetCornerRadius(2.f);
		m_zoom1To1ContextMenuButton->SetIconAreaWidth(30.0f);
		m_zoom1To1ContextMenuButton->SetExtraAreaWidth(50.0f);
		m_zoom1To1ContextMenuButton->SetCommand(UICommand::Zoom1to1);
		m_zoom1To1ContextMenuButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoom1To1ContextMenuButton->SetIcon(L"../Icons/icon_zoom_1on1.svg");
		m_zoom1To1ContextMenuButton->SetIconStyle(contextMenuButtonIconDefaultStyle);
		m_zoom1To1ContextMenuButton->SetText(L"Zoom 1:1");
		m_zoom1To1ContextMenuButton->SetExtraText(L"Ctrl 1");
		m_zoom1To1ContextMenuButton->SetTextStyle(contextMenuButtonTextDefaultStyle);

		m_contextMenuPanel->AddChild(m_zoom1To1ContextMenuButton);
	}

	{
		m_zoomFitContextMenuButton = std::make_unique<UIContextMenuButton>();

		m_zoomFitContextMenuButton->SetFontManager(fontManager);
		m_zoomFitContextMenuButton->SetStyle(contextMenuButtonDefaultStyle);
		m_zoomFitContextMenuButton->SetLayout({ 0.f, 0.f, menuButtonWidth, menuButtonHeight });
		m_zoomFitContextMenuButton->SetRounded(true);
		m_zoomFitContextMenuButton->SetCornerRadius(2.f);
		m_zoomFitContextMenuButton->SetIconAreaWidth(30.0f);
		m_zoomFitContextMenuButton->SetExtraAreaWidth(50.0f);
		m_zoomFitContextMenuButton->SetCommand(UICommand::ZoomFit);
		m_zoomFitContextMenuButton->SetEventDispatcher(m_uiEventDispatcher);
		m_zoomFitContextMenuButton->SetIcon(L"../Icons/icon_zoom_fit_image.svg");
		m_zoomFitContextMenuButton->SetIconStyle(contextMenuButtonIconDefaultStyle);
		m_zoomFitContextMenuButton->SetText(L"Zoom Fit");
		m_zoomFitContextMenuButton->SetExtraText(L"");
		m_zoomFitContextMenuButton->SetTextStyle(contextMenuButtonTextDefaultStyle);

		m_contextMenuPanel->AddChild(m_zoomFitContextMenuButton);
	}

	{
		m_contextMenuSplitBar1 = std::make_unique<UISplitBar>();

		m_contextMenuSplitBar1->SetLineColor(UIDefaultColor::contextSplitBarColor.ToD2DColor());
		m_contextMenuSplitBar1->SetLayout({ 0.f, 0.f, menuButtonWidth, 3.0f });
		m_contextMenuSplitBar1->SetSplitbarType(SplitBarType::Horizontal);
		m_contextMenuPanel->AddChild(m_contextMenuSplitBar1);
	}

	{
		m_imageCenterLineContextMenuButton = std::make_unique<UIContextMenuButton>();

		m_imageCenterLineContextMenuButton->SetFontManager(fontManager);
		m_imageCenterLineContextMenuButton->SetStyle(contextMenuButtonDefaultStyle);
		m_imageCenterLineContextMenuButton->SetLayout({ 0.f, 0.f, menuButtonWidth, menuButtonHeight });
		m_imageCenterLineContextMenuButton->SetRounded(true);
		m_imageCenterLineContextMenuButton->SetCornerRadius(2.f);
		m_imageCenterLineContextMenuButton->SetIconAreaWidth(30.0f);
		m_imageCenterLineContextMenuButton->SetExtraAreaWidth(50.0f);
		m_imageCenterLineContextMenuButton->SetCommand(UICommand::ImageCenterCrossLine);
		m_imageCenterLineContextMenuButton->SetEventDispatcher(m_uiEventDispatcher);
		m_imageCenterLineContextMenuButton->SetIcon(L"../Icons/icon_check.svg");
		m_imageCenterLineContextMenuButton->SetIconStyle(contextMenuButtonIconDefaultStyle);
		m_imageCenterLineContextMenuButton->SetText(L"Show image center line");
		m_imageCenterLineContextMenuButton->SetExtraText(L"");
		m_imageCenterLineContextMenuButton->SetTextStyle(contextMenuButtonTextDefaultStyle);
		m_imageCenterLineContextMenuButton->SetCheckable(true);

		m_contextMenuPanel->AddChild(m_imageCenterLineContextMenuButton);
	}

	return true;
}

bool UIRenderLayer::InitializeStatusbar(IRenderContext* context, float toolbarWidth, FontManager* fontManager)
{
	m_statusPanel = std::make_unique<UIStatusPanel>();

	// style
	UIStyle& style = m_statusPanel->GetStyle();
	style.borderThickness = 0.0f;
	style.normal.fill = UIDefaultColor::contextMenuPanelNormalColor.ToD2DColor();
	style.normal.border = UIDefaultColor::contextMenuPanelBorderColor.ToD2DColor();

	// layout
	uint32_t viewWidth = m_context->GetWidth();
	uint32_t viewHeight = m_context->GetHeight();

	constexpr float statusbarHeight = 26.f;

	Rect2f rect = {
		toolbarWidth,
		static_cast<float>(viewHeight) - statusbarHeight - 1.0f,
		static_cast<float>(viewWidth),
		static_cast<float>(viewHeight) };

	m_statusPanel->SetLayout(rect);

	// Alignment
	constexpr float padding = 3.0f;
	constexpr float spacing = 1.0f;
	constexpr float splitbarHeight = statusbarHeight - (padding * 2.0);

	m_statusPanel->SetPadding(padding);
	m_statusPanel->SetSpacing(spacing);
	m_statusPanel->SetLayoutType(UILayoutType::Horizontal);

	// Status Panel Initialize
	if (!m_statusPanel->Initialize(context))
		return false;

	UIStyle statusLabelDefaultStyle, statusLabelIconDefaultStyle;
	UITextStyle statusLabelTextDefaultStyle;

	{
		statusLabelDefaultStyle.borderThickness = 1.0f;

		statusLabelDefaultStyle.normal.fill = UIDefaultColor::labelNormalColor.ToD2DColor();
		statusLabelDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelDefaultStyle.hover.fill = UIDefaultColor::labelHoverColor.ToD2DColor();
		statusLabelDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelDefaultStyle.pressed.fill = UIDefaultColor::labelPressedColor.ToD2DColor();
		statusLabelDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelDefaultStyle.disabled.fill = UIDefaultColor::labelDisabledColor.ToD2DColor();
		statusLabelDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	{
		statusLabelIconDefaultStyle.borderThickness = 0.0f;

		statusLabelIconDefaultStyle.normal.fill = UIDefaultColor::iconNormalColor.ToD2DColor();
		statusLabelIconDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelIconDefaultStyle.hover.fill = UIDefaultColor::iconHoverColor.ToD2DColor();
		statusLabelIconDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelIconDefaultStyle.pressed.fill = UIDefaultColor::iconPressedColor.ToD2DColor();
		statusLabelIconDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelIconDefaultStyle.disabled.fill = UIDefaultColor::iconDisabledColor.ToD2DColor();
		statusLabelIconDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	{
		wcscpy_s(statusLabelTextDefaultStyle.fontName, 50, L"Consolas");
		statusLabelTextDefaultStyle.fontSize = 14.0f;

		statusLabelTextDefaultStyle.weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
		statusLabelTextDefaultStyle.hAlign = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_LEADING;
		statusLabelTextDefaultStyle.vAlign = DWRITE_PARAGRAPH_ALIGNMENT::DWRITE_PARAGRAPH_ALIGNMENT_CENTER;

		statusLabelTextDefaultStyle.normal.fill = UIDefaultColor::statusbarTextNormalColor.ToD2DColor();
		statusLabelTextDefaultStyle.normal.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelTextDefaultStyle.hover.fill = UIDefaultColor::statusbarTextHoverColor.ToD2DColor();
		statusLabelTextDefaultStyle.hover.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelTextDefaultStyle.pressed.fill = UIDefaultColor::statusbarTextPressedColor.ToD2DColor();
		statusLabelTextDefaultStyle.pressed.border = UIDefaultColor::transparent.ToD2DColor();

		statusLabelTextDefaultStyle.disabled.fill = UIDefaultColor::statusbarTextDisabledColor.ToD2DColor();
		statusLabelTextDefaultStyle.disabled.border = UIDefaultColor::transparent.ToD2DColor();
	}

	// Add Label
	{
		m_coordinateLabel = std::make_unique<UIIconLabel>();

		constexpr float labelWidth = 220.f;
		constexpr float iconWidth = 30.f;
		constexpr float textWidth = labelWidth - iconWidth;

		m_coordinateLabel->SetFontManager(fontManager);
		m_coordinateLabel->SetStyle(statusLabelDefaultStyle);
		m_coordinateLabel->SetLayout({ 0.f, 0.f, labelWidth, statusbarHeight });
		m_coordinateLabel->SetTextPadding(8.f, 0.f, 0.f, 0.f);
		m_coordinateLabel->SetRounded(true);
		m_coordinateLabel->SetCornerRadius(2.f);
		m_coordinateLabel->SetText(L"X : 0 Y : 0");
		m_coordinateLabel->SetTextStyle(statusLabelTextDefaultStyle);
		m_coordinateLabel->SetIconAreaWidth(iconWidth);
		m_coordinateLabel->SetTextAreaWidth(textWidth);
		m_coordinateLabel->SetIcon(L"../Icons/icon_arrow.svg");
		m_coordinateLabel->SetIconScale(0.6f);
		m_coordinateLabel->SetIconStyle(statusLabelIconDefaultStyle);

		m_statusPanel->AddChild(m_coordinateLabel);
	}

	{
		m_statusSplitBar1 = std::make_unique<UISplitBar>();

		m_statusSplitBar1->SetLineColor(UIDefaultColor::contextSplitBarColor.ToD2DColor());
		m_statusSplitBar1->SetLayout({ 0.f, 0.f, 3.0f, splitbarHeight });
		m_statusSplitBar1->SetSplitbarType(SplitBarType::Vertical);
		m_statusSplitBar1->SetLineThickness(3.0f);

		m_statusPanel->AddChild(m_statusSplitBar1);
	}

	{
		m_colorLabel = std::make_unique<UIIconLabel>();

		constexpr float labelWidth = 400.f;
		constexpr float iconWidth = 30.f;
		constexpr float textWidth = labelWidth - iconWidth;

		m_colorLabel->SetFontManager(fontManager);
		m_colorLabel->SetStyle(statusLabelDefaultStyle);
		m_colorLabel->SetLayout({ 0.f, 0.f, labelWidth, statusbarHeight });
		m_colorLabel->SetTextPadding(5.f, 0.f, 0.f, 0.f);
		m_colorLabel->SetRounded(true);
		m_colorLabel->SetCornerRadius(2.f);
		m_colorLabel->SetText(L"Gray : 0");
		m_colorLabel->SetTextStyle(statusLabelTextDefaultStyle);
		m_colorLabel->SetIconAreaWidth(iconWidth);
		m_colorLabel->SetTextAreaWidth(textWidth);
		m_colorLabel->SetIcon(L"../Icons/icon_color.svg");
		m_colorLabel->SetIconScale(0.6f);
		m_colorLabel->SetIconStyle(statusLabelIconDefaultStyle);

		m_statusPanel->AddChild(m_colorLabel);
	}

	{
		m_statusSplitBar2 = std::make_unique<UISplitBar>();

		m_statusSplitBar2->SetLineColor(UIDefaultColor::contextSplitBarColor.ToD2DColor());
		m_statusSplitBar2->SetLayout({ 0.f, 0.f, 3.0f, splitbarHeight });
		m_statusSplitBar2->SetSplitbarType(SplitBarType::Vertical);
		m_statusSplitBar2->SetLineThickness(3.0f);

		m_statusPanel->AddChild(m_statusSplitBar2);
	}

	{
		m_zoomLabel = std::make_unique<UILabel>();

		m_zoomLabel->SetFontManager(fontManager);
		m_zoomLabel->SetStyle(statusLabelDefaultStyle);
		m_zoomLabel->SetLayout({ 0.f, 0.f, 90.f, statusbarHeight - 5.f });
		m_zoomLabel->SetTextPadding(8.f, 0.f, 0.f, 0.f);
		m_zoomLabel->SetRounded(true);
		m_zoomLabel->SetCornerRadius(2.f);
		m_zoomLabel->SetText(L"100%");
		m_zoomLabel->SetTextStyle(statusLabelTextDefaultStyle);

		m_statusPanel->AddChild(m_zoomLabel);
	}

	{
		m_statusSplitBar3 = std::make_unique<UISplitBar>();

		m_statusSplitBar3->SetLineColor(UIDefaultColor::contextSplitBarColor.ToD2DColor());
		m_statusSplitBar3->SetLayout({ 0.f, 0.f, 3.0f, splitbarHeight });
		m_statusSplitBar3->SetSplitbarType(SplitBarType::Vertical);
		m_statusSplitBar3->SetLineThickness(3.0f);

		m_statusPanel->AddChild(m_statusSplitBar3);
	}

	{
		m_imageSizeLabel = std::make_unique<UIIconLabel>();

		constexpr float labelWidth = 200.f;
		constexpr float iconWidth = 30.f;
		constexpr float textWidth = labelWidth - iconWidth;

		m_imageSizeLabel->SetFontManager(fontManager);
		m_imageSizeLabel->SetStyle(statusLabelDefaultStyle);
		m_imageSizeLabel->SetLayout({ 0.f, 0.f, labelWidth, statusbarHeight - 5.f });
		m_imageSizeLabel->SetTextPadding(5.f, 0.f, 0.f, 0.f);
		m_imageSizeLabel->SetRounded(true);
		m_imageSizeLabel->SetCornerRadius(2.f);
		m_imageSizeLabel->SetText(L"0 x 0 px");
		m_imageSizeLabel->SetTextStyle(statusLabelTextDefaultStyle);
		m_imageSizeLabel->SetIconAreaWidth(iconWidth);
		m_imageSizeLabel->SetTextAreaWidth(textWidth);
		m_imageSizeLabel->SetIcon(L"../Icons/icon_imagesize.svg");
		m_imageSizeLabel->SetIconScale(0.6f);
		m_imageSizeLabel->SetIconStyle(statusLabelIconDefaultStyle);

		m_statusPanel->AddChild(m_imageSizeLabel);
	}

	return true;
}

bool UIRenderLayer::Update(float dt)
{
	// Return true while UI animation needs another render pass.
	// Return false when all animated UI elements are idle.
	const bool isToolbarPanelBusy = m_toolbarPanel->Update(dt);
	const bool isContextMenuPanelBusy = m_contextMenuPanel->Update(dt);
	const bool isStatusPanelBusy = m_statusPanel->Update(dt);

	return isToolbarPanelBusy || isContextMenuPanelBusy || isStatusPanelBusy;
}

void UIRenderLayer::SetCamera2D(Camera2D* camera)
{
	m_camera = camera;
}

UIEventResult UIRenderLayer::OnMouseEvent(UIMouseEventType type, float x, float y)
{
	if (!m_initialized ||
		!m_toolbarPanel || !m_contextMenuPanel || !m_statusPanel)
		return UIEventResult::None;

	if (m_contextMenuPanel &&
		(m_contextMenuPanel->IsVisible() || type == UIMouseEventType::RButtonUp))
	{
		if (m_contextMenuPanel->OnMouseEvent(type, x, y))
		{
			return UIEventResult::ContextMenu;
		}
	}

	if (m_toolbarPanel && m_toolbarPanel->IsVisible())
	{
		if (m_toolbarPanel->OnMouseEvent(type, x, y))
		{
			return UIEventResult::Toolbar;
		}
	}

	if (m_statusPanel && m_statusPanel->IsVisible())
	{
		if (m_statusPanel->OnMouseEvent(type, x, y))
		{
			return UIEventResult::StatusPanel;
		}
	}

	return UIEventResult::None;
}

void UIRenderLayer::SetEventDispatcher(UIEventDispatcher* dispatcher)
{
	m_uiEventDispatcher = dispatcher;
}

void UIRenderLayer::UpdateStatusbarImagePosition(int32_t x, int32_t y)
{
	wchar_t buffer[32] = {};
	swprintf_s(buffer, 32, L"X : %d   Y : %d", x, y);

	if (m_coordinateLabel)
	{
		m_coordinateLabel->SetText(buffer);
	}
}

void UIRenderLayer::UpdateStatusbarImagePixelValue(const PixelValue value[4], int32_t channel)
{
	if (m_colorLabel)
	{
		constexpr size_t bufferCount = 64;
		wchar_t buffer[bufferCount] = {};
		const PixelValueFormat format = value[0].format;

		if (format == PixelValueFormat::Integer)
		{
			if (channel == 1)
			{
				swprintf_s(buffer, bufferCount, L"Gray : %I64d", value[0].i);
			}
			else if (channel == 2)
			{
				swprintf_s(buffer, bufferCount, L"C0 : %I64d, C1 : %I64d", value[0].i, value[1].i);
			}
			else if (channel == 3)
			{
				swprintf_s(buffer, bufferCount, L"R : %I64d, G : %I64d, B : %I64d", value[0].i, value[1].i, value[2].i);
			}
			else if (channel == 4)
			{
				swprintf_s(buffer, bufferCount, L"R : %I64d, G : %I64d, B : %I64d, A : %I64d", value[0].i, value[1].i, value[2].i, value[3].i);
			}
		}
		else if (format == PixelValueFormat::Float)
		{
			if (channel == 1)
			{
				swprintf_s(buffer, bufferCount, L"Gray : %.4lf", value[0].f);
			}
			else if (channel == 2)
			{
				swprintf_s(buffer, bufferCount, L"C0 : %.4lf, C1 : %.4lf", value[0].f, value[1].f);
			}
			else if (channel == 3)
			{
				swprintf_s(buffer, bufferCount, L"R : %.4lf, G : %.4lf, B : %.4lf", value[0].f, value[1].f, value[2].f);
			}
			else if (channel == 4)
			{
				swprintf_s(buffer, bufferCount, L"R : %.4lf, G : %.4lf, B : %.4lf, A : %.4lf", value[0].f, value[1].f, value[2].f, value[3].f);
			}
		}

		m_colorLabel->SetText(buffer);
	}
}

void UIRenderLayer::UpdateStatusbarImageZoom(const float zoom)
{
	wchar_t buffer[20] = {};
	swprintf_s(buffer, 20, L"%.2f %%", zoom);

	if (m_zoomLabel)
	{
		m_zoomLabel->SetText(buffer);
	}
}

void UIRenderLayer::UpdateStatusbarImageSize(uint32_t width, uint32_t height)
{
	wchar_t buffer[32] = {};
	swprintf_s(buffer, 32, L"%d x %d", width, height);

	if (m_imageSizeLabel)
	{
		m_imageSizeLabel->SetText(buffer);
	}
}







// 호스트가 자체 UI 를 쓰는 경우 뷰어 내장 패널을 숨긴다.
//
// WPF 등에서는 이미지 위에 호스트 UI 를 얹을 수 없으므로(HWND airspace),
// 툴바/상태바를 끄고 호스트가 이미지 바깥에 자기 UI 를 두는 구성을 쓴다.
void UIRenderLayer::SetToolbarVisible(bool visible)
{
	if (m_toolbarPanel)
	{
		m_toolbarPanel->SetVisible(visible);
	}
}

void UIRenderLayer::SetStatusBarVisible(bool visible)
{
	if (m_statusPanel)
	{
		m_statusPanel->SetVisible(visible);
	}
}

void UIRenderLayer::SetMeasureButtonActive(bool active)
{
	if (!m_measureButton)
	{
		return;
	}

	m_measureButton->SetStyle(active ? m_buttonActiveStyle : m_buttonNormalStyle);
}
