#include "pch.h"
#include "D3D11ImageView_Impl.h"

// Interface
#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IUIRenderLayer.h"

// Engine & Context
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"

// Camera
#include "../../../Module/D3D11Engine/Camera/Camera2D.h"

// UI Event

#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Event/UIEventDispatcher.h"
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Event/UIEventResult.h"

// Layer
#include "../Render Layer/ImageRenderLayer.h"
#include "../Render Layer/SelectionRectRenderLayer.h"
#include "../Render Layer/OverlayRenderLayer.h"
#include "../Render Layer/ImageCenterRenderLayer.h"
#include "../Render Layer/ROIRenderLayer.h"
#include "../Render Layer/UIRenderLayer.h"

#include "../Image Tile/TilePool.h"
#include "../Image Tile/TileManager.h"

// RenderThread
#include "RenderThread.h"

// Type
#include "../../../Module/Core/ImageType/Image_8U_C1.h"
#include "../../../Module/Core/ImageType/Image_8U_C3.h"
#include "../../../Module/Core/ImageType/Image_8U_C4.h"

using namespace Core::ShapeType;
using namespace Core::ImageType;

D3D11ImageView_Impl::D3D11ImageView_Impl()
{

}

D3D11ImageView_Impl::~D3D11ImageView_Impl()
{
	if (m_hWnd && ::IsWindow(m_hWnd))
	{
		::DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
}

bool D3D11ImageView_Impl::Initialize(HWND hWndParent, const RECT& rect, DWORD style)
{
	m_isFinalized = false;

	//DWORD style = WS_CHILD | WS_VISIBLE;

	// Window
	if (!WindowBase::Create(
		0,
		L"D3D11ImageViewClass",
		L"D3D11ImageViewWindow",
		style,
		CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		rect,
		hWndParent,
		GetModuleHandle(nullptr)))
		return false;

	auto failInitialize = [this]() -> bool
		{
			if (m_hWnd && ::IsWindow(m_hWnd))
			{
				::DestroyWindow(m_hWnd);
			}
			else
			{
				Finalize();
			}

			return false;
		};

	timeBeginPeriod(1);
	m_timePeriodSet = true;

	RECT clientRect = {};
	uint32_t clientWidth = 0;
	uint32_t clientHeight = 0;
	if (GetClientRect(m_hWnd, &clientRect))
	{
		clientWidth = static_cast<uint32_t>(clientRect.right - clientRect.left);
		clientHeight = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
	}

	// Camera
	m_camera = std::make_unique<Camera2D>();

	if (!m_camera)
		return failInitialize();

	m_camera->SetViewSize(clientWidth, clientHeight);

	TileSystemDesc tileSystemDesc;
	tileSystemDesc.maxLOD = 4;
	tileSystemDesc.lods =
	{
		{512, 2 * 100}, // LOD 0 (?癒?궚, 揶쎛????돦??
		{512, 2 * 50},  // LOD 1
		{512, 2 * 25},  // LOD 2
		{512, 2 * 15},  // LOD 3
		{512, 2 * 10},  // LOD 4 (椰꾧퀣???紐껉퐬??
	};

	// Rendering Engine
	RenderEngineConfig renderEngineConfig;
	renderEngineConfig.initD2D = true;
	renderEngineConfig.initD3D = true;
#if defined(_DEBUG)
	renderEngineConfig.initDebugLayer = true;
#endif
	renderEngineConfig.initFontManager = true;

	m_renderEngine = std::make_unique<D3D11RenderEngine>();
	if (!m_renderEngine->Initialize(renderEngineConfig))
		return failInitialize();

	// Rendering Context
	m_renderContext = std::make_unique<D3D11RenderContext>(m_renderEngine.get());
	if (!m_renderContext->Initialize(m_hWnd))
		return failInitialize();

	// Image Tile Manager 
	m_tileManager = std::make_unique<TileManager>();
	m_tileManager->Initialize(m_renderEngine->GetD3DDevice(), m_renderEngine->GetD3DDeviceContext(), tileSystemDesc);

	// Render Image Layer
	m_imageLayer = std::make_unique<ImageRenderLayer>();
	m_imageLayer->SetCamera2D(m_camera.get());
	m_imageLayer->SetTileManager(m_tileManager.get());
	if (!m_imageLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_imageLayer.get());

	// Render Selection Rectangle Layer
	m_selectionRectLayer = std::make_unique<SelectionRectRenderLayer>();
	if (!m_selectionRectLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_selectionRectLayer.get());

	// Render Overlay Layer
	m_overlayLayer = std::make_unique<OverlayRenderLayer>();
	m_overlayLayer->SetCamera2D(m_camera.get());

	if (!m_overlayLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_overlayLayer.get());

	// Render ROI Layer
	m_roiLayer = std::make_unique<ROIRenderLayer>();
	m_roiLayer->SetCamera2D(m_camera.get());

	if (!m_roiLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_roiLayer.get());

	// Render Image Center Line Layer
	m_imageCenterLineLayer = std::make_unique<ImageCenterRenderLayer>();
	m_imageCenterLineLayer->SetCamera2D(m_camera.get());

	if (!m_imageCenterLineLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_imageCenterLineLayer.get());

	// UI Event Dispatch
	m_uiEventDispatcher = std::make_unique<UIEventDispatcher>();
	m_uiEventDispatcher->RegisterCallback(&D3D11ImageView_Impl::OnUICommand, this);

	// Render UI
	m_uiLayer = std::make_unique<UIRenderLayer>();
	m_uiLayer->SetCamera2D(m_camera.get());
	m_uiLayer->SetEventDispatcher(m_uiEventDispatcher.get());

	if (!m_uiLayer->Initialize(m_renderContext.get()))
		return failInitialize();

	m_layers.push_back(m_uiLayer.get());

	// Render Thread
	m_renderThread = std::make_unique<RenderThread>();

	// Request Render
	InvalidateFrame();

	m_renderThread->SetRenderFPS(120.0);
	m_renderThread->SetRenderFunction(&D3D11ImageView_Impl::RenderCallback, this);

	if (!m_renderThread->StartThread())
		return failInitialize();

	return true;
}

void D3D11ImageView_Impl::Finalize()
{
	if (m_isFinalized)
		return;

	m_isFinalized = true;

	if (m_renderThread)
		m_renderThread->StopThread();

	// 1. ??됱뵠??? RenderContext?癒?퐣 ?브쑬??
	if (m_renderContext)
	{
		for (IRenderLayer* layer : m_layers)
		{
			if (!layer)
				continue;

			if (IDeviceEventListener* dev = dynamic_cast<IDeviceEventListener*>(layer))
				m_renderContext->RemoveDeviceListener(dev);

			if (IResizeEventListener* res = dynamic_cast<IResizeEventListener*>(layer))
				m_renderContext->RemoveResizeListener(res);
		}
	}

	// 2. ??됱뵠??GPU ?귐딅꺖????곸젫 (engine ??곷툡??됱뱽 ??
	m_overlayLayer.reset();
	m_roiLayer.reset();
	m_selectionRectLayer.reset();
	m_imageLayer.reset();
	m_imageCenterLineLayer.reset();
	m_uiLayer.reset();

	m_layers.clear();

	// 3. RenderContext ?ル굝利?(swapchain ??釉?
	if (m_renderContext)
	{
		m_renderContext->Shutdown();
		m_renderContext.reset();
	}

	// 4. RenderEngine ?ル굝利?
	if (m_renderEngine)
	{
		m_renderEngine->Shutdown(); // ??덈뼄筌?
		m_renderEngine.reset();
	}

	// 5. Camera ??곸젫
	if (m_camera)
	{
		m_camera.reset();
	}

	m_tileManager.reset();
	m_renderThread.reset();
	m_uiEventDispatcher.reset();

	if (m_timePeriodSet)
	{
		timeEndPeriod(1);
		m_timePeriodSet = false;
	}
}

HWND D3D11ImageView_Impl::GetHWND() const
{
	return WindowBase::GetHWND();
}

ID3D11Device* D3D11ImageView_Impl::GetDevice() const
{
	return m_renderEngine ? m_renderEngine->GetD3DDevice() : nullptr;
}

ID3D11DeviceContext* D3D11ImageView_Impl::GetDeviceContext() const
{
	return m_renderEngine ? m_renderEngine->GetD3DDeviceContext() : nullptr;
}

bool D3D11ImageView_Impl::Render(uint64_t frameID)
{
	if (!m_renderContext)
		return false;

	::AcquireSRWLockExclusive(&m_renderLock);

	m_renderContext->Tick();

	if (!ApplyPendingImageUpdate())
	{
		::ReleaseSRWLockExclusive(&m_renderLock);
		return false;
	}

	const float dt = m_renderContext->GetDeltaTime();
	const bool isCameraAnimating = m_camera->Update(dt);
	const bool isUiAnimating = m_uiLayer->Update(dt);

	m_uiLayer->Prepare();

	if (isCameraAnimating || isUiAnimating || m_isDirty)
	{
		m_isDirty = false;

		if (!m_renderContext->BeginFrame())
		{
			::ReleaseSRWLockExclusive(&m_renderLock);
			return false;
		}

		m_imageLayer->SetFrameID(frameID);
		m_imageLayer->Render();
		if (m_imageLayer->IsImageRenderDirty())
		{
			m_isDirty = true;
			InvalidateFrame();
		}

		m_renderContext->BeginOverlay();

		m_overlayLayer->Render();

		m_selectionRectLayer->Render();

		if (m_showImageCenterLineLayer)
		{
			m_imageCenterLineLayer->Render();
		}

		m_roiLayer->Render();

		m_uiLayer->Render();

		m_renderContext->EndOverlay();

		m_renderContext->EndFrame();
	}

	::ReleaseSRWLockExclusive(&m_renderLock);

	return isCameraAnimating || isUiAnimating || m_isDirty;
}

UIEventResult D3D11ImageView_Impl::HandleMouseEventUI(UIMouseEventType type, int32_t mousePosX, int32_t mousePosY)
{
	UIEventResult uiEventResult = UIEventResult::None;

	if (m_uiLayer)
	{
		uiEventResult = m_uiLayer->OnMouseEvent(
			type,
			static_cast<float>(mousePosX),
			static_cast<float>(mousePosY));

		if (uiEventResult != UIEventResult::None)
		{
			// UI button, panel, icon etc.. hit success
			InvalidateFrame();
		}
	}

	return uiEventResult;
}

void D3D11ImageView_Impl::OnUICommand(UICommand command, void* userData)
{
	auto* self = static_cast<D3D11ImageView_Impl*>(userData);

	if (!self)
		return;

	self->HandleUICommand(command);
}

void D3D11ImageView_Impl::HandleUICommand(UICommand command)
{
	switch (command)
	{
	case UICommand::None:
		break;
	case UICommand::ZoomIn:
		ZoomIn();
		break;
	case UICommand::ZoomOut:
		ZoomOut();
		break;
	case UICommand::Zoom1to1:
		Zoom1To1();
		break;
	case UICommand::ZoomFit:
		ZoomFit();
		break;
	case UICommand::ImageCenterCrossLine:
		ToggleImageCenterCrossLine();
		break;

	default:
		break;
	}
}

void D3D11ImageView_Impl::Zoom(float zoomFactor)
{
	if (!m_camera || m_layers.empty())
		return;

	InvalidateFrame();

	m_camera->Zoom(zoomFactor);

	UpdateStatusbar(-1, -1);

	InvalidateFrame();
}

void D3D11ImageView_Impl::ZoomIn()
{
	constexpr float zoomSpeed = m_defaultZoomFactor;

	float zoomFactor = expf(zoomSpeed);

	InvalidateFrame();

	Zoom(zoomFactor);

	InvalidateFrame();
}

void D3D11ImageView_Impl::ZoomOut()
{
	constexpr float zoomSpeed = m_defaultZoomFactor;

	float zoomFactor = expf(-zoomSpeed);

	Zoom(zoomFactor);

	InvalidateFrame();
}

void D3D11ImageView_Impl::Zoom1To1()
{
	if (!m_camera || m_layers.empty())
		return;

	m_camera->Zoom1to1();

	UpdateStatusbar(-1, -1);

	InvalidateFrame();
}

void D3D11ImageView_Impl::ZoomFit()
{
	if (!m_camera || m_layers.empty())
		return;

	m_camera->Fit();

	UpdateStatusbar(-1, -1);

	InvalidateFrame();
}

void D3D11ImageView_Impl::Zoom(float zoomFactor, int32_t mousePosX, int32_t mousePosY)
{
	if (!m_camera || m_layers.empty())
		return;

	m_camera->Zoom(zoomFactor, static_cast<float>(mousePosX), static_cast<float>(mousePosY));

	UpdateStatusbar(mousePosX, mousePosY);

	InvalidateFrame();
}

void D3D11ImageView_Impl::Zoom1To1(int32_t mousePosX, int32_t mousePosY)
{
	if (!m_camera || m_layers.empty())
		return;

	m_camera->Zoom1to1(static_cast<float>(mousePosX), static_cast<float>(mousePosY));

	UpdateStatusbar(mousePosX, mousePosY);

	InvalidateFrame();
}

void D3D11ImageView_Impl::BeginPan(int32_t mouseX, int32_t mouseY)
{
	m_camera->BeginPan(static_cast<float>(mouseX), static_cast<float>(mouseY));

	InvalidateFrame();
}

void D3D11ImageView_Impl::UpdatePan(int32_t mouseX, int32_t mouseY)
{
	float dt = m_renderContext->GetDeltaTime();

	m_camera->UpdatePan(static_cast<float>(mouseX), static_cast<float>(mouseY), dt);

	InvalidateFrame();
}

void D3D11ImageView_Impl::EndPan(int32_t mouseX, int32_t mouseY)
{
	m_camera->EndPan();

	InvalidateFrame();
}

void D3D11ImageView_Impl::BeginSelection(const Point2i& point)
{
	if (!m_camera || m_layers.empty())
		return;

	m_selectionRectLayer->OnLButtonDown(point);

	InvalidateFrame();
}

void D3D11ImageView_Impl::UpdateSelection(const Point2i& point)
{
	if (!m_camera || m_layers.empty())
		return;

	m_selectionRectLayer->OnMouseMove(point);

	InvalidateFrame();
}

void D3D11ImageView_Impl::EndSelection(const Point2i& point)
{
	if (!m_camera || m_layers.empty())
		return;

	m_selectionRectLayer->OnLButtonUp(point);

	InvalidateFrame();
}

void D3D11ImageView_Impl::ToggleImageCenterCrossLine()
{
	if (!m_camera || m_layers.empty())
		return;

	m_showImageCenterLineLayer = !m_showImageCenterLineLayer;

	InvalidateFrame();
}

void D3D11ImageView_Impl::ShowImageCenterCrossLine()
{
	if (!m_camera || m_layers.empty())
		return;

	m_showImageCenterLineLayer = true;

	InvalidateFrame();
}

void D3D11ImageView_Impl::HideImageCenterCrossLine()
{
	if (!m_camera || m_layers.empty())
		return;

	m_showImageCenterLineLayer = false;

	InvalidateFrame();
}

bool D3D11ImageView_Impl::GetPixelValueForStatusbar(const ImageBase* image, int32_t x, int32_t y, int32_t channel, PixelValue outValue[4])
{
	if (!image || image->IsEmpty() || !image->IsInside(x, y))
		return false;

	switch (image->GetPixelType())
	{
	case PixelType::U8:
		FetchIntPixel<uint8_t>(image, x, y, channel, outValue);
		break;
	case PixelType::U16:
		FetchIntPixel<uint16_t>(image, x, y, channel, outValue);
		break;
	case PixelType::F32:
		FetchFloatPixel(image, x, y, channel, outValue);
		break;
	default:
		break;
	}

	return true;
}

void D3D11ImageView_Impl::UpdateStatusbar(int32_t mouseX, int32_t mouseY)
{
	if (!m_camera || !m_uiLayer)
		return;

	int32_t imageCoordinateX(0), imageCoordinateY(0);

	m_uiLayer->UpdateStatusbarImageZoom(m_camera->GetZoomPercent());

	if (mouseX >= 0 && mouseY >= 0 &&
		m_camera->ScreenToImagePixel(
			static_cast<float>(mouseX), static_cast<float>(mouseY),
			imageCoordinateX, imageCoordinateY))
	{
		m_uiLayer->UpdateStatusbarImagePosition(imageCoordinateX, imageCoordinateY);

		const Core::ImageType::ImageBase* image = m_imageLayer->GetImage();
		if (image)
		{
			PixelValue value[4];

			if (GetPixelValueForStatusbar(
				image,
				imageCoordinateX, imageCoordinateY,
				image->Channel(),
				value))
			{
				m_uiLayer->UpdateStatusbarImagePixelValue(value, image->Channel());
			}
		}
	}
	else
	{
		//m_uiLayer->ClearStatusbarImagePosition();
		//m_uiLayer->ClearStatusbarImagePixelValue();
	}

	InvalidateFrame();
}

bool D3D11ImageView_Impl::RenderCallback(void* param)
{
	RenderContext* renderContext = static_cast<RenderContext*>(param);

	D3D11ImageView_Impl* D3D11ImageView = static_cast<D3D11ImageView_Impl*>(renderContext->imageViewImpl);
	if (D3D11ImageView)
	{
		uint64_t frameId = renderContext->frameID;
		return D3D11ImageView->Render(frameId);
	}

	return false;
}

void D3D11ImageView_Impl::InvalidateFrame()
{
	m_isDirty = true;

	if (m_renderThread)
	{
		m_renderThread->RequestFrame();
	}
}








