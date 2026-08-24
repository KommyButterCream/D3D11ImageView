#include "pch.h"
#include "D3D11ImageView_Impl.h"
#include "../Render Layer/ImageRenderLayer.h"
#include "../Render Layer/UIRenderLayer.h"
#include "../Image Tile/TileManager.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"

using namespace Core::ShapeType;
using namespace Core::ImageType;

bool D3D11ImageView_Impl::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth)
{
	if (!m_imageLayer || !data || width == 0 || height == 0 || stride == 0)
		return false;

	// 포맷 유효성(채널/비트깊이/stride)은 ImageRenderLayer::UpdateImage 가
	// 판정한다. 여기서 중복 검사하면 두 곳이 갈라질 수 있다.
	return QueueImageUpdate(data, width, height, stride, channel, bitDepth);
}

bool D3D11ImageView_Impl::UpdateSharedTexture(HANDLE sharedHandle)
{
	if (!m_imageLayer)
		return false;

	return QueueSharedTextureUpdate(sharedHandle);
}

bool D3D11ImageView_Impl::UpdateTexture(ID3D11Texture2D* texture)
{
	if (!m_imageLayer || !texture)
		return false;

	return QueueTextureUpdate(texture);
}

bool D3D11ImageView_Impl::QueueImageUpdate(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth)
{
	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.texture)
	{
		m_pendingImageUpdate.texture->Release();
	}
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::RawImage;
	m_pendingImageUpdate.rawData = data;
	m_pendingImageUpdate.width = width;
	m_pendingImageUpdate.height = height;
	m_pendingImageUpdate.stride = stride;
	m_pendingImageUpdate.channel = channel;
	m_pendingImageUpdate.bitDepth = bitDepth;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	m_hasPendingImageUpdate = true;
	InvalidateFrame();

	return true;
}

bool D3D11ImageView_Impl::QueueTextureUpdate(ID3D11Texture2D* texture)
{
	if (!texture)
		return false;

	texture->AddRef();

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.texture)
	{
		m_pendingImageUpdate.texture->Release();
	}
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::Texture;
	m_pendingImageUpdate.texture = texture;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	m_hasPendingImageUpdate = true;
	InvalidateFrame();

	return true;
}

bool D3D11ImageView_Impl::QueueSharedTextureUpdate(HANDLE sharedHandle)
{
	if (!sharedHandle)
		return false;

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.texture)
	{
		m_pendingImageUpdate.texture->Release();
	}
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::SharedTexture;
	m_pendingImageUpdate.sharedHandle = sharedHandle;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	m_hasPendingImageUpdate = true;
	InvalidateFrame();

	return true;
}

void D3D11ImageView_Impl::DetachImage()
{
	if (!m_imageLayer)
		return;

	// 렌더 스레드가 Render() 안에 있으면 끝날 때까지 기다린다.
	// (Render 도 같은 락을 잡으므로, 여기를 통과하면 프레임 밖임이 보장된다)
	::AcquireSRWLockExclusive(&m_renderLock);

	// 아직 적용되지 않은 대기 업데이트도 버린다. 그 안의 rawData 도
	// 호출자 버퍼를 가리키고 있을 수 있다.
	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.texture)
	{
		m_pendingImageUpdate.texture->Release();
	}
	m_pendingImageUpdate.texture = nullptr;
	m_pendingImageUpdate.Reset();
	m_hasPendingImageUpdate = false;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	// 풀 해제 + ImageBase 참조 해제
	m_imageLayer->DetachImage();

	::ReleaseSRWLockExclusive(&m_renderLock);

	InvalidateFrame();
}

bool D3D11ImageView_Impl::ApplyPendingImageUpdate()
{
	if (!m_imageLayer || !m_hasPendingImageUpdate.exchange(false))
		return true;

	PendingImageUpdate pendingUpdate = {};

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	pendingUpdate = std::move(m_pendingImageUpdate);
	m_pendingImageUpdate.texture = nullptr;
	m_pendingImageUpdate.Reset();
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	bool result = false;

	switch (pendingUpdate.type)
	{
	case PendingImageUpdateType::RawImage:
		result = m_imageLayer->UpdateImage(
			pendingUpdate.rawData,
			pendingUpdate.width,
			pendingUpdate.height,
			pendingUpdate.stride,
			pendingUpdate.channel,
			pendingUpdate.bitDepth);

		if (m_uiLayer && result)
		{
			m_uiLayer->UpdateStatusbarImageSize(pendingUpdate.width, pendingUpdate.height);
		}
		break;

	case PendingImageUpdateType::Texture:
	{
		uint32_t width = 0;
		uint32_t height = 0;
		result = m_imageLayer->UpdateTexture(pendingUpdate.texture, width, height);

		if (m_uiLayer && result)
		{
			m_uiLayer->UpdateStatusbarImageSize(width, height);
		}
		break;
	}

	case PendingImageUpdateType::SharedTexture:
	{
		uint32_t width = 0;
		uint32_t height = 0;
		result = m_imageLayer->UpdateSharedTexture(pendingUpdate.sharedHandle, width, height);

		if (m_uiLayer && result)
		{
			m_uiLayer->UpdateStatusbarImageSize(width, height);
		}
		break;
	}

	case PendingImageUpdateType::None:
	default:
		result = true;
		break;
	}

	if (pendingUpdate.texture)
	{
		pendingUpdate.texture->Release();
		pendingUpdate.texture = nullptr;
	}

	return result;
}



bool D3D11ImageView_Impl::SimulateDeviceLost()
{
	if (!m_renderContext)
		return false;

	// 렌더 스레드가 프레임 밖에 있을 때 실행해야 한다.
	// (Render 도 같은 락을 잡는다)
	::AcquireSRWLockExclusive(&m_renderLock);
	const bool result = m_renderContext->SimulateDeviceLost();
	::ReleaseSRWLockExclusive(&m_renderLock);

	InvalidateFrame();

	return result;
}
