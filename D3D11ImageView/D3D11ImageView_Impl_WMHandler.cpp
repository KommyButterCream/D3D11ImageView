#include "pch.h"
#include "D3D11ImageView_Impl.h"

#include <windowsx.h>

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IUIRenderLayer.h"


#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"

// UI Event
#include "../../../Module/D3D11UIFramework/D3D11UIFramework/Event/UIEventResult.h"

#include "../Render Layer/ImageRenderLayer.h"
#include "../Render Layer/ROIRenderLayer.h"
#include "../Render Layer/UIRenderLayer.h"
#include "../Render Layer/SelectionRectRenderLayer.h"


using namespace Core::ShapeType;

// WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE 최적화
LRESULT D3D11ImageView_Impl::WndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		//case WM_COMMAND:		return OnCommand(wParam, lParam);
	case WM_CREATE:			return OnCreate(wParam, lParam);
	case WM_CLOSE:			return OnClose(wParam, lParam);
	case WM_DESTROY:		return OnDestroy(wParam, lParam);
	case WM_ERASEBKGND:		return OnEraseBkgnd(wParam, lParam);
	case WM_LBUTTONDBLCLK:	return OnLButtonDblClk(wParam, lParam);
	case WM_LBUTTONDOWN:	return OnLButtonDown(wParam, lParam);
	case WM_LBUTTONUP:		return OnLButtonUp(wParam, lParam);
	case WM_MOUSEMOVE:		return OnMouseMove(wParam, lParam);
	case WM_MOUSELEAVE:		return OnMouseLeave(wParam, lParam);
	case WM_MOUSEWHEEL:		return OnMouseWheel(wParam, lParam);
	case WM_CAPTURECHANGED:	return OnCaptureChanged(wParam, lParam);
	case WM_NCDESTROY:		return OnNcDestroy(wParam, lParam);
	case WM_PAINT:			return OnPaint(wParam, lParam);
	case WM_RBUTTONDOWN:	return OnRButtonDown(wParam, lParam);
	case WM_RBUTTONUP:		return OnRButtonUp(wParam, lParam);
	case WM_SETCURSOR:		return OnSetCursor(wParam, lParam);
	case WM_SIZE:			return OnSize(wParam, lParam);
	case WM_TIMER:			return OnTimer(wParam, lParam);
	case WM_KEYDOWN:		return OnKeyDown(wParam, lParam);
	case WM_D3IV_SAVE_IMAGE:	return OnSaveImageRequested(wParam, lParam);
	}

	return __super::WndProc(message, wParam, lParam);
}

LRESULT D3D11ImageView_Impl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	DWORD commandId = LOWORD(wParam);
	DWORD commandEvent = HIWORD(wParam);

	UNREFERENCED_PARAMETER(commandId);
	UNREFERENCED_PARAMETER(commandEvent);
	UNREFERENCED_PARAMETER(lParam);

	return 1L;
}

LRESULT D3D11ImageView_Impl::OnCreate(WPARAM wParam, LPARAM lParam)
{

	return 0L;
}

// 사용자가 창을 닫으려 한다(닫기 버튼, Alt+F4, 시스템 메뉴).
//
// 호스트에게 먼저 알린 뒤 기본 처리로 넘긴다. 순서가 중요하다 — 파괴가
// 시작되기 전에 알려야 호스트가 프레임 공급을 끊을 기회를 갖는다.
//
// 여기서 PostQuitMessage 를 부르지 않는 이유: 이 뷰어는 호스트 창의 자식으로
// 얹히기도 하는데, 그때 WM_QUIT 를 보내면 호스트의 메시지 루프까지 끝난다.
// 종료 여부는 호스트가 정할 일이다.
LRESULT D3D11ImageView_Impl::OnClose(WPARAM wParam, LPARAM lParam)
{
	const CloseHandler handler = m_closeHandler;
	void* const userData = m_closeUserData;

	if (handler)
	{
		handler(userData);
	}

	// DefWindowProc 가 DestroyWindow 를 부른다.
	return __super::WndProc(WM_CLOSE, wParam, lParam);
}

LRESULT D3D11ImageView_Impl::OnDestroy(WPARAM wParam, LPARAM lParam)
{
	Finalize();

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnEraseBkgnd(WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

// 키보드 단축키.
//
// 컨텍스트 메뉴가 Ctrl +, Ctrl -, Ctrl 1 을 라벨로 광고하고 있었는데
// 실제로는 WM_KEYDOWN 핸들러 자체가 없어서 눌러도 아무 일이 없었다.
//
// 포커스는 OnLButtonDown 에서 가져온다. 자식 창이라 클릭 전에는 호스트가
// 키를 받는다.
LRESULT D3D11ImageView_Impl::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	// GetKeyState 의 최상위 비트가 눌림이다.
	const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (!ctrl)
		return 0L;

	switch (wParam)
	{
	// 메인 키보드의 '+' 는 Shift 없이 누르면 '=' 자리라 VK_OEM_PLUS 로 온다.
	// 숫자패드는 VK_ADD 다. 둘 다 받는다.
	case VK_OEM_PLUS:
	case VK_ADD:
		ZoomIn();
		return 0L;

	case VK_OEM_MINUS:
	case VK_SUBTRACT:
		ZoomOut();
		return 0L;

	case '1':
	case VK_NUMPAD1:
		Zoom1To1();
		return 0L;

	case '0':
	case VK_NUMPAD0:
		ZoomFit();
		return 0L;

	default:
		break;
	}

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnLButtonDblClk(WPARAM wParam, LPARAM lParam)
{
	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	if (HandleMouseEventUI(UIMouseEventType::LButtonDoubleDown, mousePosition.x, mousePosition.y)
		!= UIEventResult::None)
	{
		if (HandleMouseEventUI(UIMouseEventType::LButtonDown, mousePosition.x, mousePosition.y)
			!= UIEventResult::None)
		{
			return 0L;
		}
	}

	const bool isControl = ::GetAsyncKeyState(VK_LCONTROL) < 0;
	const bool isShift = ::GetAsyncKeyState(VK_LSHIFT) < 0;

	if (isControl && isShift)
	{
		// Control + Shift + 더블클릭 = Zoom-out
		Zoom(expf(-m_defaultZoomFactor), mousePosition.x, mousePosition.y);
	}
	else if (isControl)
	{
		// Control + 더블 클릭 = Zoom-in
		Zoom(expf(m_defaultZoomFactor), mousePosition.x, mousePosition.y);
	}
	else
	{
		// 더블 클릭 = 1:1 Zoom-in
		Zoom1To1(mousePosition.x, mousePosition.y);
	}

	return 1L;
}

LRESULT D3D11ImageView_Impl::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	// 키보드 포커스를 가져온다.
	//
	// 이게 없으면 WM_KEYDOWN 이 호스트로 가고 뷰어의 단축키가 영영 안 먹는다.
	// 자식 창 뷰어에서 이미지를 클릭하면 포커스가 오는 것이 일반적인 동작이다.
	if (m_hWnd && ::GetFocus() != m_hWnd)
	{
		::SetFocus(m_hWnd);
	}

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	UIEventResult uiEventResult = HandleMouseEventUI(UIMouseEventType::LButtonDown,	mousePosition.x, mousePosition.y);

	if (uiEventResult == UIEventResult::Toolbar ||
		uiEventResult == UIEventResult::ContextMenu)
	{
		return 0L;
	}

	if (DispatchMouseEvent(MouseEventType::LButtonDown, mousePosition.x, mousePosition.y))
	{
		return 0L;
	}

	// 거리 측정 모드가 켜져 있으면 좌클릭은 측정 전용이다.
	//
	// 캡처를 걸지 않는다. 드래그가 아니라 클릭 → 이동 → 클릭이라, 두 클릭
	// 사이에 우클릭 팬으로 화면을 옮길 수 있어야 한다.
	if (m_measureActive && m_roiLayer)
	{
		bool completed = false;
		if (m_roiLayer->MeasureOnClick(
			static_cast<float>(mousePosition.x),
			static_cast<float>(mousePosition.y), completed))
		{
			if (completed)
			{
				// 두 번째 점을 찍었다. 모드만 내리고 측정선은 남긴다.
				m_measureActive = false;

				if (m_uiLayer)
				{
					m_uiLayer->SetMeasureButtonActive(false);
				}
			}

			InvalidateFrame();
			return 0L;
		}
	}

	// 각도 측정도 같은 방식이다. 다만 점을 셋 찍는다.
	if (m_angleActive && m_roiLayer)
	{
		bool completed = false;
		if (m_roiLayer->AngleOnClick(
			static_cast<float>(mousePosition.x),
			static_cast<float>(mousePosition.y), completed))
		{
			if (completed)
			{
				// 세 번째 점을 찍었다. 모드만 내리고 측정 결과는 남긴다.
				m_angleActive = false;

				if (m_uiLayer)
				{
					m_uiLayer->SetAngleButtonActive(false);
				}
			}

			InvalidateFrame();
			return 0L;
		}
	}

	if (m_roiLayer && m_roiLayer->OnLButtonDown(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)))
	{
		m_mouseButtonMode = MouseButtonMode::LBUTTON_ROI_EDIT;
		::SetCapture(m_hWnd);
		InvalidateFrame();
		return 0L;
	}

	m_lButtonDown = mousePosition;

	m_mouseButtonMode = MouseButtonMode::LBUTTON_SELECTION;

	switch (m_mouseButtonMode)
	{
	case MouseButtonMode::LBUTTON_SELECTION:
		BeginSelection(m_lButtonDown);
		break;

	}

	::SetCapture(m_hWnd);

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	if (HandleMouseEventUI(UIMouseEventType::LButtonUp, mousePosition.x, mousePosition.y) != UIEventResult::None)
	{
		return 0L;
	}

	if (DispatchMouseEvent(MouseEventType::LButtonUp, mousePosition.x, mousePosition.y))
	{
		return 0L;
	}

	if (m_mouseButtonMode == MouseButtonMode::LBUTTON_ROI_EDIT)
	{
		if (m_roiLayer && m_roiLayer->OnLButtonUp(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)))
		{
			InvalidateFrame();
		}

		m_mouseButtonMode = MouseButtonMode::NOTHING;
		::ReleaseCapture();
		return 0L;
	}

	m_lButtonDragPoint = mousePosition;

	switch (m_mouseButtonMode)
	{
	case MouseButtonMode::LBUTTON_SELECTION:
		EndSelection(m_lButtonDragPoint);
		break;
	}

	m_mouseButtonMode = MouseButtonMode::NOTHING;

	::ReleaseCapture();

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	// 마우스가 창을 벗어나는 경우에 Leave 이벤트를 받을 수 있도록 한다.
	// WM_MOUSELEAVE 를 한 번 받으려면 매번 등록 해주어야 한다.
	// TME_LEAVE 는 한 번 발생하면 자동으로 해제된다.
	if (!m_isMouseTracking)
	{
		TRACKMOUSEEVENT trackMouseEvent = {};
		trackMouseEvent.cbSize = sizeof(trackMouseEvent);
		trackMouseEvent.dwFlags = TME_LEAVE;
		trackMouseEvent.hwndTrack = m_hWnd;

		if (::TrackMouseEvent(&trackMouseEvent))
		{
			m_isMouseTracking = true;
		}
	}

	const UIEventResult uiEventResult = HandleMouseEventUI(UIMouseEventType::Move, mousePosition.x, mousePosition.y);

	if (uiEventResult != UIEventResult::None)
	{
		return 0L;
	}

	if (DispatchMouseEvent(MouseEventType::Move, mousePosition.x, mousePosition.y))
	{
		return 0L;
	}

	// 끝점을 찍기 전까지는 끝점이 마우스를 따라간다.
	// hover 재계산은 건너뛴다 — 지금 중요한 건 만들고 있는 선뿐이다.
	if (m_measureActive && m_roiLayer && m_roiLayer->IsMeasurePlacingPoint())
	{
		if (m_roiLayer->MeasureOnMouseMove(
			static_cast<float>(mousePosition.x),
			static_cast<float>(mousePosition.y)))
		{
			InvalidateFrame();
		}

		UpdateStatusbar(mousePosition.x, mousePosition.y);
		return 0L;
	}

	// 각도 측정도 마찬가지다. 아직 안 찍은 점이 마우스를 따라간다.
	if (m_angleActive && m_roiLayer && m_roiLayer->IsAnglePlacingPoint())
	{
		if (m_roiLayer->AngleOnMouseMove(
			static_cast<float>(mousePosition.x),
			static_cast<float>(mousePosition.y)))
		{
			InvalidateFrame();
		}

		UpdateStatusbar(mousePosition.x, mousePosition.y);
		return 0L;
	}

	if (m_roiLayer)
	{
		const bool roiStateChanged = m_roiLayer->OnMouseMove(static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y));
		if (roiStateChanged)
		{
			InvalidateFrame();
		}

		if (m_mouseButtonMode == MouseButtonMode::LBUTTON_ROI_EDIT)
		{
			return 0L;
		}
	}

	m_lButtonDragPoint = mousePosition;

	switch (m_mouseButtonMode)
	{
	case MouseButtonMode::NOTHING:
		UpdateStatusbar(mousePosition.x, mousePosition.y);
		break;
	case MouseButtonMode::LBUTTON_SELECTION:
		UpdateSelection(m_lButtonDragPoint);
		UpdateStatusbar(mousePosition.x, mousePosition.y);
		break;
	case MouseButtonMode::RBUTTON_PANNING:
		UpdatePan(mousePosition.x, mousePosition.y);
		break;
	default:
		break;
	}

	m_rButtonDown = mousePosition;

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	m_isMouseTracking = false;

	// UI 요소의 hover 상태를 푼다. 좌표는 창 밖이므로 어떤 요소에도
	// 맞지 않는 값을 준다.
	HandleMouseEventUI(UIMouseEventType::Leave, -1, -1);

	if (m_roiLayer)
	{
		// ROI hover 해제. 드래그 중이면 캡처가 살아 있어 애초에
		// WM_MOUSELEAVE 가 오지 않는다.
		if (m_roiLayer->OnMouseMove(-1.0f, -1.0f))
		{
			InvalidateFrame();
		}
	}

	// 상태바를 창 밖 상태로 갱신 (좌표/픽셀값은 유지하고 줌만 반영)
	UpdateStatusbar(-1, -1);

	return 0L;
}

// 캡처를 잃었다.
// 팬/선택/ROI 편집은 모두 SetCapture 를 잡고 시작하는데, Alt+Tab 이나 다른
// 창이 캡처를 가져가면 WM_LBUTTONUP / WM_RBUTTONUP 이 오지 않는다. 그러면
// m_mouseButtonMode 가 그대로 남아 버튼을 뗀 뒤에도 팬이 계속되는 것처럼
// 보인다. 여기서 진행 중이던 동작을 정리한다.
LRESULT D3D11ImageView_Impl::OnCaptureChanged(WPARAM wParam, LPARAM lParam)
{
	// 우리가 캡처를 얻는 경우는 정리 대상이 아니다.
	if (reinterpret_cast<HWND>(lParam) == m_hWnd)
		return 0L;

	if (m_mouseButtonMode == MouseButtonMode::NOTHING)
		return 0L;

	// 마지막으로 알던 위치로 진행 중인 동작을 닫는다.
	switch (m_mouseButtonMode)
	{
	case MouseButtonMode::LBUTTON_ROI_EDIT:
		if (m_roiLayer)
		{
			m_roiLayer->OnLButtonUp(
				static_cast<float>(m_lButtonDragPoint.x),
				static_cast<float>(m_lButtonDragPoint.y));
		}
		break;

	case MouseButtonMode::LBUTTON_SELECTION:
		EndSelection(m_lButtonDragPoint);
		break;

	case MouseButtonMode::RBUTTON_PANNING:
		EndPan(m_rButtonDown.x, m_rButtonDown.y);
		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
		break;

	default:
		break;
	}

	m_mouseButtonMode = MouseButtonMode::NOTHING;

	InvalidateFrame();

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
	// Screen 좌표계
	// 사용자 체감이 더욱 정확하게 하기 위해 MouseWheel 은 GetCursorPos 사용
	// 실시간으로 마우스 위치를 가져온다.

	POINT mousePosition = {};
	short delta = GET_WHEEL_DELTA_WPARAM(wParam);

	::GetCursorPos(&mousePosition);
	::ScreenToClient(m_hWnd, &mousePosition);

	// 호스트가 휠을 선점하면 줌하지 않는다.
	// (예: Ctrl+휠로 자체 기능을 넣는 경우)
	if (DispatchMouseEvent(MouseEventType::Wheel,
		mousePosition.x, mousePosition.y, static_cast<int32_t>(delta)))
	{
		return 0L;
	}

	int wheelStep = delta / WHEEL_DELTA;

	float zoomSpeed = m_defaultZoomFactor;

	if (::GetAsyncKeyState(VK_SHIFT) & 0x8000) zoomSpeed *= 2.0f;
	if (::GetAsyncKeyState(VK_CONTROL) & 0x8000) zoomSpeed *= 0.5f;

	float zoomFactor = expf(zoomSpeed * static_cast<float>(wheelStep));

	Zoom(zoomFactor, mousePosition.x, mousePosition.y);

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnNcDestroy(WPARAM wParam, LPARAM lParam)
{
	::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, 0);
	m_hWnd = nullptr;

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnPaint(WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps = {};

	::BeginPaint(m_hWnd, &ps);

	//Render(); // 별도의 렌더링 스레드에서 렌더 수행

	::EndPaint(m_hWnd, &ps);

	return 1L;
}

LRESULT D3D11ImageView_Impl::OnRButtonDown(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	if (HandleMouseEventUI(UIMouseEventType::RButtonDown, mousePosition.x, mousePosition.y)
		!= UIEventResult::None)
	{
		//return 0L;
	}

	if (DispatchMouseEvent(MouseEventType::RButtonDown, mousePosition.x, mousePosition.y))
	{
		return 0L;
	}

	m_rButtonDown = mousePosition;
	m_rButtonDownInitial = mousePosition;

	BeginPan(mousePosition.x, mousePosition.y);

	m_mouseButtonMode = MouseButtonMode::RBUTTON_PANNING;

	::SetCapture(m_hWnd);
	::SetCursor(::LoadCursor(nullptr, IDC_HAND));

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnRButtonUp(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	::ReleaseCapture();

	::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

	EndPan(mousePosition.x, mousePosition.y);

	m_mouseButtonMode = MouseButtonMode::NOTHING;

	if (m_rButtonDownInitial == mousePosition)
	{
		if (HandleMouseEventUI(UIMouseEventType::RButtonUp, mousePosition.x, mousePosition.y)
			!= UIEventResult::None)
		{
			//return 0L;
		}
	}
	else
	{

	}


	return 0L;
}

LRESULT D3D11ImageView_Impl::OnSetCursor(WPARAM wParam, LPARAM lParam)
{
	return LRESULT();
}

LRESULT D3D11ImageView_Impl::OnSize(WPARAM wParam, LPARAM lParam)
{
	if (wParam == SIZE_MINIMIZED)
		return 0L;

	uint32_t width = LOWORD(lParam);
	uint32_t height = HIWORD(lParam);

	if (m_renderContext)
	{
		m_renderContext->RequestResize(width, height);
		InvalidateFrame();
	}

	return 1L;
}

LRESULT D3D11ImageView_Impl::OnTimer(WPARAM wParam, LPARAM lParam)
{
	return 0L;
}






