#include "pch.h"
#include "D3D11ImageView_Impl.h"

#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"
#include "../../../Module/D3D11Engine/Camera/Camera2D.h"

#include "../Render Layer/ImageRenderLayer.h"
#include "../Render Layer/ROIRenderLayer.h"
#include "../Render Layer/UIRenderLayer.h"

#include <algorithm>

using namespace Core::ShapeType;
using namespace Core::ImageType;

/*=========================================================
	ROI 조회

	전부 ROIRenderLayer 로 위임한다. 락은 그쪽이 잡는다.
=========================================================*/
uint32_t D3D11ImageView_Impl::ROIGetCount() const
{
	return m_roiLayer ? m_roiLayer->ROIGetCount() : 0u;
}

bool D3D11ImageView_Impl::ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const
{
	return m_roiLayer ? m_roiLayer->ROIGetShape(key, outShape) : false;
}

uint32_t D3D11ImageView_Impl::ROIGetVertices(const wchar_t* key, Point2f* buffer,
	uint32_t capacity, uint32_t segmentsPerCurve) const
{
	return m_roiLayer
		? m_roiLayer->ROIGetVertices(key, buffer, capacity, segmentsPerCurve)
		: 0u;
}

bool D3D11ImageView_Impl::ROIGetBounds(const wchar_t* key, Rect2f& outBounds) const
{
	return m_roiLayer ? m_roiLayer->ROIGetBounds(key, outBounds) : false;
}

bool D3D11ImageView_Impl::ROIGetInfo(const wchar_t* key, ROIRenderLayer::ROIInfoData& outInfo) const
{
	return m_roiLayer ? m_roiLayer->ROIGetInfo(key, outInfo) : false;
}

uint32_t D3D11ImageView_Impl::ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars) const
{
	return m_roiLayer ? m_roiLayer->ROIGetName(key, buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView_Impl::ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const
{
	return m_roiLayer ? m_roiLayer->ROIGetKeyAt(index, buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView_Impl::ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const
{
	return m_roiLayer ? m_roiLayer->ROIGetSelectedKey(buffer, bufferChars) : 0u;
}

uint32_t D3D11ImageView_Impl::ROIHitTestKey(float imageX, float imageY, float tolerance,
	wchar_t* buffer, uint32_t bufferChars) const
{
	return m_roiLayer
		? m_roiLayer->ROIHitTestKey(imageX, imageY, tolerance, buffer, bufferChars)
		: 0u;
}

bool D3D11ImageView_Impl::ROIRemove(const wchar_t* key)
{
	const bool removed = m_roiLayer ? m_roiLayer->ROIRemove(key) : false;

	if (removed)
	{
		InvalidateFrame();
	}

	return removed;
}

void D3D11ImageView_Impl::SetPixelScale(double xScale, double yScale, const wchar_t* unit)
{
	if (m_roiLayer)
	{
		m_roiLayer->SetPixelScale(xScale, yScale, unit);
		InvalidateFrame();
	}
}

void D3D11ImageView_Impl::ToggleMeasureDistance()
{
	if (!m_roiLayer)
	{
		return;
	}

	// 토글이다. 어느 쪽으로 가든 기존 측정선은 리셋된다.
	m_measureActive = !m_measureActive;

	if (m_measureActive)
	{
		m_roiLayer->BeginMeasure();

		// 각도 측정과 배타적이다. 둘 다 좌클릭을 가로채므로 같이 켜지면
		// 어느 쪽이 먹는지 알 수 없다.
		if (m_angleActive)
		{
			m_angleActive = false;
			m_roiLayer->CancelAngle();

			if (m_uiLayer)
			{
				m_uiLayer->SetAngleButtonActive(false);
			}
		}
	}
	else
	{
		m_roiLayer->CancelMeasure();
	}

	if (m_uiLayer)
	{
		m_uiLayer->SetMeasureButtonActive(m_measureActive);
	}

	InvalidateFrame();
}

bool D3D11ImageView_Impl::IsMeasureActive() const
{
	return m_measureActive;
}

void D3D11ImageView_Impl::ToggleMeasureAngle()
{
	if (!m_roiLayer)
	{
		return;
	}

	// 토글이다. 어느 쪽으로 가든 기존 측정 결과는 리셋된다.
	m_angleActive = !m_angleActive;

	if (m_angleActive)
	{
		m_roiLayer->BeginAngle();

		if (m_measureActive)
		{
			m_measureActive = false;
			m_roiLayer->CancelMeasure();

			if (m_uiLayer)
			{
				m_uiLayer->SetMeasureButtonActive(false);
			}
		}
	}
	else
	{
		m_roiLayer->CancelAngle();
	}

	if (m_uiLayer)
	{
		m_uiLayer->SetAngleButtonActive(m_angleActive);
	}

	InvalidateFrame();
}

bool D3D11ImageView_Impl::IsMeasureAngleActive() const
{
	return m_angleActive;
}

void D3D11ImageView_Impl::SetROIEventHandler(ROIRenderLayer::ROIEventHandler handler, void* userData)
{
	// Initialize 전에도 등록할 수 있어야 한다. C# 래퍼처럼 생성 직후
	// 콜백부터 붙이는 호스트가 흔하다. 보관해 두고 레이어가 생기면 옮긴다.
	m_roiEventHandler = handler;
	m_roiEventUserData = userData;

	if (m_roiLayer)
	{
		m_roiLayer->SetROIEventHandler(handler, userData);
	}
}

void D3D11ImageView_Impl::ApplyPendingROIEventHandler()
{
	if (m_roiLayer && m_roiEventHandler)
	{
		m_roiLayer->SetROIEventHandler(m_roiEventHandler, m_roiEventUserData);
	}
}

/*=========================================================
	마우스 콜백

	WndProc(UI 스레드)에서 호출된다. UI 처리 다음, ROI 처리 앞이므로
	m_roiLock 밖이고, 콜백에서 ROI API 를 불러도 데드락이 없다.
=========================================================*/
void D3D11ImageView_Impl::SetMouseHandler(MouseHandler handler, void* userData)
{
	m_mouseHandler = handler;
	m_mouseUserData = userData;
}

bool D3D11ImageView_Impl::DispatchMouseEvent(MouseEventType type,
	int32_t screenX, int32_t screenY, int32_t wheelDelta)
{
	if (!m_mouseHandler)
		return false;

	MouseEventData data = {};
	data.type = type;
	data.screenX = screenX;
	data.screenY = screenY;
	data.wheelDelta = wheelDelta;

	// 이미지 좌표. 이미지 밖이면 isInsideImage 가 false 로 남는다.
	if (m_camera && screenX >= 0 && screenY >= 0)
	{
		float imageX = 0.0f;
		float imageY = 0.0f;

		if (m_camera->ScreenToImage(static_cast<float>(screenX),
			static_cast<float>(screenY), imageX, imageY))
		{
			data.imageX = imageX;
			data.imageY = imageY;
			data.isInsideImage = true;
		}
	}

	// 수정자/버튼 상태는 메시지 인자가 아니라 현재 키 상태에서 읽는다.
	// WM_MOUSEWHEEL 은 스크린 좌표를 주는 등 메시지마다 규약이 달라
	// wParam 을 일관되게 해석하기 어렵다.
	if (::GetKeyState(VK_CONTROL) < 0) data.modifiers |= MouseModifier_Ctrl;
	if (::GetKeyState(VK_SHIFT) < 0)   data.modifiers |= MouseModifier_Shift;
	if (::GetKeyState(VK_MENU) < 0)    data.modifiers |= MouseModifier_Alt;

	if (::GetKeyState(VK_LBUTTON) < 0) data.buttons |= MouseButton_Left;
	if (::GetKeyState(VK_RBUTTON) < 0) data.buttons |= MouseButton_Right;
	if (::GetKeyState(VK_MBUTTON) < 0) data.buttons |= MouseButton_Middle;

	// true 를 반환하면 뷰어는 이 이벤트를 더 처리하지 않는다.
	return m_mouseHandler(data, m_mouseUserData);
}

/*=========================================================
	뷰 제어 / 좌표 변환
=========================================================*/
void D3D11ImageView_Impl::SetZoomLevel(float zoom, bool animate)
{
	if (!m_camera)
		return;

	m_camera->SetZoom(zoom, animate);
	InvalidateFrame();
}

float D3D11ImageView_Impl::GetZoomLevel() const
{
	return m_camera ? m_camera->GetZoom() : 0.0f;
}

void D3D11ImageView_Impl::ZoomFitProgrammatic(bool animate)
{
	if (!m_camera)
		return;

	if (animate)
		m_camera->Fit();
	else
		m_camera->FitInstant();

	InvalidateFrame();
}

void D3D11ImageView_Impl::Zoom1To1Programmatic(bool animate)
{
	if (!m_camera)
		return;

	if (animate)
		m_camera->Zoom1to1();
	else
		m_camera->SetZoom(1.0f, false);

	InvalidateFrame();
}

void D3D11ImageView_Impl::SetViewCenter(float imageX, float imageY, bool animate)
{
	if (!m_camera)
		return;

	m_camera->SetCenter(imageX, imageY, animate);
	InvalidateFrame();
}

void D3D11ImageView_Impl::GetViewCenter(float& outX, float& outY) const
{
	if (!m_camera)
	{
		outX = 0.0f;
		outY = 0.0f;
		return;
	}

	m_camera->GetCenter(outX, outY);
}

void D3D11ImageView_Impl::ZoomToRect(const Rect2f& imageRect, float marginRatio, bool animate)
{
	if (!m_camera)
		return;

	m_camera->ZoomToRect(imageRect, marginRatio, animate);
	InvalidateFrame();
}

bool D3D11ImageView_Impl::GetVisibleImageRect(Rect2f& outRect) const
{
	if (!m_camera)
		return false;

	// GetViewImageRect 는 이미지 경계로 clamp 된 정수 사각형이다.
	// "화면에 보이는 이미지 영역" 이라는 의미에 그게 맞다.
	const Rect2i rect = m_camera->GetViewImageRect();

	outRect.left = static_cast<float>(rect.left);
	outRect.top = static_cast<float>(rect.top);
	outRect.right = static_cast<float>(rect.right);
	outRect.bottom = static_cast<float>(rect.bottom);

	return true;
}

bool D3D11ImageView_Impl::ScreenToImage(int32_t screenX, int32_t screenY, float& outImageX, float& outImageY) const
{
	if (!m_camera)
		return false;

	return m_camera->ScreenToImage(static_cast<float>(screenX), static_cast<float>(screenY), outImageX, outImageY);
}

bool D3D11ImageView_Impl::ImageToScreen(float imageX, float imageY, int32_t& outScreenX, int32_t& outScreenY) const
{
	if (!m_camera)
		return false;

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_camera->ImageToScreen(imageX, imageY, screenX, screenY);

	outScreenX = static_cast<int32_t>(screenX);
	outScreenY = static_cast<int32_t>(screenY);

	return true;
}

/*=========================================================
	이미지 정보
=========================================================*/
bool D3D11ImageView_Impl::GetImageSize(uint32_t& outWidth, uint32_t& outHeight) const
{
	if (!m_imageLayer)
		return false;

	const ImageBase* image = m_imageLayer->GetImage();
	if (!image || image->IsEmpty())
		return false;

	outWidth = static_cast<uint32_t>(image->Width());
	outHeight = static_cast<uint32_t>(image->Height());

	return true;
}

bool D3D11ImageView_Impl::GetImageChannelInfo(uint32_t& outChannel, uint32_t& outBitDepth) const
{
	if (!m_imageLayer)
		return false;

	const ImageBase* image = m_imageLayer->GetImage();
	if (!image || image->IsEmpty())
		return false;

	outChannel = static_cast<uint32_t>(image->Channel());

	switch (image->GetPixelType())
	{
	case PixelType::U16: outBitDepth = 16; break;
	case PixelType::F32: outBitDepth = 32; break;
	case PixelType::U8:
	default:             outBitDepth = 8;  break;
	}

	return true;
}

bool D3D11ImageView_Impl::GetPixelValueAt(int32_t imageX, int32_t imageY,
	double* outValues, uint32_t valueCapacity, uint32_t& outChannelCount) const
{
	if (!m_imageLayer || !outValues || valueCapacity == 0)
		return false;

	// 렌더 스레드가 Attach 로 버퍼를 갈아끼우는 것과 경합하지 않도록
	// 프레임 밖에서 읽는다(Render 도 같은 락을 잡는다).
	::AcquireSRWLockExclusive(&m_renderLock);

	bool result = false;

	const ImageBase* image = m_imageLayer->GetImage();
	if (image && !image->IsEmpty() && image->IsInside(imageX, imageY))
	{
		const int32_t channel = image->Channel();

		PixelValue values[4] = {};
		if (GetPixelValueForStatusbar(image, imageX, imageY, channel, values))
		{
			const uint32_t count =
				(std::min)(static_cast<uint32_t>(channel), valueCapacity);

			for (uint32_t i = 0; i < count; ++i)
			{
				outValues[i] = (values[i].format == PixelValueFormat::Float)
					? values[i].f
					: static_cast<double>(values[i].i);
			}

			outChannelCount = count;
			result = true;
		}
	}

	::ReleaseSRWLockExclusive(&m_renderLock);

	return result;
}

/*=========================================================
	표시 옵션
=========================================================*/
void D3D11ImageView_Impl::SetToolbarVisible(bool visible)
{
	if (m_uiLayer)
	{
		m_uiLayer->SetToolbarVisible(visible);
		InvalidateFrame();
	}
}

void D3D11ImageView_Impl::SetStatusBarVisible(bool visible)
{
	if (m_uiLayer)
	{
		m_uiLayer->SetStatusBarVisible(visible);
		InvalidateFrame();
	}
}

void D3D11ImageView_Impl::SetBackgroundColor(uint32_t colorRGB)
{
	if (!m_renderContext)
		return;

	// COLORREF 배치: 0x00BBGGRR
	const float r = static_cast<float>(colorRGB & 0xFFu) / 255.0f;
	const float g = static_cast<float>((colorRGB >> 8) & 0xFFu) / 255.0f;
	const float b = static_cast<float>((colorRGB >> 16) & 0xFFu) / 255.0f;

	m_renderContext->SetBackgroundColor(r, g, b, 1.0f);
	InvalidateFrame();
}

void D3D11ImageView_Impl::SetVSyncEnabled(bool enable)
{
	if (m_renderContext)
	{
		m_renderContext->SetVSyncEnabled(enable);
	}
}

void D3D11ImageView_Impl::SetMipMapGenerationEnabled(bool enable)
{
	::AcquireSRWLockExclusive(&m_renderLock);

	bool applied = true;
	if (m_imageLayer)
	{
		applied = m_imageLayer->SetMipMapGenerationEnabled(enable);
	}

	if (applied)
	{
		m_mipMapGenerationEnabled = enable;
	}

	::ReleaseSRWLockExclusive(&m_renderLock);

	if (applied)
	{
		InvalidateFrame();
	}
}

bool D3D11ImageView_Impl::IsMipMapGenerationEnabled() const
{
	::AcquireSRWLockShared(&m_renderLock);
	const bool enabled = m_mipMapGenerationEnabled;
	::ReleaseSRWLockShared(&m_renderLock);
	return enabled;
}
