#include "pch.h"
#include "D3D11ImageView_Impl.h"
#include "../Render Layer/ImageRenderLayer.h"
#include "../Render Layer/UIRenderLayer.h"

using namespace Core::ShapeType;
using namespace Core::ImageType;

bool D3D11ImageView_Impl::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	if (!m_imageLayer || !data || width == 0 || height == 0 || stride == 0)
		return false;

	return QueueImageUpdate(data, width, height, stride, channel);
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

bool D3D11ImageView_Impl::QueueImageUpdate(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
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
			pendingUpdate.channel);

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


