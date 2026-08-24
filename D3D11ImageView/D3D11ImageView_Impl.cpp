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

bool D3D11ImageView_Impl::Initialize(HWND hWndParent, const RECT& rect, DWORD style, D3D11RenderEngine* D3D11Engine)
{
	return Initialize(D3D11Engine, hWndParent, rect, style);
}

bool D3D11ImageView_Impl::Initialize(D3D11RenderEngine* D3D11Engine, HWND hWndParent, const RECT& rect, DWORD style)
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

	// Rendering Engine
	if (D3D11Engine)
	{
		m_renderEngine = D3D11Engine;
		m_ownsRenderEngine = false;
	}
	else
	{
		RenderEngineConfig renderEngineConfig;
		renderEngineConfig.initD2D = true;
		renderEngineConfig.initD3D = true;
#if defined(_DEBUG)
		renderEngineConfig.initDebugLayer = true;
#endif
		renderEngineConfig.initFontManager = true;

		m_renderEngine = new D3D11RenderEngine();
		if (!m_renderEngine)
			return failInitialize();
		m_ownsRenderEngine = true;

		if (!m_renderEngine->Initialize(renderEngineConfig))
			return failInitialize();
	}

	// Rendering Context
	m_renderContext = std::make_unique<D3D11RenderContext>(m_renderEngine);
	if (!m_renderContext->Initialize(m_hWnd))
		return failInitialize();

	// Image Tile Manager
	m_tileManager = std::make_unique<TileManager>();
	m_tileManager->Initialize(m_renderEngine->GetD3DDevice(), m_renderEngine->GetD3DDeviceContext());

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

	// Initialize 전에 등록된 ROI 이벤트 핸들러를 이제 붙인다.
	ApplyPendingROIEventHandler();

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

	// 프레임 페이싱은 Present(vsync) 가 담당한다. 이 값은 안전망 상한이다.
	//
	// 화면 주사율 근처(예: 60)로 두면 소프트웨어 타이머와 vblank 가 거의 같은
	// 주기로 위상 간섭을 일으켜 애니메이션이 오히려 불규칙해진다. 충분히 높게
	// 잡아 실제 제한은 vblank 가 걸도록 한다.
	// (D3D11RenderContext::SetVSyncEnabled 로 vsync 를 끄는 경우에는 이 값을
	//  실제 목표 FPS 로 내려야 한다)
	m_renderThread->SetRenderFPS(240.0);
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

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.texture)
	{
		m_pendingImageUpdate.texture->Release();
	}
	m_pendingImageUpdate.Reset();
	m_hasPendingImageUpdate = false;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	// 1. Detach device/resize listeners from RenderContext.
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

	// 2. Release GPU resources owned by render layers before shutting down the context.
	m_overlayLayer.reset();
	m_roiLayer.reset();
	m_selectionRectLayer.reset();
	m_imageLayer.reset();
	m_imageCenterLineLayer.reset();
	m_uiLayer.reset();

	m_layers.clear();

	// 3. Shutdown RenderContext and swap chain resources.
	if (m_renderContext)
	{
		m_renderContext->Shutdown();
		m_renderContext.reset();
	}

	// 4. Shutdown owned RenderEngine.
	if (m_ownsRenderEngine && m_renderEngine)
	{
		m_renderEngine->Shutdown(); // owned engine shutdown
		delete m_renderEngine;
	}
	m_renderEngine = nullptr;
	m_ownsRenderEngine = false;

	// 5. Release camera state.
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

bool D3D11ImageView_Impl::Render(uint64_t frameID, bool resumedFromIdle)
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

	// ── 애니메이션에 쓸 delta time
	//
	// 엔진 타이머는 Tick 사이의 실제 경과 시간을 준다. 그런데 렌더 스레드는
	// 할 일이 없으면 잠들기 때문에, 유휴 후 첫 프레임의 dt 에는 유휴 시간이
	// 통째로 들어있다(타이머가 0.1초로 클램프).
	//
	// 그대로 보간에 넣으면 Camera2D 의 보간 속도 14 기준
	//   k = 1 - exp(-14 * 0.1) = 0.75
	// 즉 첫 프레임에 목표까지 거리의 75% 를 소비해서, 휠을 한 칸 돌렸을 때
	// 애니메이션 없이 곧바로 목표 배율로 튀는 것처럼 보인다.
	// (휠을 연타하면 스레드가 잠들지 않아 dt 가 정상이고 부드럽게 보인다)
	//
	// 애니메이션은 휠을 돌린 순간부터 시작해야 하므로, 그 앞의 유휴 시간은
	// 진행에 포함하지 않는다. 이 프레임은 시작 상태를 그대로 그리고,
	// 다음 프레임부터 정상 dt 로 보간이 진행된다.
	const float dt = resumedFromIdle ? 0.0f : m_renderContext->GetDeltaTime();

	const bool isCameraAnimating = m_camera->Update(dt);

	// 이미지 교체와 카메라 갱신 뒤에 반영한다. 그래야 방금 붙은 이미지와
	// 이 프레임이 실제로 그릴 카메라 상태를 기준으로 좌표/픽셀값을 읽는다.
	// Prepare() 보다는 앞이어야 바뀐 텍스트의 레이아웃이 이 프레임에 잡힌다.
	ApplyPendingStatusbarUpdate();

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

bool D3D11ImageView_Impl::GetPixelValueForStatusbar(const ImageBase* image, int32_t x, int32_t y, int32_t channel, PixelValue outValue[4]) const
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

// 호출자(UI) 스레드용. 좌표만 적어두고 프레임을 요청한다.
//
// 실제 갱신을 여기서 하면 안 된다. 상태바는 세 가지를 건드리는데
// 전부 렌더 스레드 소유다.
//   1) m_imageLayer 의 ImageBase  - 렌더 스레드가 Attach/Detach 로 갈아끼운다
//   2) m_camera                   - 렌더 스레드가 매 프레임 Update 한다
//   3) UILabel::m_text            - 렌더 스레드가 DWrite 레이아웃으로 읽는다
// UI 스레드에서 직접 만지면 (1) 은 해제된 버퍼 역참조, (3) 은 delete[] 된
// 버퍼 역참조가 된다. 그래서 좌표만 넘기고 판단은 렌더 스레드에 맡긴다.
void D3D11ImageView_Impl::UpdateStatusbar(int32_t mouseX, int32_t mouseY)
{
	// x, y 를 한 번의 원자적 쓰기로 묶어 찢어진 좌표가 보이지 않게 한다.
	const uint64_t packed =
		(static_cast<uint64_t>(static_cast<uint32_t>(mouseY)) << 32) |
		static_cast<uint64_t>(static_cast<uint32_t>(mouseX));

	m_pendingStatusbarPos.store(packed, std::memory_order_relaxed);
	m_hasPendingStatusbarUpdate.store(true, std::memory_order_release);

	InvalidateFrame();
}

// 렌더 스레드 전용. Render() 안에서 m_renderLock 을 쥔 채로 호출된다.
void D3D11ImageView_Impl::ApplyPendingStatusbarUpdate()
{
	if (!m_hasPendingStatusbarUpdate.exchange(false, std::memory_order_acquire))
		return;

	if (!m_camera || !m_uiLayer || !m_imageLayer)
		return;

	const uint64_t packed = m_pendingStatusbarPos.load(std::memory_order_relaxed);
	const int32_t mouseX = static_cast<int32_t>(static_cast<uint32_t>(packed & 0xFFFFFFFFull));
	const int32_t mouseY = static_cast<int32_t>(static_cast<uint32_t>(packed >> 32));

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

	// 여기서 InvalidateFrame 을 부르면 안 된다. 렌더 스레드가 자기 자신을
	// 다시 더럽혀 프레임이 끝없이 돌아간다. 프레임 요청은 좌표를 적어넣는
	// UpdateStatusbar 쪽(UI 스레드)이 담당한다.
}

bool D3D11ImageView_Impl::RenderCallback(void* param)
{
	RenderContext* renderContext = static_cast<RenderContext*>(param);

	D3D11ImageView_Impl* D3D11ImageView = static_cast<D3D11ImageView_Impl*>(renderContext->imageViewImpl);
	if (D3D11ImageView)
	{
		return D3D11ImageView->Render(renderContext->frameID, renderContext->resumedFromIdle);
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








