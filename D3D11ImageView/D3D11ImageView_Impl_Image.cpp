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

// 풀 등록은 렌더 스레드와 배타적으로 한다. 렌더 스레드가 슬롯 배열을 읽는
// 도중에 그것을 갈아치우면 안 되기 때문이다.
bool D3D11ImageView_Impl::RegisterSharedTexturePool(const HANDLE* sharedHandles, uint32_t count)
{
	if (!m_imageLayer || !sharedHandles || count == 0)
		return false;

	::AcquireSRWLockExclusive(&m_renderLock);
	const bool result = m_imageLayer->RegisterSharedTexturePool(sharedHandles, count);
	::ReleaseSRWLockExclusive(&m_renderLock);

	return result;
}

void D3D11ImageView_Impl::UnregisterSharedTexturePool()
{
	if (!m_imageLayer)
		return;

	::AcquireSRWLockExclusive(&m_renderLock);

	// 아직 적용되지 않은 슬롯 업데이트가 남아 있으면 곧 닫힐 슬롯을
	// 가리키게 된다. 같이 버린다.
	::AcquireSRWLockExclusive(&m_pendingImageLock);
	if (m_pendingImageUpdate.type == PendingImageUpdateType::SharedTexturePoolSlot)
	{
		m_pendingImageUpdate.Reset();
		::InterlockedExchange(&m_hasPendingImageUpdate, FALSE);
	}
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	m_imageLayer->UnregisterSharedTexturePool();

	::ReleaseSRWLockExclusive(&m_renderLock);
}

bool D3D11ImageView_Impl::UpdateSharedTexturePoolSlot(uint32_t slot)
{
	if (!m_imageLayer)
		return false;

	return QueueSharedTexturePoolSlotUpdate(slot);
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

	SafeRelease(m_pendingImageUpdate.texture);
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::RawImage;
	m_pendingImageUpdate.rawData = data;
	m_pendingImageUpdate.width = width;
	m_pendingImageUpdate.height = height;
	m_pendingImageUpdate.stride = stride;
	m_pendingImageUpdate.channel = channel;
	m_pendingImageUpdate.bitDepth = bitDepth;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	::InterlockedExchange(&m_hasPendingImageUpdate, TRUE);
	InvalidateFrame();

	return true;
}

bool D3D11ImageView_Impl::QueueTextureUpdate(ID3D11Texture2D* texture)
{
	if (!texture)
		return false;

	texture->AddRef();

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	SafeRelease(m_pendingImageUpdate.texture);
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::Texture;
	m_pendingImageUpdate.texture = texture;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	::InterlockedExchange(&m_hasPendingImageUpdate, TRUE);
	InvalidateFrame();

	return true;
}

bool D3D11ImageView_Impl::QueueSharedTextureUpdate(HANDLE sharedHandle)
{
	if (!sharedHandle)
		return false;

	::AcquireSRWLockExclusive(&m_pendingImageLock);
	SafeRelease(m_pendingImageUpdate.texture);
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::SharedTexture;
	m_pendingImageUpdate.sharedHandle = sharedHandle;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	::InterlockedExchange(&m_hasPendingImageUpdate, TRUE);
	InvalidateFrame();

	return true;
}

// 대기 슬롯은 하나뿐이라, 렌더 스레드가 따라오지 못하면 앞의 것이 덮인다.
// 최신 프레임만 보여주면 되는 입력이라 그게 맞는 동작이다.
//
// 생산자는 이 호출이 끝나면 슬롯 핸들을 반납해도 된다. 실제 복사는 렌더
// 스레드에서 일어나지만, 그 사이 생산자가 같은 슬롯을 덮어쓰더라도 키드
// 뮤텍스가 둘을 갈라 놓는다. 최악의 경우 한 프레임 더 새로운 화면이 나온다.
bool D3D11ImageView_Impl::QueueSharedTexturePoolSlotUpdate(uint32_t slot)
{
	::AcquireSRWLockExclusive(&m_pendingImageLock);
	SafeRelease(m_pendingImageUpdate.texture);
	m_pendingImageUpdate.Reset();
	m_pendingImageUpdate.type = PendingImageUpdateType::SharedTexturePoolSlot;
	m_pendingImageUpdate.poolSlot = slot;
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	::InterlockedExchange(&m_hasPendingImageUpdate, TRUE);
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
	SafeRelease(m_pendingImageUpdate.texture);
	m_pendingImageUpdate.Reset();
	::InterlockedExchange(&m_hasPendingImageUpdate, FALSE);
	::ReleaseSRWLockExclusive(&m_pendingImageLock);

	// 풀 해제 + ImageBase 참조 해제
	m_imageLayer->DetachImage();

	::ReleaseSRWLockExclusive(&m_renderLock);

	InvalidateFrame();
}

bool D3D11ImageView_Impl::ApplyPendingImageUpdate()
{
	// 단축 평가 순서를 유지한다. m_imageLayer 가 없으면 플래그를 건드리지 않는다.
	if (!m_imageLayer || ::InterlockedExchange(&m_hasPendingImageUpdate, FALSE) == FALSE)
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

	case PendingImageUpdateType::SharedTexturePoolSlot:
	{
		uint32_t width = 0;
		uint32_t height = 0;
		result = m_imageLayer->UpdateSharedTexturePoolSlot(pendingUpdate.poolSlot, width, height);

		if (m_uiLayer && result)
		{
			m_uiLayer->UpdateStatusbarImageSize(width, height);
		}

		// 뮤텍스를 못 잡아 이번 프레임을 건너뛴 것은 실패가 아니다.
		// false 를 그대로 올리면 상위가 LUT 재평가까지 건너뛴다.
		result = true;
		break;
	}

	case PendingImageUpdateType::None:
	default:
		result = true;
		break;
	}

	SafeRelease(pendingUpdate.texture);

	// 이미지가 바뀌면 LUT 를 쓸 수 있는지도 바뀐다.
	// 컬러로 넘어갔으면 켜져 있던 LUT 를 내리고 버튼을 비활성으로 만든다.
	if (result)
	{
		RefreshLutAvailability();
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
