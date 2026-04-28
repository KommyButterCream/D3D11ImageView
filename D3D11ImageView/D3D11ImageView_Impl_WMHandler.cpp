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
#include "../Render Layer/SelectionRectRenderLayer.h"


using namespace Core::ShapeType;

// WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE 최적화
LRESULT D3D11ImageView_Impl::WndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		//case WM_COMMAND:		return OnCommand(wParam, lParam);
	case WM_CREATE:			return OnCreate(wParam, lParam);
	case WM_DESTROY:		return OnDestroy(wParam, lParam);
	case WM_ERASEBKGND:		return OnEraseBkgnd(wParam, lParam);
	case WM_LBUTTONDBLCLK:	return OnLButtonDblClk(wParam, lParam);
	case WM_LBUTTONDOWN:	return OnLButtonDown(wParam, lParam);
	case WM_LBUTTONUP:		return OnLButtonUp(wParam, lParam);
	case WM_MOUSEMOVE:		return OnMouseMove(wParam, lParam);
	case WM_MOUSEWHEEL:		return OnMouseWheel(wParam, lParam);
	case WM_NCDESTROY:		return OnNcDestroy(wParam, lParam);
	case WM_PAINT:			return OnPaint(wParam, lParam);
	case WM_RBUTTONDOWN:	return OnRButtonDown(wParam, lParam);
	case WM_RBUTTONUP:		return OnRButtonUp(wParam, lParam);
	case WM_SETCURSOR:		return OnSetCursor(wParam, lParam);
	case WM_SIZE:			return OnSize(wParam, lParam);
	case WM_TIMER:			return OnTimer(wParam, lParam);
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

LRESULT D3D11ImageView_Impl::OnDestroy(WPARAM wParam, LPARAM lParam)
{
	Finalize();

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnEraseBkgnd(WPARAM wParam, LPARAM lParam)
{
	return TRUE;
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

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	UIEventResult uiEventResult = HandleMouseEventUI(
		UIMouseEventType::LButtonDown,
		mousePosition.x, mousePosition.y);

	if (uiEventResult == UIEventResult::Toolbar ||
		uiEventResult == UIEventResult::ContextMenu)
	{
		return 0L;
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
	//::SetCursor(::LoadCursor(nullptr, IDC_HAND));

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	if (HandleMouseEventUI(UIMouseEventType::LButtonUp, mousePosition.x, mousePosition.y)
		!= UIEventResult::None)
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
	//::SetCursor(::LoadCursor(nullptr, IDC_ARROW));

	return 0L;
}

LRESULT D3D11ImageView_Impl::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
	// Client 좌표계

	const Point2i mousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

	const UIEventResult uiEventResult = HandleMouseEventUI(UIMouseEventType::Move, mousePosition.x, mousePosition.y);

	if (uiEventResult != UIEventResult::None)
	{
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

	//const int32_t differenceRButtonX = mousePosition.x - m_RButtonDown.x;
	//const int32_t differenceRButtonY = mousePosition.y - m_RButtonDown.y;

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

LRESULT D3D11ImageView_Impl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
	// Screen 좌표계
	// 사용자 체감이 더욱 정확하게 하기 위해 MouseWheel 은 GetCursorPos 사용
	// 실시간으로 마우스 위치를 가져온다.

	POINT mousePosition = {};
	short delta = GET_WHEEL_DELTA_WPARAM(wParam);

	::GetCursorPos(&mousePosition);
	::ScreenToClient(m_hWnd, &mousePosition);

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






