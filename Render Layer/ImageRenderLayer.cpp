#include "pch.h"
#include "ImageRenderLayer.h"

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11EngineInterface/IRenderStructures.h"

#include "../../../Module/Core/ImageType/ImageBase.h"

#include "../Image Tile/Tile.h"
#include "../Image Tile/TileManager.h"

#include <algorithm>
#include <string>

// OpenSharedResource1 (NT 공유 핸들) 용
#include <d3d11_1.h>

static const uint16_t kQuadIndices[] = { 0,1,2, 0,2,3 };

// 타일 1장 = 삼각형 2개 = 정점 6개.
static constexpr uint32_t kVerticesPerTile = 6;

// 정점 버퍼 상한(= 약 10,900 타일). 정상 경로에서는 닿지 않는다.
// 뷰포트/프리페치 계산이 깨졌을 때 버퍼가 무한히 커지는 것을 막는 안전장치다.
static constexpr uint32_t kMaxTileVertexLimit = 65536;

// 공유 프레임 풀의 소유권 프로토콜. 키 0 하나만 쓴다.
// 생산자와 규약이 같아야 하므로 D3D11DuplicateEngine 의 FRAME_POOL_MUTEX_KEY
// 와 값이 맞아야 한다.
static constexpr UINT64 kFramePoolAcquireKey = 0;

// 슬롯 하나를 기다리는 상한. 생산자가 뮤텍스를 쥐는 구간은 CopyResource
// 제출 한 번뿐이라 실제로는 거의 즉시 잡힌다. 이 값은 렌더 스레드가
// 붙잡히지 않게 하는 안전망이다.
static constexpr DWORD kFramePoolAcquireTimeout_ms = 8;

static bool CanCopyWholeTexture(const D3D11_TEXTURE2D_DESC& destination,
	const D3D11_TEXTURE2D_DESC& source)
{
	return destination.Width == source.Width &&
		destination.Height == source.Height &&
		destination.MipLevels == source.MipLevels &&
		destination.ArraySize == source.ArraySize &&
		destination.Format == source.Format &&
		destination.SampleDesc.Count == source.SampleDesc.Count &&
		destination.SampleDesc.Quality == source.SampleDesc.Quality;
}

// 확대 필터 전환 임계(Single 모드).
//
// 이 아래는 LINEAR(부드러움), 이 위는 POINT(픽셀 경계 보존).
// 1.0 이 아니라 2.0 인 이유: fit ~ 1:1 구간은 휠 조작에서 가장 자주
// 오가는 곳이라 여기서 필터가 바뀌면 애니메이션 도중에 튄다.
// 픽셀 단위 판독이 필요한 배율은 실질적으로 2x 이상이다.
//
// Enter/Exit 를 벌려 히스테리시스를 만든다. 줌 애니메이션이 임계를
// 살짝 넘나들 때 프레임마다 필터가 바뀌는 것을 막는다.
static constexpr float kMagPointEnterZoom = 2.0f;
static constexpr float kMagPointExitZoom  = 1.7f;

ImageRenderLayer::ImageRenderLayer()
{

}

ImageRenderLayer::~ImageRenderLayer()
{
	Shutdown();
}

bool ImageRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
		return false;

	m_context = dynamic_cast<D3D11RenderContext*>(context);
	if (!m_context)
		return false;

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return false;

	m_device = engine->GetD3DDevice();
	m_contextD3D = engine->GetD3DDeviceContext();
	ID3D11RenderTargetView* rtv = m_context->GetD3DRenderTargetView();

	if (!m_device || !m_contextD3D || !rtv)
		return false;

	m_image = new Core::ImageType::ImageBase;
	if (!m_image)
		return false;

	if (!CreateDeviceResources())
		return false;

	m_context->AddResizeListener(this);
	m_context->AddDeviceListener(this);

	m_initialized = true;

	return true;
}

void ImageRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveResizeListener(this);
		m_context->RemoveDeviceListener(this);
	}

	ReleaseDeviceResources();

	m_sharedHandle = nullptr;
	m_tileManager = nullptr;
	m_initialized = false;
}

bool ImageRenderLayer::Prepare()
{
	return true;
}

bool ImageRenderLayer::Render()
{
	if (!m_initialized)
		return true;

	// 디바이스 로스트 ~ 복구 사이에는 아무것도 그리지 않는다.
	if (!m_deviceResourcesReady)
		return true;

	if (!m_context)
		return false;

	if (GetRenderMode() == RenderMode::Tiled)
	{
		if (!m_image->IsEmpty() && m_tileManager)
		{
			// 뷰포트가 크게 바뀌면 작업세트와 maxLOD 가 달라지므로 재구성한다.
			const uint32_t viewWidth = m_context->GetWidth();
			const uint32_t viewHeight = m_context->GetHeight();

			if (viewWidth > 0 && viewHeight > 0 &&
				m_tileManager->NeedsReconfigure(viewWidth, viewHeight))
			{
				if (m_tileManager->Configure(m_image->Width(), m_image->Height(),
					m_image->Channel(), GetAttachedBitDepth(), viewWidth, viewHeight))
				{
					m_tileManager->PrimeCoarsestLevel(m_image->ImageBuffer(),
						m_image->Width(), m_image->Stride(), m_image->Height(), m_image->Channel());
				}
			}

			const Core::ShapeType::Rect2i rect = m_camera->GetViewImageRect();

			// 모션 게이팅: 카메라가 움직이는 중이면 새 타일을 만들지 않고
			// 상주분(부모 fallback 포함)으로만 그린다. 드래그 중 스치는 타일은
			// 업로드가 끝나기도 전에 화면을 벗어나므로 그 비용이 낭비다.
			const bool cameraSettled = m_camera->IsSettled();

			m_tileManager->UpdateVisibleTiles(rect, m_camera->GetZoom(), m_frameID,
				m_image->ImageBuffer(), m_image->Width(), m_image->Stride(), m_image->Height(), m_image->Channel(),
				cameraSettled);
		}
	}

	m_context->GetEngine();

	ID3D11RenderTargetView* rtv = m_context->GetD3DRenderTargetView();

	if (!rtv)
		return false;

	// OM
	m_contextD3D->OMSetRenderTargets(1, &rtv, nullptr);

	// Viewport
	uint32_t viewWidth = m_context->GetWidth();
	uint32_t viewHeight = m_context->GetHeight();

	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<FLOAT>(viewWidth);
	vp.Height = static_cast<FLOAT>(viewHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	m_contextD3D->RSSetViewports(1, &vp);

	ViewParams viewParams;
	m_camera->GetViewParams(viewParams);
	D3D11_MAPPED_SUBRESOURCE mappedCB{};
	if (SUCCEEDED(m_contextD3D->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
	{
		memcpy(mappedCB.pData, &viewParams, sizeof(ViewParams));
		m_contextD3D->Unmap(m_constantBuffer, 0);
	}
	m_contextD3D->VSSetConstantBuffers(0, 1, &m_constantBuffer);

	bool result = false;
	if (m_currentMode == RenderMode::Tiled)
	{
		result = RenderTiled();
	}
	else
	{
		result = RenderSingle();
	}

	return true;
}

void ImageRenderLayer::OnResize(uint32_t width, uint32_t height)
{
	if (!m_camera)
		return;

	m_camera->SetViewSize(width, height);
	m_camera->Fit();
}

void ImageRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();

	// 타일 풀의 ID3D11Texture2D 도 Lost된 디바이스 소속이다.
	// TileManager 는 리스너로 등록되어 있지 않으므로 여기서 위임한다.
	if (m_tileManager)
	{
		m_tileManager->OnDeviceLost();
	}

	// 디바이스가 사라졌으므로 어떤 모드도 그릴 수 없는 상태다.
	m_deviceResourcesReady = false;
}

void ImageRenderLayer::OnDeviceRestored()
{
	if (!m_context)
		return;

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return;

	m_device = engine->GetD3DDevice();
	m_contextD3D = engine->GetD3DDeviceContext();

	if (!m_device || !m_contextD3D)
		return;

	if (!CreateDeviceResources())
	{
		ReleaseDeviceResources();
		return;
	}

	if (m_tileManager)
	{
		m_tileManager->OnDeviceRestored(m_device, m_contextD3D);
	}

	m_deviceResourcesReady = true;

	// 셰이더/샘플러만 되살려서는 화면이 비어 있다.
	// 원본 CPU 버퍼가 살아 있으면 이미지까지 다시 올린다.
	RestoreImageAfterDeviceLoss();
}

// 디바이스 복구 후 화면을 되돌린다.
//
// m_image 는 호출자 버퍼를 Attach 한 것이라 디바이스와 무관하게 살아 있다.
// 그걸 그대로 다시 업로드하면 사용자는 로스트를 눈치채지 못한다.
// (텍스처/공유텍스처 입력은 원본이 GPU 쪽이라 되살릴 수 없다 -> 호출자 재공급 필요)
void ImageRenderLayer::RestoreImageAfterDeviceLoss()
{
	if (m_inputSource != ImageInputSource::RawImage)
		return;

	if (!m_image || m_image->IsEmpty())
		return;

	const uint8_t* data = m_image->ImageBuffer();
	const uint32_t width = m_image->Width();
	const uint32_t height = m_image->Height();
	const uint32_t stride = m_image->Stride();
	const uint32_t channel = m_image->Channel();

	if (!data || width == 0 || height == 0)
		return;

	// UpdateImage 는 판정부터 다시 하므로 모드/포맷/풀이 일관되게 재구성된다.
	// (카메라는 UpdateImageState 가 크기 변화 없음을 보고 Fit 을 건너뛴다)
	UpdateImage(data, width, height, stride, channel, GetAttachedBitDepth());
}

uint32_t ImageRenderLayer::GetAttachedBitDepth() const
{
	if (!m_image)
		return 8;

	switch (m_image->GetPixelType())
	{
	case PixelType::U16: return 16;
	case PixelType::F32: return 32;
	case PixelType::U8:
	default:             return 8;
	}
}

void ImageRenderLayer::SetCamera2D(Camera2D* camera)
{
	m_camera = camera;
}

void ImageRenderLayer::SetTileManager(TileManager* tileManager)
{
	m_tileManager = tileManager;
}

void ImageRenderLayer::SetFrameID(uint64_t frameID)
{
	m_frameID = frameID;
}

bool ImageRenderLayer::IsImageRenderDirty() const
{
	if (m_currentMode != RenderMode::Tiled || !m_tileManager)
		return false;

	return m_tileManager->HasPendingUploads();
}

bool ImageRenderLayer::SetMipMapGenerationEnabled(bool enable)
{
	if (m_mipMapGenerationEnabled == enable)
		return true;

	// Device lost 중에는 재생성할 GPU 리소스가 없다. 정책만 저장하면
	// RestoreImageAfterDeviceLoss 또는 다음 텍스처 입력이 새 설정을 적용한다.
	if (!m_deviceResourcesReady)
	{
		m_mipMapGenerationEnabled = enable;
		return true;
	}

	// RawImage는 정책 변경으로 VRAM 예산 판정까지 달라질 수 있다. 같은 원본을
	// UpdateImage에 다시 통과시켜 Single <-> Tiled 전환도 함께 처리한다.
	if (m_inputSource == ImageInputSource::RawImage && m_image && !m_image->IsEmpty())
	{
		const uint8_t* data = m_image->ImageBuffer();
		const uint32_t width = m_image->Width();
		const uint32_t height = m_image->Height();
		const uint32_t stride = m_image->Stride();
		const uint32_t channel = m_image->Channel();
		const uint32_t bitDepth = GetAttachedBitDepth();
		const bool previousSetting = m_mipMapGenerationEnabled;

		m_mipMapGenerationEnabled = enable;
		if (UpdateImage(data, width, height, stride, channel, bitDepth))
			return true;

		// 새 정책 적용이 실패하면 이전 정책으로 화면 구성을 복구한다.
		m_mipMapGenerationEnabled = previousSetting;
		UpdateImage(data, width, height, stride, channel, bitDepth);
		return false;
	}

	// Tiled 모드는 TileManager가 자체 LOD를 관리한다. GPU texture 입력은 현재
	// Tiled를 사용하지 않으므로, 이 경우에는 다음 Single 입력용 정책만 저장한다.
	if (m_currentMode != RenderMode::Single || !m_singleTexture)
	{
		m_mipMapGenerationEnabled = enable;
		return true;
	}

	// 이미 표시 중인 Single 이미지는 mip 0을 보존한 채 새 구성으로 교체한다.
	// 실패하면 CreateSingleBuffer가 기존 리소스를 유지하므로 설정도 바꾸지 않는다.
	if (!RecreateSingleBufferForMipSetting(enable))
		return false;

	m_mipMapGenerationEnabled = enable;
	return true;
}

bool ImageRenderLayer::IsMipMapGenerationEnabled() const
{
	return m_mipMapGenerationEnabled;
}

bool ImageRenderLayer::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth)
{
	if (!data || width == 0 || height == 0 || stride == 0)
		return false;

	if (channel != 1 && channel != 3 && channel != 4)
		return false;

	if (bitDepth != 8 && bitDepth != 16)
		return false;

	// 16bit 은 Gray 만 받는다. 3채널은 D3D11 에 48bit 포맷이 아예 없고,
	// 4채널은 R16G16B16A16_UNORM 이 있긴 하지만 TileSampler 와 픽셀 셰이더가
	// 아직 8bit 만 다룬다. 조용히 깨진 화면을 내놓는 대신 거절한다.
	if (bitDepth == 16 && channel != 1)
		return false;

	// stride 가 최소 한 줄을 담을 수 있어야 한다. 16bit 소스에 8bit stride 가
	// 들어오는 실수를 여기서 걸러야 이후 샘플링이 버퍼를 넘지 않는다.
	const uint32_t minimumStride = width * channel * (bitDepth / 8);
	if (stride < minimumStride)
		return false;

	const DXGI_FORMAT format = TileFormat::ResolveTextureFormat(channel, bitDepth);

	// Single(전량 상주) 가능 여부.
	//   1) 변 단위 하드 한계(16384)
	//   2) 현재 mip 생성 정책을 반영한 상주 예산
	// D3D11 에는 부분 상주 텍스처가 없으므로, 예산을 넘으면 타일링으로 간다.
	const bool preferSingle = TileFormat::CanUseSingleTexture(
		width, height, format, m_concurrentViewCount, m_mipMapGenerationEnabled);

	// 3채널만 컴퓨트 셰이더 확장이 필요하다.
	// (D3D11 에 24bit 텍스처 포맷이 아예 없어 저장 자체가 불가능하므로,
	//  텍스처가 되기 전에 4바이트로 펴야 한다)
	const bool needsComputeUpload = TileFormat::NeedsChannelExpansion(channel);

	RenderMode newMode = RenderMode::Tiled;

	if (preferSingle && CreateSingleBuffer(
		width, height, format, needsComputeUpload, m_mipMapGenerationEnabled))
	{
		newMode = RenderMode::Single;

		if (channel == 1 || channel == 4)
		{
			// 포맷이 소스와 일치하므로 변환 없이 그대로 올린다.
			// (Gray 는 R8_UNORM 이라 예전의 4바이트 확장이 불필요해졌다.)
			m_contextD3D->UpdateSubresource(m_singleTexture, 0, nullptr, data, stride, 0);
		}
		else
		{
			// BGR 24bit 만 확장이 필요하다. 이미지 전체를 한 번에 변환하는
			// 큰 병렬 작업이라 컴퓨트 셰이더가 맞는 도구다.
			if (!CreateRawUploadBuffer(width * height * channel))
				return false;

			UploadSingleImage_GPU(data, width, height, stride, channel);
		}

		if (m_mipMapGenerationEnabled)
		{
			GenerateSingleMips();
		}
	}

	if (newMode == RenderMode::Tiled)
	{
		if (!m_tileManager)
			return false;

		uint32_t viewWidth = m_context ? m_context->GetWidth() : 0;
		uint32_t viewHeight = m_context ? m_context->GetHeight() : 0;
		if (viewWidth == 0 || viewHeight == 0)
		{
			viewWidth = 1920;
			viewHeight = 1080;
		}

		// maxLOD / 용량 / 포맷이 모두 이미지 의존이므로 여기서 풀을 재구성한다.
		if (!m_tileManager->Configure(width, height, channel, bitDepth, viewWidth, viewHeight))
			return false;

		// 가장 거친 LOD 전체를 먼저 채운다. 이게 끝나기 전에는 이미지를 그리지
		// 않으므로 타일이 하나씩 나타나는 과정이 보이지 않고, 끝난 뒤에는
		// 부모 fallback 이 항상 성공해 화면에 구멍이 생기지 않는다.
		m_tileManager->PrimeCoarsestLevel(data, width, stride, height, channel);
	}

	// 모드가 바뀌면 반대쪽 리소스를 놓아준다.
	// 그러지 않으면 두 경로가 동시에 상주해 피크 VRAM 이 두 배가 된다.
	ReleaseUnusedModeResources(newMode);

	// Update image metadata and buffer ownership.
	if (m_image)
	{
		m_image->Attach(const_cast<uint8_t*>(data), width, height, stride, channel, bitDepth);
	}

	// Update camera state only when the image source or size changes.
	UpdateImageState(ImageInputSource::RawImage, width, height, newMode, channel);

	return true;
}

void ImageRenderLayer::DetachImage()
{
	// 풀을 먼저 해제해 이후 프레임이 원본을 다시 읽지 않게 한 뒤,
	// ImageBase 의 참조를 끊는다.
	if (m_tileManager)
	{
		m_tileManager->ReleasePools();
	}

	if (m_image)
	{
		m_image->ReleaseBuffer();
	}

	// Single 텍스처는 이미 GPU 사본이라 원본과 무관하므로 유지해도 되지만,
	// Detach 의 의미상 화면을 비우는 쪽이 예측 가능하다.
	SafeRelease(m_singleSRV);
	SafeRelease(m_singleUAV);
	SafeRelease(m_singleTexture);
	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_rawUploadBuffer);

	m_singleTextureWidth = 0;
	m_singleTextureHeight = 0;
	m_singleTextureFormat = DXGI_FORMAT_UNKNOWN;
	m_singleTextureHasMipMaps = false;
	m_maxByteSize = 0;

	m_inputSource = ImageInputSource::None;
	m_inputChannel = 0;
	m_texWidth = 0;
	m_texHeight = 0;
	m_renderVertices.clear();
}

// 사용하지 않는 렌더 경로의 GPU 리소스를 해제한다.
void ImageRenderLayer::ReleaseUnusedModeResources(RenderMode activeMode)
{
	if (activeMode == RenderMode::Single)
	{
		if (m_tileManager)
			m_tileManager->ReleasePools();
	}
	else
	{
		SafeRelease(m_singleSRV);
		SafeRelease(m_singleUAV);
		SafeRelease(m_singleTexture);
		SafeRelease(m_rawUploadSRV);
		SafeRelease(m_rawUploadBuffer);

		m_singleTextureWidth = 0;
		m_singleTextureHeight = 0;
		m_singleTextureFormat = DXGI_FORMAT_UNKNOWN;
		m_singleTextureHasMipMaps = false;
		m_maxByteSize = 0;
	}
}

bool ImageRenderLayer::UpdateTexture(ID3D11Texture2D* texture, uint32_t& width, uint32_t& height)
{
	if (!texture)
		return false;

	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);

	if (desc.Width == 0 || desc.Height == 0 || desc.Width > TileFormat::kMaxTextureDim || desc.Height > TileFormat::kMaxTextureDim)
		return false;

	ID3D11Device* sourceDevice = nullptr;
	texture->GetDevice(&sourceDevice);
	const bool sameDevice = (sourceDevice == m_device);
	SafeRelease(sourceDevice);
	if (!sameDevice)
		return false;

	width = desc.Width;
	height = desc.Height;

	// 텍스처/공유텍스처 입력은 CopyResource 로 받으므로 CS 가 필요 없다.
	if (!CreateSingleBuffer(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
		false, m_mipMapGenerationEnabled))
		return false;

	D3D11_TEXTURE2D_DESC destinationDesc = {};
	m_singleTexture->GetDesc(&destinationDesc);

	if (CanCopyWholeTexture(destinationDesc, desc))
	{
		m_contextD3D->CopyResource(m_singleTexture, texture);
	}
	else
	{
		m_contextD3D->CopySubresourceRegion(
			m_singleTexture, 0, 0, 0, 0, texture, 0, nullptr);
		if (m_mipMapGenerationEnabled)
		{
			GenerateSingleMips();
		}
	}

	if (m_image)
	{
		m_image->ReleaseBuffer();
	}

	// 직전 입력이 Tiled RawImage였다면 타일 풀을 놓는다. 텍스처 입력은
	// 항상 Single로 렌더링하므로 두 경로의 VRAM을 동시에 유지할 이유가 없다.
	ReleaseUnusedModeResources(RenderMode::Single);

	UpdateImageState(ImageInputSource::Texture, width, height, RenderMode::Single, 0);

	return true;
}

bool ImageRenderLayer::UpdateSharedTexture(HANDLE sharedHandle, uint32_t& width, uint32_t& height)
{
	if (!sharedHandle)
		return false;

	if (!OpenSharedResource(sharedHandle))
		return false;

	// 이 경로의 입력(NVDEC 등)은 MISC_SHARED 로 만들어져 키드 뮤텍스가 없다.
	// 동기화를 하는 입력은 RegisterSharedTexturePool 쪽을 쓴다.
	return CopySharedSourceToSingle(m_sharedTexture, width, height);
}

// 생산자가 준 슬롯 핸들을 전부 열어 둔다. 등록 후에는 슬롯 번호만 오간다.
//
// 풀 핸들은 CreateSharedHandle 로 만든 NT 핸글이라 OpenSharedResource1 이
// 필요하다. 구식 OpenSharedResource 는 NT 핸들에 실패한다 — 단일 공유
// 텍스처 경로가 그 구식 API 를 계속 쓰는 이유이기도 하다.
//
// 여기서 연 참조는 우리 것이다. 생산자가 자기 핸들을 닫아도 우리 텍스처는
// 살아 있다.
bool ImageRenderLayer::RegisterSharedTexturePool(const HANDLE* sharedHandles, uint32_t count)
{
	UnregisterSharedTexturePool();

	if (!sharedHandles || count == 0 || !m_device)
		return false;

	ID3D11Device1* device1 = nullptr;
	if (FAILED(m_device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device1))) || !device1)
		return false;

	bool succeeded = true;
	m_sharedPool.resize(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (!sharedHandles[i])
		{
			succeeded = false;
			break;
		}

		HRESULT hr = device1->OpenSharedResource1(
			sharedHandles[i], __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&m_sharedPool[i].texture));

		if (FAILED(hr) || !m_sharedPool[i].texture)
		{
			succeeded = false;
			break;
		}

		// 뮤텍스가 없는 슬롯은 받지 않는다. 생산자가 공유용으로 만들지 않은
		// 텍스처라는 뜻이고, 동기화 없이 읽으면 찢어진 화면이 나온다.
		hr = m_sharedPool[i].texture->QueryInterface(
			__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&m_sharedPool[i].keyedMutex));

		if (FAILED(hr) || !m_sharedPool[i].keyedMutex)
		{
			succeeded = false;
			break;
		}
	}

	SafeRelease(device1);

	if (!succeeded)
	{
		// 일부만 열린 풀은 쓸 수 없다. 슬롯 번호로 찾는 구조라 구멍이
		// 있으면 그 번호가 오는 순간 조용히 화면이 멈춘다.
		UnregisterSharedTexturePool();
		return false;
	}

	return true;
}

void ImageRenderLayer::UnregisterSharedTexturePool()
{
	for (SharedPoolSlot& slot : m_sharedPool)
	{
		SafeRelease(slot.keyedMutex);
		SafeRelease(slot.texture);
	}

	m_sharedPool.clear();
}

uint32_t ImageRenderLayer::GetSharedTexturePoolCount() const
{
	return static_cast<uint32_t>(m_sharedPool.size());
}

// 슬롯 하나를 Single 버퍼로 가져온다.
//
// 풀은 키 0 하나만 쓴다. 단일 공유 텍스처처럼 0/1 을 핑퐁하지 않는 이유는,
// 이 풀이 latest-only 로 소비되어 소비자가 손도 대지 않은 프레임이 정상적으로
// 버려지기 때문이다. 핑퐁이면 그때 키가 한쪽에 걸린 채 슬롯이 죽는다.
//
// 못 잡으면 이번 프레임은 건너뛴다. 기다려 봐야 생산자가 그 슬롯에 쓰고 있는
// 중이고, 곧 더 새로운 프레임이 온다.
bool ImageRenderLayer::UpdateSharedTexturePoolSlot(uint32_t slot, uint32_t& width, uint32_t& height)
{
	if (slot >= m_sharedPool.size())
		return false;

	SharedPoolSlot& poolSlot = m_sharedPool[slot];
	if (!poolSlot.texture || !poolSlot.keyedMutex)
		return false;

	const HRESULT acquireHr = poolSlot.keyedMutex->AcquireSync(
		kFramePoolAcquireKey, kFramePoolAcquireTimeout_ms);

	if (acquireHr != S_OK)
		return false;

	const bool updateSucceeded = CopySharedSourceToSingle(poolSlot.texture, width, height);

	// 뮤텍스를 놓기 전에 복사 명령을 제출한다. 놓고 나면 생산자가 이 슬롯에
	// 덮어쓸 수 있고, 그때까지 우리 복사가 큐에만 있으면 덮어쓴 내용을 읽는다.
	m_contextD3D->Flush();
	poolSlot.keyedMutex->ReleaseSync(kFramePoolAcquireKey);

	return updateSucceeded;
}

// 공유 소스에서 Single 버퍼로 옮기는 공통 부분.
//
// 이 텍스처를 읽어도 되는 상태인지는 호출자가 보장한다. 풀 경로는 뮤텍스를
// 잡고 들어오고, 단일 텍스처 경로는 애초에 동기화 없는 입력이다.
bool ImageRenderLayer::CopySharedSourceToSingle(ID3D11Texture2D* source, uint32_t& width, uint32_t& height)
{
	if (!source || !m_contextD3D)
		return false;

	D3D11_TEXTURE2D_DESC desc = {};
	source->GetDesc(&desc);

	if (desc.Width == 0 || desc.Height == 0 || desc.Width > TileFormat::kMaxTextureDim || desc.Height > TileFormat::kMaxTextureDim)
		return false;

	width = desc.Width;
	height = desc.Height;

	// 텍스처/공유텍스처 입력은 CopyResource 로 받으므로 CS 가 필요 없다.
	if (!CreateSingleBuffer(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
		false, m_mipMapGenerationEnabled))
		return false;

	D3D11_TEXTURE2D_DESC destinationDesc = {};
	m_singleTexture->GetDesc(&destinationDesc);

	if (CanCopyWholeTexture(destinationDesc, desc))
	{
		m_contextD3D->CopyResource(m_singleTexture, source);
	}
	else
	{
		m_contextD3D->CopySubresourceRegion(
			m_singleTexture, 0, 0, 0, 0, source, 0, nullptr);
		if (m_mipMapGenerationEnabled)
		{
			GenerateSingleMips();
		}
	}

	// 공유 텍스처 입력은 CPU 이미지 메모리를 갖지 않는다.
	if (m_image)
	{
		m_image->ReleaseBuffer();
	}

	ReleaseUnusedModeResources(RenderMode::Single);

	// 소스나 크기가 바뀔 때만 카메라 상태를 갱신한다.
	UpdateImageState(ImageInputSource::SharedTexture, width, height, RenderMode::Single, 0);

	return true;
}

void ImageRenderLayer::UpdateImageState(ImageInputSource source, uint32_t width, uint32_t height, RenderMode mode, uint32_t channel)
{
	const bool needFit =
		m_inputSource != source ||
		m_texWidth != width ||
		m_texHeight != height ||
		m_currentMode != mode ||
		(source == ImageInputSource::RawImage && m_inputChannel != channel);

	m_inputSource = source;
	m_inputChannel = (source == ImageInputSource::RawImage) ? channel : 0;
	m_currentMode = mode;
	m_texWidth = width;
	m_texHeight = height;

	// 새 이미지가 붙었으면 스트레치 범위가 달라진다. 다음에 필요할 때
	// 다시 굽는다(꺼져 있으면 영영 굽지 않으므로 비용이 없다).
	m_lutDirty = true;

	if (m_camera && needFit)
	{
		m_camera->SetImageSize(width, height);
		m_camera->FitInstant();
	}
}

RenderMode ImageRenderLayer::GetRenderMode() const
{
	return m_currentMode;
}

const Core::ImageType::ImageBase* ImageRenderLayer::GetImage() const
{
	return m_image;
}

ImageInputSource ImageRenderLayer::GetInputSource() const
{
	return m_inputSource;
}

// ────────────────────────────────────────────────────────────────────
// LUT
// ────────────────────────────────────────────────────────────────────

// LUT 는 Gray 전용이다.
//
// 컬러 이미지는 이미 표시용 공간으로 나온 결과라 다시 매핑할 이유가 없고,
// 의사색을 씌우면 실제 색 정보를 버리게 된다. 텍스처/공유텍스처 입력은
// BGRA 로 받으므로 여기에 해당한다.
bool ImageRenderLayer::SupportsLut() const
{
	if (m_inputSource != ImageInputSource::RawImage)
		return false;

	return m_inputChannel == 1;
}

void ImageRenderLayer::SetLutEnabled(bool enable)
{
	if (m_lutEnabled == enable)
		return;

	m_lutEnabled = enable;

	// 켜는 순간에만 굽는다. 꺼져 있는 동안은 히스토그램 비용이 0 이다.
	if (m_lutEnabled)
	{
		m_lutDirty = true;
	}
}

bool ImageRenderLayer::IsLutEnabled() const
{
	return m_lutEnabled;
}

void ImageRenderLayer::SetLutPreset(LutPreset preset)
{
	if (m_lutPreset == preset)
		return;

	m_lutPreset = preset;
	m_lutDirty = true;
}

LutPreset ImageRenderLayer::GetLutPreset() const
{
	return m_lutPreset;
}

// 현재 이미지에 맞는 LUT 텍스처를 만든다.
//
// 엔트리 수가 비트깊이에 묶여 있어서(8bit 256, 16bit 65536) 이미지가 바뀌면
// 텍스처를 다시 만들어야 할 수 있다.
bool ImageRenderLayer::EnsureLutTexture(uint32_t entryCount)
{
	if (m_lutTexture && m_lutEntryCount == entryCount)
		return true;

	SafeRelease(m_lutSRV);
	SafeRelease(m_lutTexture);
	m_lutEntryCount = 0;

	if (!m_device || entryCount == 0)
		return false;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = entryCount;
	desc.Height = 1;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_lutTexture)))
		return false;

	if (FAILED(m_device->CreateShaderResourceView(m_lutTexture, nullptr, &m_lutSRV)))
	{
		SafeRelease(m_lutTexture);
		return false;
	}

	m_lutEntryCount = entryCount;
	return true;
}

// 히스토그램을 훑어 범위를 구하고 테이블을 구워 올린다.
//
// 이 함수만 비용이 있다. 이미지가 바뀌거나 프리셋이 바뀔 때만 불린다.
bool ImageRenderLayer::RebuildLut()
{
	if (!SupportsLut() || !m_image || m_image->IsEmpty())
		return false;

	const uint32_t bitDepth = GetAttachedBitDepth();
	const uint32_t entryCount = LutTable::GetTextureEntryCount(bitDepth);

	if (!EnsureLutTexture(entryCount))
		return false;

	const LutRange range = LutTable::ComputeRange(
		m_image->ImageBuffer(),
		static_cast<uint32_t>(m_image->Width()),
		static_cast<uint32_t>(m_image->Height()),
		static_cast<uint32_t>(m_image->Stride()),
		static_cast<uint32_t>(m_image->Channel()),
		bitDepth);

	std::vector<uint8_t> table(static_cast<size_t>(entryCount) * 4);
	LutTable::Build(m_lutPreset, entryCount,
		LutTable::GetDomainMax(bitDepth), range, table.data());

	m_contextD3D->UpdateSubresource(
		m_lutTexture, 0, nullptr,
		table.data(), entryCount * 4, 0);

	m_lutDirty = false;

	return true;
}

// 이번 프레임에 LUT 셰이더를 쓸 수 있는가.
bool ImageRenderLayer::IsLutActive()
{
	if (!m_lutEnabled || !SupportsLut())
		return false;

	if (m_lutDirty && !RebuildLut())
		return false;

	return m_lutSRV != nullptr;
}

ID3D11Texture2D* ImageRenderLayer::GetSingleTexture() const
{
	return m_singleTexture;
}

bool ImageRenderLayer::CreateDeviceResources()
{
	ReleaseDeviceResources();

	uint32_t tileWidth(0), tileHeight(0);
	m_tileManager->GetTileSize(0, tileWidth, tileHeight);

	if (!CreateShaders()) goto FAIL;
	if (!CreateGeometry(tileWidth, tileHeight)) goto FAIL;
	if (!CreateSampler()) goto FAIL;
	if (!CreateConstantBuffer()) goto FAIL;
	if (!CreateRasterizerState()) goto FAIL;
	if (!CreateTileDynamicBuffer(128)) goto FAIL;

	return true;

FAIL:
	ReleaseDeviceResources();
	return false;
}

void ImageRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_samplerPoint);
	SafeRelease(m_samplerSingleLinear);
	SafeRelease(m_samplerSingleMagPoint);

	SafeRelease(m_lutSampler);
	SafeRelease(m_lutSRV);
	SafeRelease(m_lutTexture);
	m_lutEntryCount = 0;
	// 디바이스가 날아가면 테이블도 같이 사라진다. 다음 프레임에 다시 굽는다.
	m_lutDirty = true;

	SafeRelease(m_constantBuffer);
	SafeRelease(m_wireColorBuffer);

	SafeRelease(m_tileVertexBuffer);
	SafeRelease(m_indexBuffer);

	SafeRelease(m_rasterizerWireFrame);
	SafeRelease(m_rasterizerSolid);

	SafeRelease(m_singleTextureCS);

	SafeRelease(m_inputLayout);
	SafeRelease(m_vs);
	SafeRelease(m_ps);
	SafeRelease(m_grayPS);
	SafeRelease(m_grayLutPS);
	SafeRelease(m_wirePS);

	SafeRelease(m_singleSRV);
	SafeRelease(m_singleTexture);
	SafeRelease(m_singleUAV);
	SafeRelease(m_rawUploadBuffer);
	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_singleConvertCB);

	SafeRelease(m_sharedTexture);
	m_sharedHandle = nullptr;

	// 풀 텍스처도 사라진 디바이스 소속이다. 다시 열어 주는 것은 생산자를
	// 아는 호출자의 몫이라, 여기서는 놓기만 하고 DeviceRestored 이후
	// RegisterSharedTexturePool 이 다시 불릴 때까지 비워 둔다.
	UnregisterSharedTexturePool();

	// 지연 생성 리소스의 크기 추적값을 리셋해야 재생성 시 다시 잡힌다.
	m_singleTextureWidth = 0;
	m_singleTextureHeight = 0;
	m_singleTextureFormat = DXGI_FORMAT_UNKNOWN;
	m_singleTextureHasMipMaps = false;
	m_maxByteSize = 0;
}

// --- 컴파일된 셰이더(.cso) 찾기 ---
//
// 예전에는 D3DReadFileToBlob 에 L"../Shaders/X.cso" 를 그대로 넘겼다.
// 상대 경로는 프로세스의 현재 작업 디렉터리를 기준으로 풀리므로, 이 DLL 이
// 어디에 있든 상관없이 "호스트가 어느 폴더에서 실행됐는가" 가 결과를 정했다.
//
// 그래서 exe 를 자기 출력 폴더에서 실행하면 초기화가 실패했다 —
// x64\Release 에서 ..\Shaders 는 x64\Shaders 이고 거기엔 아무것도 없다.
// 프로젝트 폴더에서 F5 로 띄우면 우연히 맞아떨어져서 오래 드러나지 않았고,
// 증상은 "D3D11ImageView 초기화 실패" 한 줄뿐이라 원인과 거리가 멀었다.
//
// 이제는 이 DLL 이 놓인 위치를 기준으로 찾는다. 작업 디렉터리와 무관하다.
//
// 후보를 여럿 두는 이유는 배포 형태가 하나가 아니기 때문이다.
//   <dll>\Shaders\        배포 기본형. DLL 옆에 셰이더를 같이 둔다
//   <dll>\..\Shaders\     구성 폴더 위에 두는 경우
//   <dll>\..\..\Shaders\  현재 솔루션 배치 (바이너리가 x64\<Config> 에 있다)
//
// 마지막에 예전 경로를 한 번 더 시도한다. 작업 디렉터리를 맞춰 두고 쓰던
// 호스트(TestHost 가 그렇다)를 이 변경으로 깨뜨리지 않기 위해서다.
static bool GetOwnModuleDirectory(std::wstring& outDirectory)
{
	// 이 함수 자신의 주소로 모듈을 찾는다. exe 가 아니라 이 DLL 이어야 한다.
	// UNCHANGED_REFCOUNT 를 주는 이유는 참조를 늘리지 않기 위해서다 —
	// 늘리면 그만큼 FreeLibrary 를 해야 하는데 그럴 자리가 없다.
	HMODULE module = nullptr;
	if (!::GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&GetOwnModuleDirectory), &module) || !module)
	{
		return false;
	}

	// MAX_PATH 를 넘는 경로가 있을 수 있다. 잘린 경로로 파일을 찾으면
	// 엉뚱한 곳을 보게 되므로 들어갈 때까지 버퍼를 늘린다.
	std::wstring path(MAX_PATH, L'\0');
	for (;;)
	{
		const DWORD length = ::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
		if (length == 0)
			return false;

		if (length < path.size())
		{
			path.resize(length);
			break;
		}

		if (path.size() >= 32768u)
			return false;

		path.resize(path.size() * 2);
	}

	const size_t lastSeparator = path.find_last_of(L"\\/");
	if (lastSeparator == std::wstring::npos)
		return false;

	outDirectory.assign(path, 0, lastSeparator + 1);   // 구분자까지 포함한다
	return true;
}

static bool ShaderFileExists(const std::wstring& path)
{
	const DWORD attributes = ::GetFileAttributesW(path.c_str());
	return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static HRESULT LoadCompiledShader(const wchar_t* fileName, ID3DBlob** outBlob)
{
	std::wstring moduleDirectory;
	if (GetOwnModuleDirectory(moduleDirectory))
	{
		static const wchar_t* const kShaderRoots[] =
		{
			L"Shaders\\",
			L"..\\Shaders\\",
			L"..\\..\\Shaders\\",
		};

		for (const wchar_t* shaderRoot : kShaderRoots)
		{
			const std::wstring candidate = moduleDirectory + shaderRoot + fileName;

			// 존재를 먼저 확인한다. D3DReadFileToBlob 에 없는 경로를 주면
			// 실패 HRESULT 만 돌아와서 "없는 것" 과 "깨진 것" 이 구분되지 않는다.
			if (!ShaderFileExists(candidate))
				continue;

			return ::D3DReadFileToBlob(candidate.c_str(), outBlob);
		}
	}

	return ::D3DReadFileToBlob((std::wstring(L"..\\Shaders\\") + fileName).c_str(), outBlob);
}

bool ImageRenderLayer::CreateShaders()
{
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* wirePSBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* csBlob = nullptr;

	HRESULT hr = LoadCompiledShader(L"ImageVS.cso", &vsBlob);

	//HRESULT hr = ::D3DCompileFromFile(
	//	L"ImageVS.hlsl",
	//	nullptr,
	//	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	//	"main",
	//	"vs_5_0",
	//	D3DCOMPILE_ENABLE_STRICTNESS,
	//	0,
	//	&vsBlob,
	//	&errorBlob
	//);

	if (FAILED(hr))
	{
		SafeRelease(errorBlob);
		return false;
	}

	hr = LoadCompiledShader(L"ImagePS.cso", &psBlob);

	//hr = ::D3DCompileFromFile(
	//	L"ImagePS.hlsl",
	//	nullptr,
	//	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	//	"main",
	//	"ps_5_0",
	//	D3DCOMPILE_ENABLE_STRICTNESS,
	//	0,
	//	&psBlob,
	//	&errorBlob
	//);

	if (FAILED(hr))
	{
		SafeRelease(vsBlob);
		SafeRelease(errorBlob);
		return false;
	}

	hr = LoadCompiledShader(L"WireFramePS.cso", &wirePSBlob);

	if (FAILED(hr))
	{
		SafeRelease(vsBlob);
		SafeRelease(errorBlob);
		SafeRelease(wirePSBlob);
		return false;
	}

	// 단일 채널(R8/R16) 소스용 픽셀 셰이더.
	// 1채널 텍스처를 그대로 쓰고 3채널 복제를 셰이더에서 처리하므로
	// 업로드 시점의 4바이트 확장이 불필요해진다.
	{
		ID3DBlob* grayPSBlob = nullptr;
		hr = LoadCompiledShader(L"ImageGrayPS.cso", &grayPSBlob);

		if (FAILED(hr))
		{
			SafeRelease(vsBlob);
			SafeRelease(psBlob);
			SafeRelease(wirePSBlob);
			SafeRelease(errorBlob);
			SafeRelease(grayPSBlob);
			return false;
		}

		hr = m_device->CreatePixelShader(
			grayPSBlob->GetBufferPointer(),
			grayPSBlob->GetBufferSize(),
			nullptr,
			&m_grayPS
		);

		SafeRelease(grayPSBlob);

		if (FAILED(hr))
		{
			SafeRelease(vsBlob);
			SafeRelease(psBlob);
			SafeRelease(wirePSBlob);
			SafeRelease(errorBlob);
			return false;
		}
	}

	// 단일 채널 + LUT 픽셀 셰이더.
	//
	// 상수 버퍼로 분기하지 않고 셰이더를 둘로 나눈다. LUT 를 안 쓰는 경로가
	// 예전과 완전히 같은 코드로 남으므로 회귀 위험이 없고, 매 픽셀 분기도 없다.
	{
		ID3DBlob* grayLutPSBlob = nullptr;
		hr = LoadCompiledShader(L"ImageGrayLutPS.cso", &grayLutPSBlob);

		if (FAILED(hr))
		{
			SafeRelease(vsBlob);
			SafeRelease(psBlob);
			SafeRelease(wirePSBlob);
			SafeRelease(errorBlob);
			SafeRelease(grayLutPSBlob);
			return false;
		}

		hr = m_device->CreatePixelShader(
			grayLutPSBlob->GetBufferPointer(),
			grayLutPSBlob->GetBufferSize(),
			nullptr,
			&m_grayLutPS
		);

		SafeRelease(grayLutPSBlob);

		if (FAILED(hr))
		{
			SafeRelease(vsBlob);
			SafeRelease(psBlob);
			SafeRelease(wirePSBlob);
			SafeRelease(errorBlob);
			return false;
		}
	}

	hr = LoadCompiledShader(L"SingleConvertCS.cso", &csBlob);

	// Create shaders
	if (SUCCEEDED(hr))
	{
		hr = m_device->CreateComputeShader(
			csBlob->GetBufferPointer(),
			csBlob->GetBufferSize(),
			nullptr,
			&m_singleTextureCS
		);

		SafeRelease(csBlob);
	}


	hr = m_device->CreateVertexShader(
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		nullptr,
		&m_vs
	);

	if (FAILED(hr))
		goto CLEANUP;

	hr = m_device->CreatePixelShader(
		psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(),
		nullptr,
		&m_ps
	);

	if (FAILED(hr))
		goto CLEANUP;

	hr = m_device->CreatePixelShader(
		wirePSBlob->GetBufferPointer(),
		wirePSBlob->GetBufferSize(),
		nullptr,
		&m_wirePS
	);

	if (FAILED(hr))
		goto CLEANUP;

	{
		// Input Layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = m_device->CreateInputLayout(
			layout,
			ARRAYSIZE(layout),
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			&m_inputLayout
		);
	}

CLEANUP:
	SafeRelease(vsBlob);
	SafeRelease(psBlob);
	SafeRelease(errorBlob);
	SafeRelease(wirePSBlob);
	SafeRelease(csBlob);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateConstantBuffer()
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(ViewParams);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbWireFrameDesc = {};
	cbWireFrameDesc.ByteWidth = 16; // float4
	cbWireFrameDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbWireFrameDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbWireFrameDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = m_device->CreateBuffer(&cbWireFrameDesc, nullptr, &m_wireColorBuffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = 32;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_device->CreateBuffer(&cbd, nullptr, &m_singleConvertCB);
	if (FAILED(hr))
		return false;

	return true;
}

bool ImageRenderLayer::CreateGeometry(uint32_t tileWidth, uint32_t tileHeight)
{
	HRESULT hr = S_OK;

	// Index Buffer
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(kQuadIndices);
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = kQuadIndices;

	hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
	if (FAILED(hr))
		return false;

	return true;
}

bool ImageRenderLayer::CreateSampler()
{
	D3D11_SAMPLER_DESC sd = {};
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0.0;
	sd.MaxLOD = D3D11_FLOAT32_MAX;

	// Tiled 전용: 타일 경계 이음새를 피하기 위해 POINT 유지.
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	HRESULT hr = m_device->CreateSamplerState(&sd, &m_samplerPoint);
	if (FAILED(hr))
		return false;

	// ── Single 전용 샘플러 2종
	//
	// 축소는 둘 다 LINEAR + 밉 보간이다(축소 에일리어싱 제거). 갈리는 건
	// 확대 필터뿐이고, SetCommonShaderStates 가 배율을 보고 고른다.
	//
	// 왜 배율로 나누는가:
	//   - LINEAR 확대는 배율이 연속 변해도 부드럽지만 픽셀 경계가 흐려져
	//     검사에 쓰기 어렵다.
	//   - POINT 확대는 픽셀 경계가 살지만, 배율이 변하는 동안 텍셀->픽셀
	//     대응이 정수 단위로 흔들려 블록이 튀는 것처럼 보인다.
	//
	// 원래 코드는 D3D 의 기본 동작(zoom 1.0)에서 갈렸는데, 그 지점이 하필
	// fit -> 1:1 애니메이션이 매번 지나가는 구간이었다(1024px 이미지의 fit 은
	// 0.94 로 실측됨). 그래서 임계를 1.0 보다 충분히 위로 올린다.

	// 부드러운 쪽: 낮은 배율에서 사용
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = m_device->CreateSamplerState(&sd, &m_samplerSingleLinear);
	if (FAILED(hr))
		return false;

	// 선명한 쪽: 픽셀을 봐야 하는 높은 배율에서 사용
	sd.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	hr = m_device->CreateSamplerState(&sd, &m_samplerSingleMagPoint);
	if (FAILED(hr))
		return false;

	// LUT 전용 샘플러.
	//
	// 선형으로 둔다. 텍셀 중심을 정확히 맞추려면 셔이데가 LUT 크기를
	// 알아야 하고 그러면 상수 버퍼가 다시 필요해진다. 프리셋은
	// 모두 연속적인 색 램프라 이웃 항목을 섞어도 차이가 없고 더 매넄하다.
	D3D11_SAMPLER_DESC lutDesc = {};
	lutDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	lutDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	lutDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	lutDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	lutDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	lutDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = m_device->CreateSamplerState(&lutDesc, &m_lutSampler);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateRasterizerState()
{
	HRESULT hr = S_OK;
	D3D11_RASTERIZER_DESC rd = {};

	rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;

	hr = m_device->CreateRasterizerState(&rd, &m_rasterizerWireFrame);

	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;

	hr = m_device->CreateRasterizerState(&rd, &m_rasterizerSolid);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateTileDynamicBuffer(uint32_t maxTileCount)
{
	// 타일 1장 = 삼각형 2개 = 정점 6개.
	return CreateTileVertexBuffer(maxTileCount * kVerticesPerTile);
}

bool ImageRenderLayer::CreateTileVertexBuffer(uint32_t vertexCount)
{
	SafeRelease(m_tileVertexBuffer);

	// 실패 시 예전 용량이 남아 있으면 UpdateVertexBuffer 가 없는 버퍼에
	// 쓰려 하므로 먼저 0 으로 내린다.
	m_maxTileVertexCount = 0;

	if (!m_device || vertexCount == 0)
		return false;

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(GRAPHICS::BatchVertex) * vertexCount);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&bd, nullptr, &m_tileVertexBuffer)))
		return false;

	m_maxTileVertexCount = vertexCount;

	return true;
}

bool ImageRenderLayer::CreateSingleBuffer(uint32_t width, uint32_t height,
	DXGI_FORMAT format, bool needsComputeUpload, bool generateMipMaps)
{
	// UAV와 mip 구성까지 일치해야 재사용할 수 있다.
	// (같은 BGRA 라도 3채널 소스는 UAV 가 필요하고 4채널은 아니다)
	const bool hasUAV = (m_singleUAV != nullptr);

	if (m_singleTexture &&
		m_singleTextureWidth == width &&
		m_singleTextureHeight == height &&
		m_singleTextureFormat == format &&
		hasUAV == needsComputeUpload &&
		m_singleTextureHasMipMaps == generateMipMaps)
	{
		return true;
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = generateMipMaps ? 0u : 1u;
	desc.ArraySize = 1;
	// 소스 채널에 맞춘 포맷. Gray 는 R8/R16 이라 BGRA 대비 VRAM 이 1/4~1/2 다.
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT; // GPU resource used as a copy/render target
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (generateMipMaps)
	{
		// GENERATE_MIPS 는 RENDER_TARGET과 SHADER_RESOURCE를 함께 요구한다.
		desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	// BGR 24bit 확장은 컴퓨트 셰이더가 UAV 로 mip 0 에 쓴다.
	// 그 외(1채널 R8/R16, 4채널 BGRA)는 포맷이 소스와 일치해
	// UpdateSubresource 로 직행하므로 UAV 가 필요 없다.
	if (needsComputeUpload)
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	ID3D11Texture2D* newTexture = nullptr;
	ID3D11ShaderResourceView* newSRV = nullptr;
	ID3D11UnorderedAccessView* newUAV = nullptr;

	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &newTexture);
	if (FAILED(hr))
		return false;

	hr = m_device->CreateShaderResourceView(newTexture, nullptr, &newSRV);
	if (FAILED(hr))
	{
		SafeRelease(newTexture);
		return false;
	}

	if (needsComputeUpload)
	{
		// 컴퓨트 셰이더는 mip 0에만 쓴다. mip chain이 있으면 이후 GenerateMips가
		// 나머지를 채우고, 없으면 이 한 레벨이 곧 전체 텍스처다.
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		hr = m_device->CreateUnorderedAccessView(newTexture, &uavDesc, &newUAV);
		if (FAILED(hr))
		{
			SafeRelease(newSRV);
			SafeRelease(newTexture);
			return false;
		}
	}

	// 새 구성이 완성된 뒤 교체해야 생성 실패 시 현재 화면을 보존할 수 있다.
	SafeRelease(m_singleSRV);
	SafeRelease(m_singleUAV);
	SafeRelease(m_singleTexture);
	m_singleTexture = newTexture;
	m_singleSRV = newSRV;
	m_singleUAV = newUAV;

	m_singleTextureWidth = width;
	m_singleTextureHeight = height;
	m_singleTextureFormat = format;
	m_singleTextureHasMipMaps = generateMipMaps;

	return true;
}

bool ImageRenderLayer::RecreateSingleBufferForMipSetting(bool generateMipMaps)
{
	if (!m_singleTexture || m_currentMode != RenderMode::Single)
		return true;

	ID3D11Texture2D* previousTexture = m_singleTexture;
	previousTexture->AddRef();

	const bool needsComputeUpload = (m_singleUAV != nullptr);
	const uint32_t width = m_singleTextureWidth;
	const uint32_t height = m_singleTextureHeight;
	const DXGI_FORMAT format = m_singleTextureFormat;

	if (!CreateSingleBuffer(width, height, format, needsComputeUpload, generateMipMaps))
	{
		previousTexture->Release();
		return false;
	}

	// 이전 리소스의 mip 0은 모든 입력 타입에서 유효하다. 새 텍스처의 mip 0에
	// 복사한 뒤 필요한 경우에만 하위 mip을 다시 만든다.
	m_contextD3D->CopySubresourceRegion(
		m_singleTexture, 0, 0, 0, 0, previousTexture, 0, nullptr);
	previousTexture->Release();

	if (generateMipMaps)
	{
		GenerateSingleMips();
	}

	return true;
}

// mip 0 업로드가 끝난 뒤 나머지 밉 레벨을 GPU 하드웨어로 생성한다.
// 반드시 UAV 언바인딩 이후에 호출해야 한다(리소스 해저드).
void ImageRenderLayer::GenerateSingleMips()
{
	if (m_contextD3D && m_singleSRV)
	{
		m_contextD3D->GenerateMips(m_singleSRV);
	}
}

bool ImageRenderLayer::CreateRawUploadBuffer(uint32_t maxByteSize)
{
	// 이미 충분히 크면 재생성하지 않는다(축소는 하지 않음).
	if (m_rawUploadBuffer && m_maxByteSize >= maxByteSize)
		return true;

	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_rawUploadBuffer);

	D3D11_BUFFER_DESC desc = {};
	// RAW SRV 는 4바이트 단위이므로 4의 배수로 올림한다.
	maxByteSize = (maxByteSize + 3u) & ~3u;

	desc.ByteWidth = maxByteSize;
	desc.Usage = D3D11_USAGE_DYNAMIC; // dynamic buffer for Map/Unmap uploads
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; // allow raw buffer SRV access

	HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_rawUploadBuffer);
	if (FAILED(hr))
		return false;

	// Create shader resource view for raw upload buffer.
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS; // raw buffer uses typeless format
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = maxByteSize / 4; // number of 4-byte elements
	srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW; // RAW view flag

	hr = m_device->CreateShaderResourceView(m_rawUploadBuffer, &srvDesc, &m_rawUploadSRV);
	if (FAILED(hr))
		return false;

	m_maxByteSize = maxByteSize;

	return true;
}

bool ImageRenderLayer::OpenSharedResource(HANDLE sharedHandle)
{
	HRESULT hr = S_OK;

	if (sharedHandle != m_sharedHandle || m_sharedTexture == nullptr)
	{
		SafeRelease(m_sharedTexture);
		m_sharedHandle = nullptr;

		hr = m_device->OpenSharedResource(
			sharedHandle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_sharedTexture));

		if (SUCCEEDED(hr))
		{
			m_sharedHandle = sharedHandle;
		}
		else
		{
			sharedHandle = nullptr;
			return false; // opening the shared texture failed
		}
	}

	return true;
}

bool ImageRenderLayer::CheckViewChanged()
{
	Core::ShapeType::Rect2i currentRect = m_camera->GetViewImageRect();
	float currentZoom = m_camera->GetZoom();
	uint32_t currentTileCount = (uint32_t)m_tileManager->GetVisibleTiles().size();

	bool changed = false;

	// 1. Check whether visible image rect changed.
	if (m_prevViewRect.left != currentRect.left || m_prevViewRect.top != currentRect.top ||
		m_prevViewRect.right != currentRect.right || m_prevViewRect.bottom != currentRect.bottom)
	{
		changed = true;
	}

	// 2. Check whether zoom changed.
	if (std::abs(m_prevZoom - currentZoom) > 1e-6f)
	{
		changed = true;
	}

	// 3. Check whether visible tile count changed.
	if (m_prevTileCount != currentTileCount)
	{
		changed = true;
	}

	// Cache current view state for the next frame.
	if (changed)
	{
		m_prevViewRect = currentRect;
		m_prevZoom = currentZoom;
		m_prevTileCount = currentTileCount;
	}

	return changed;
}

void ImageRenderLayer::UploadSingleImage_GPU(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	// [1] Upload raw image data into the GPU raw upload buffer.
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_contextD3D->Map(m_rawUploadBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);
		const size_t packedRow = static_cast<size_t>(width) * channel;

		// mapped 포인터는 보통 Write-Combine 메모리다.
		//   - 순차 쓰기는 빠르고, 읽기는 재앙적으로 느리다(절대 읽지 않는다)
		//   - WC 쓰기는 코어 1개의 WC 버퍼 수에 묶이므로 여러 스레드가
		//     PCIe 를 더 잘 채운다 -> 행 범위를 나눠 병렬 memcpy 한다
		//
		// Map/Unmap 이 렌더 스레드에 묶여 있어 지속 큐가 아니라 fork-join 이다.
		// 호출 스레드도 청크를 처리하므로 워커가 0개여도 동작한다.
		const auto copyRows = [dst, data, packedRow, stride](uint32_t beginRow, uint32_t endRow)
			{
				for (uint32_t y = beginRow; y < endRow; ++y)
				{
					memcpy(dst + static_cast<size_t>(y) * packedRow,
						data + static_cast<size_t>(y) * stride,
						packedRow);
				}
			};

		// 청크가 너무 작으면 동기화 비용이 이득을 넘는다.
		// 대략 512KB 이상이 되도록 최소 행 수를 잡는다.
		constexpr size_t kMinChunkBytes = 512u * 1024u;
		const uint32_t minRows = static_cast<uint32_t>(
			(std::max)(size_t(1), kMinChunkBytes / (std::max)(size_t(1), packedRow)));

		copyRows(0, height);

		m_contextD3D->Unmap(m_rawUploadBuffer, 0);
	}

	// [2] Update conversion constants.
	struct {
		uint32_t width;
		uint32_t height;
		uint32_t stride; // packed GPU buffer stride: width * channel
		uint32_t channel;
	} cb;
	cb.width = width;
	cb.height = height;
	cb.stride = width * channel;
	cb.channel = channel;

	m_contextD3D->UpdateSubresource(m_singleConvertCB, 0, nullptr, &cb, 0, 0);

	// [3] Dispatch compute shader conversion.
	m_contextD3D->CSSetShader(m_singleTextureCS, nullptr, 0);
	m_contextD3D->CSSetConstantBuffers(0, 1, &m_singleConvertCB);
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &m_singleUAV, nullptr);
	m_contextD3D->CSSetShaderResources(0, 1, &m_rawUploadSRV);

	// 16x16 thread groups.
	m_contextD3D->Dispatch((width + 15) / 16, (height + 15) / 16, 1);

	// [4] Unbind UAV to release the resource hazard.
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}

bool ImageRenderLayer::RenderTiled()
{
	if (!m_tileManager)
		return true;

	// 가장 거친 LOD 프라이밍이 끝나기 전에는 이미지를 그리지 않는다.
	// 타일이 하나씩 채워지는 과정을 노출하지 않고, 준비되면 한 번에 나타난다.
	// (GLViewer_2DEngine 이 캐시 미준비 시 SwapBuffers 를 생략하는 것과 같은 효과)
	if (!m_tileManager->IsPrimed())
		return true;

	const auto& renderDataList = m_tileManager->GetRenderDataList();
	if (renderDataList.empty()) return true;

	bool viewChanged = CheckViewChanged();
	if (viewChanged || m_tileManager->HasPendingUploads() || m_renderVertices.empty())
	{
		m_renderVertices.clear();
		const float imageWidth = static_cast<float>(m_texWidth);
		const float imageHeight = static_cast<float>(m_texHeight);

		for (const auto& data : renderDataList)
		{
			const Core::ShapeType::Rect2i rect = m_tileManager->CalcTilePixelRect(data.targetKey);
			const float x0 = (float)rect.left;
			const float y0 = (float)rect.top;
			const float x1 = min((float)rect.right, imageWidth);
			const float y1 = min((float)rect.bottom, imageHeight);

			uint32_t tw = 0, th = 0;
			m_tileManager->GetTileSize(data.targetKey.lod, tw, th);
			const float scale = (float)(1 << data.targetKey.lod);

			float localU = (x1 - x0) / (tw * scale);
			float localV = (y1 - y0) / (th * scale);
			float finalU1 = data.u0 + (data.u1 - data.u0) * localU;
			float finalV1 = data.v0 + (data.v1 - data.v0) * localV;

			const uint32_t texIdx = data.tile->arrayIndex;
			m_renderVertices.push_back({ {x0, y0, 0.f}, {data.u0, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y0, 0.f}, {finalU1, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y1, 0.f}, {finalU1, finalV1}, texIdx });
			m_renderVertices.push_back({ {x0, y0, 0.f}, {data.u0, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y1, 0.f}, {finalU1, finalV1}, texIdx });
			m_renderVertices.push_back({ {x0, y1, 0.f}, {data.u0, finalV1}, texIdx });
		}

		UpdateVertexBuffer(m_renderVertices);
	}

	// Bind pipeline state and draw tiles.
	SetCommonShaderStates();

	uint32_t vertexOffset = 0;
	for (uint32_t i = 0; i < renderDataList.size(); )
	{
		uint32_t currentLOD = renderDataList[i].tile->key.lod;
		ID3D11ShaderResourceView* poolSRV = m_tileManager->GetPoolSRV(currentLOD);
		m_contextD3D->PSSetShaderResources(0, 1, &poolSRV);

		uint32_t batchCount = 0;
		while (i + batchCount < renderDataList.size() && renderDataList[i + batchCount].tile->key.lod == currentLOD)
			batchCount++;

		m_contextD3D->Draw(batchCount * 6, vertexOffset);
		vertexOffset += batchCount * 6;
		i += batchCount;
	}

	if (m_renderWireFrame)
	{
		m_contextD3D->RSSetState(m_rasterizerWireFrame);
		m_contextD3D->VSSetShader(m_vs, nullptr, 0);
		m_contextD3D->PSSetShader(m_wirePS, nullptr, 0);

		// Wire Frame Color
		constexpr float wireColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

		D3D11_MAPPED_SUBRESOURCE mappedCB{};
		if (SUCCEEDED(m_contextD3D->Map(m_wireColorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			memcpy(mappedCB.pData, &wireColor, sizeof(wireColor));
			m_contextD3D->Unmap(m_wireColorBuffer, 0);
		}

		// Wiare Frame Pixel Shader Constant Buffer(register b1)
		m_contextD3D->PSSetConstantBuffers(1, 1, &m_wireColorBuffer);

		// Unbind SRV before wireframe pass.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_contextD3D->PSSetShaderResources(0, 1, &nullSRV);

		m_contextD3D->Draw(static_cast<UINT>(m_renderVertices.size()), 0);
	}

	return true;
}

bool ImageRenderLayer::RenderSingle()
{
	if (!m_singleSRV) return false;

	bool viewChanged = CheckViewChanged();
	if (viewChanged || m_renderVertices.empty())
	{
		m_renderVertices.clear();
		float w = (float)m_texWidth;
		float h = (float)m_texHeight;

		// Single image quad uses normalized texture coordinates.
		m_renderVertices.push_back({ {0.f, 0.f, 0.f}, {0.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, 0.f, 0.f}, {1.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, h, 0.f}, {1.f, 1.f}, 0 });
		m_renderVertices.push_back({ {0.f, 0.f, 0.f}, {0.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, h, 0.f}, {1.f, 1.f}, 0 });
		m_renderVertices.push_back({ {0.f, h, 0.f}, {0.f, 1.f}, 0 });

		UpdateVertexBuffer(m_renderVertices);
	}

	SetCommonShaderStates();
	m_contextD3D->PSSetShaderResources(0, 1, &m_singleSRV);
	m_contextD3D->Draw(6, 0);

	return true;
}

void ImageRenderLayer::SetCommonShaderStates()
{
	uint32_t stride = sizeof(GRAPHICS::BatchVertex);
	uint32_t offset = 0;
	m_contextD3D->IASetInputLayout(m_inputLayout);
	m_contextD3D->IASetVertexBuffers(0, 1, &m_tileVertexBuffer, &stride, &offset);
	m_contextD3D->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_contextD3D->VSSetShader(m_vs, nullptr, 0);
	m_contextD3D->RSSetState(m_rasterizerSolid);

	// 단일 채널(R8/R16) 텍스처는 Sample() 이 (r,0,0,1) 을 주므로
	// .r 을 3채널로 복제하는 전용 픽셀 셰이더를 쓴다.
	const DXGI_FORMAT activeFormat = (m_currentMode == RenderMode::Single)
		? m_singleTextureFormat
		: (m_tileManager ? m_tileManager->GetFormat() : DXGI_FORMAT_B8G8R8A8_UNORM);

	ID3D11PixelShader* pixelShader = m_ps;

	if (TileFormat::IsSingleChannel(activeFormat))
	{
		// LUT 를 켜는 것은 셰이더를 바꿔 끼우는 것뿐이다. 텍스처 재업로드가
		// 없으므로 토글이 즉시 반영되고, 타일 캐시도 그대로 살아 있다.
		if (IsLutActive())
		{
			pixelShader = m_grayLutPS;

			m_contextD3D->PSSetShaderResources(1, 1, &m_lutSRV);
			m_contextD3D->PSSetSamplers(1, 1, &m_lutSampler);
		}
		else
		{
			pixelShader = m_grayPS;
		}
	}

	m_contextD3D->PSSetShader(pixelShader, nullptr, 0);

	// Single은 설정에 따라 mip 0만 있거나 전체 mip chain을 가진다. 두 경우
	// 모두 LINEAR 축소가 가능하다(한 레벨이면 mip 필터는 mip 0으로 고정됨).
	// Tiled는 타일 경계 이음새 때문에 POINT를 유지한다.
	ID3D11SamplerState* sampler = m_samplerPoint;

	if (m_currentMode == RenderMode::Single)
	{
		// 확대 필터를 배율로 고른다. 히스테리시스를 둬서 임계 근처를 오갈 때
		// 프레임마다 필터가 바뀌며 깜빡이는 것을 막는다.
		const float zoom = m_camera ? m_camera->GetZoom() : 1.0f;

		if (m_magPointActive)
		{
			if (zoom < kMagPointExitZoom)
				m_magPointActive = false;
		}
		else
		{
			if (zoom >= kMagPointEnterZoom)
				m_magPointActive = true;
		}

		sampler = m_magPointActive ? m_samplerSingleMagPoint : m_samplerSingleLinear;
	}

	m_contextD3D->PSSetSamplers(0, 1, &sampler);
}

void ImageRenderLayer::UpdateVertexBuffer(const std::vector<GRAPHICS::BatchVertex>& vertices)
{
	if (vertices.empty()) return;

	// ── 용량 검사
	//
	// 예전에는 m_maxTileVertexCount 를 대입만 하고 읽는 곳이 없어서, 아래
	// memcpy 가 Map 으로 받은 스테이징 영역 밖으로 나갈 수 있었다.
	// 버퍼는 128 타일(정점 768개) 고정인데 RenderTiled 는 렌더 대상 타일마다
	// 6 정점을 무제한 push 한다.
	//
	// 필요 타일 수는 뷰포트에 비례한다. 프리페치 마진이 타일 한 장이라
	// 가시 키의 상한은 (ceil(viewW/512)+2) * (ceil(viewH/512)+2) 이고,
	// 실제 렌더 목록은 상주/업로드 예산 때문에 그보다 작다.
	// (실측: 5124x1421 뷰포트 + LOD0 에서 35 타일. 상한 계산은 65 였다)
	// 뷰포트가 커지면 128 을 넘길 수 있으므로 고정 용량으로 두지 않는다.
	const size_t requiredCount = vertices.size();

	if (requiredCount > kMaxTileVertexLimit)
	{
		// 여기까지 오면 뷰포트/프리페치 계산이 깨진 것이다. 잘라 그리는 대신
		// 이 프레임의 갱신을 포기한다(직전 내용이 그대로 남아 한 프레임 낡는다).
		return;
	}

	if (!m_tileVertexBuffer || m_maxTileVertexCount < requiredCount)
	{
		// 재생성이 매 프레임 반복되지 않도록 2배씩 키운다.
		uint32_t newCount = (m_maxTileVertexCount > 0)
			? m_maxTileVertexCount
			: (128u * kVerticesPerTile);

		while (newCount < requiredCount)
		{
			newCount *= 2;
		}

		if (!CreateTileVertexBuffer(newCount))
			return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(m_contextD3D->Map(m_tileVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, vertices.data(), sizeof(GRAPHICS::BatchVertex) * requiredCount);
		m_contextD3D->Unmap(m_tileVertexBuffer, 0);
	}
}
