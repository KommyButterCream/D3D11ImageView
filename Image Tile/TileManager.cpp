#include "pch.h"
#include "TileManager.h"
#include "TilePool.h"

#include "../../../Module/Core/ShapeType/Rect2i.h"

#include <cassert>
#include <cmath>
#include <algorithm>

TileManager::~TileManager()
{
	ReleaseGPUResources();
}

void TileManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* contextD3D, const TileSystemDesc& desc)
{
	m_device = device;
	m_contextD3D = contextD3D;

	m_tileSystemDesc = desc;

	m_maxLOD = desc.maxLOD;
	assert(desc.lods.size() == m_maxLOD + 1);

	m_currentGpuCache.size = m_gpuUploadTextureSize;

	m_pools.resize(m_maxLOD + 1);

	for (uint32_t lod = 0; lod <= m_maxLOD; lod++)
	{
		const TileLODDesc& lodDesc = desc.lods[lod];

		m_pools[lod] = std::make_unique<TilePool>(device, lodDesc.tileSize, lodDesc.capacity, m_uploadMode);
	}

	if (!InitializeGPUResources(device))
	{

	}
}

bool TileManager::InitializeGPUResources(ID3D11Device* device)
{
	HRESULT hr = S_OK;

	if (m_uploadMode == UploadMode::Hybrid)
	{
		D3D11_BUFFER_DESC uploadDesc = {};

		uploadDesc.ByteWidth = m_gpuUploadTextureSize * m_gpuUploadTextureSize * 4;
		uploadDesc.Usage = D3D11_USAGE_DYNAMIC;
		uploadDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		uploadDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		uploadDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

		hr = device->CreateBuffer(&uploadDesc, nullptr, &m_rawUploadBuffer);
		if (FAILED(hr))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC uploadSRVDesc = {};
		uploadSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uploadSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		uploadSRVDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		uploadSRVDesc.BufferEx.NumElements = uploadDesc.ByteWidth / 4;

		hr = device->CreateShaderResourceView(m_rawUploadBuffer, &uploadSRVDesc, &m_rawUploadSRV);
		if (FAILED(hr))
			return false;

		ID3DBlob* csBlob = nullptr;
		hr = ::D3DReadFileToBlob(L"../Shaders/TileSamplingCS.cso", &csBlob);

		if (SUCCEEDED(hr))
		{
			hr = device->CreateComputeShader(
				csBlob->GetBufferPointer(),
				csBlob->GetBufferSize(),
				nullptr,
				&m_rawUploadtileCS
			);

			SafeRelease(csBlob);
		}

		D3D11_BUFFER_DESC cbd = {};
		cbd.Usage = D3D11_USAGE_DEFAULT;         // 잦은 업데이트를 위해 Dynamic 권장
		cbd.ByteWidth = 32;                     // 16바이트 배수 유지 (현재 4개 uint = 16바이트지만 여유있게 32)
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		hr = device->CreateBuffer(&cbd, nullptr, &m_csConstantBuffer); // 공용 멤버 변수 사용 가정
		if (FAILED(hr)) return false;
	}

	return SUCCEEDED(hr);
}

void TileManager::ReleaseGPUResources()
{
	SafeRelease(m_rawUploadtileCS);
	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_rawUploadBuffer);
	SafeRelease(m_csConstantBuffer);
}

void TileManager::ClearPools()
{
	for (auto& pool : m_pools)
	{
		pool->Clear();
	}
	m_visibleTiles.clear();
}

void TileManager::SetUploadMode(UploadMode mode)
{
	m_uploadMode = mode;
}

void TileManager::UpdateVisibleTiles(const Core::ShapeType::Rect2i& viewPixelRect, float zoom, uint64_t frameID,
	const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel)
{
	m_previousVisibleTiles = m_visibleTiles;

	//if (m_uploadMode == UploadMode::Hybrid) 
	//{
	//	static uint64_t lastProcessedFrame = 0;
	//	if (lastProcessedFrame != frameID) 
	//	{
	//		// 프레임이 바뀌었으므로 이전 GPU 캐시는 더 이상 유효하지 않음
	//		m_currentGpuCache.isValid = false;
	//		lastProcessedFrame = frameID;
	//	}
	//}

	m_visibleTiles.clear();
	m_renderDataList.clear();
	m_hasPendingUploads = false;

	const uint32_t LODLevel = SelectLOD(zoom);
	TilePool* pool = m_pools[LODLevel].get();

	std::vector<TileKey> keys;
	keys.reserve(32);
	CalcVisibleKeys(LODLevel, viewPixelRect, keys);

	uint32_t uploadCount = 0;
	const uint32_t MAX_UPLOAD_PER_FRAME = 4; // 한 프레임에 최대 2개만 새로 생성

	for (const TileKey& key : keys)
	{
		Tile* tile = pool->Acquire(key, frameID);
		if (!tile)
			continue;

		if (tile->state == TileState::None)
		{
			if (m_uploadMode == UploadMode::OnlyCPU)
			{
				// CPU 모드: 멀티스레드로 직접 픽셀 샘플링하여 전송
				if (uploadCount < MAX_UPLOAD_PER_FRAME)
				{
					UploadTileData_CPU(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);

					//UploadTileData_CPU_Parallel(pool, tile, imageData, imageWidth, imageStride, imageHeight);
					tile->state = TileState::Ready;
					uploadCount++;
				}
				else
				{
					// 이번 프레임엔 로드하지 않음 (건너뜀)
					m_hasPendingUploads = true;
					tile->state = TileState::None;
					//continue;
				}
			}
			else if (m_uploadMode == UploadMode::Hybrid)
			{
				// 이 타일이 현재 캐시 영역에 있는지 확인
				uint32_t lodScale = 1u << tile->key.lod;
				uint32_t targetRegionSize = m_tileSystemDesc.lods[tile->key.lod].tileSize * lodScale;
				uint32_t targetStartX = tile->key.x * targetRegionSize;
				uint32_t targetStartY = tile->key.y * targetRegionSize;

				bool inCache = (m_currentGpuCache.isValid &&
					m_currentGpuCache.lod == tile->key.lod &&
					m_currentGpuCache.Contains(targetStartX, targetStartY, targetRegionSize));

				if (inCache)
				{
					// 캐시에 있으면 전송 비용이 거의 없으므로 즉시 업로드
					UploadTileData_GPU(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);
					tile->state = TileState::Ready;
				}
				else if (uploadCount < MAX_UPLOAD_PER_FRAME)
				{
					// 캐시에 없으면 새로 Map 해야 하므로 횟수 제한 적용
					if (tile->key.lod <= 3)
						UploadTileData_GPU(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);
					else
						UploadTileData_CPU(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);

					tile->state = TileState::Ready;
					uploadCount++;
				}
				else
				{
					// 이번 프레임엔 로드하지 않음 (건너뜀)
					m_hasPendingUploads = true;
					tile->state = TileState::None;
					//continue;
				}
			}
		}

		if (tile->state == TileState::Ready)
			tile->state = TileState::Active;

		// 2. [핵심] Fallback 로직 적용
		TileRenderData renderData;
		renderData.targetKey = key; // 원래 그려져야 할 위치/LOD 정보

		if (tile->state == TileState::Active)
		{
			renderData.tile = tile;
			renderData.u0 = 0.0f; renderData.v0 = 0.0f; renderData.u1 = 1.0f; renderData.v1 = 1.0f;
		}
		else
		{
			// 현재 타일이 없으면 부모를 찾아본다
			TileKey parentKey;
			Tile* parent = FindAvailableParent(key, frameID, parentKey);
			if (parent)
			{
				renderData.tile = parent;

				// 부모와 자식의 해상도 차이 비율 (예: 1단계 차이면 2, 2단계 차이면 4)
				uint32_t ratio = 1 << (parentKey.lod - key.lod);

				float size = 1.0f / (float)ratio;

				// 부모 타일 한 변에 들어가는 자식 타일의 개수가 ratio개이므로
				// 자식 좌표(x, y)를 ratio로 나눈 나머지가 부모 내에서의 인덱스가 됩니다.
				renderData.u0 = (key.x % ratio) * size;
				renderData.v0 = (key.y % ratio) * size;
				renderData.u1 = renderData.u0 + size;
				renderData.v1 = renderData.v0 + size;
			}
			else
			{
				// 부모도 없으면 그리지 않음 (혹은 가장 낮은 LOD 텍스처를 씌움)
				continue;
			}
		}


		////
		tile->lastFrameUsed = frameID;
		m_visibleTiles.push_back(tile);
		m_renderDataList.push_back(renderData);
	}

	for (Tile* oldTile : m_previousVisibleTiles)
	{
		oldTile->lastFrameUsed = frameID;
	}

	for (uint32_t i = 0; i <= m_maxLOD; i++)
	{
		//if (i == LODLevel /* m_pools[i]->HasInactiveTiles()*/)
		//	m_pools[i]->Evict(frameID, 120 * 10);
	}
}

Tile* TileManager::FindAvailableParent(const TileKey& childKey, uint64_t frameID, TileKey& outParentKey)
{
	TileKey currentKey = childKey;

	// 현재 LOD보다 숫자가 큰(저해상도) 쪽으로 탐색
	for (uint32_t l = childKey.lod + 1; l <= m_maxLOD; ++l)
	{
		currentKey.lod = l;
		currentKey.x /= 2; // 부모는 자식의 절반 좌표
		currentKey.y /= 2;

		Tile* parent = m_pools[l]->Find(currentKey);
		if (parent && parent->state == TileState::Active)
		{
			parent->lastFrameUsed = frameID; // 사용 중임을 표시해 캐시 유지
			outParentKey = currentKey;
			return parent;
		}
	}
	return nullptr;
}

void TileManager::UploadTileData_CPU(TilePool* pool, Tile* tile, const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel)
{
	ID3D11Texture2D* textureArray = pool->GetTextureArray();
	if (!textureArray)
		return;

	const uint32_t lodScale = 1u << tile->key.lod;
	const uint32_t tileSize = m_tileSystemDesc.lods[tile->key.lod].tileSize;

	const uint32_t srcStartX = tile->key.x * tileSize * lodScale;
	const uint32_t srcStartY = tile->key.y * tileSize * lodScale;

	if (m_cpuScratchBuffer.size() < static_cast<size_t>(tileSize * tileSize * 4))
	{
		m_cpuScratchBuffer.resize(static_cast<size_t>(tileSize * tileSize * 4));
	}

	uint32_t* pDestData = reinterpret_cast<uint32_t*>(m_cpuScratchBuffer.data());

	if (channel == 1) // Gray
	{
		for (uint32_t y = 0; y < tileSize; y++)
		{
			uint32_t currentSrcY = srcStartY + (y * lodScale);
			if (currentSrcY >= imageHeight) continue;

			uint32_t* pDstRow = pDestData + (y * tileSize);
			const uint8_t* pSrcRowBase = imageData + (currentSrcY * imageStride);

			for (uint32_t x = 0; x < tileSize; x++)
			{
				uint32_t currentSrcX = srcStartX + (x * lodScale);
				if (currentSrcX < imageWidth)
				{
					uint8_t g = pSrcRowBase[currentSrcX];
					pDstRow[x] = (0xFF << 24) | (g << 16) | (g << 8) | g;
				}
			}
		}
	}
	else if (channel == 3) // BGR
	{
		for (uint32_t y = 0; y < tileSize; y++)
		{
			uint32_t currentSrcY = srcStartY + (y * lodScale);
			if (currentSrcY >= imageHeight) continue;

			uint32_t* pDstRow = pDestData + (y * tileSize);
			const uint8_t* pSrcRowBase = imageData + (currentSrcY * imageStride);

			for (uint32_t x = 0; x < tileSize; x++)
			{
				uint32_t currentSrcX = srcStartX + (x * lodScale);
				if (currentSrcX < imageWidth)
				{
					const uint8_t* pPixel = pSrcRowBase + (currentSrcX * 3);
					pDstRow[x] = (0xFF << 24) | (pPixel[2] << 16) | (pPixel[1] << 8) | pPixel[0];
				}
			}
		}
	}
	else if (channel == 4) // BGRA
	{
		if (lodScale == 1) { // 추가된 최적화 경로
			for (uint32_t y = 0; y < tileSize; y++)
			{
				uint32_t currentSrcY = srcStartY + y;
				if (currentSrcY >= imageHeight) break;
				uint32_t* pDstRow = pDestData + (y * tileSize);
				const uint8_t* pSrcRowBase = imageData + (currentSrcY * imageStride) + (srcStartX * 4);

				// 이미지 가로 경계 처리 후 복사
				uint32_t copyWidth = (srcStartX + tileSize <= imageWidth) ? tileSize :
					(imageWidth > srcStartX ? imageWidth - srcStartX : 0);
				if (copyWidth > 0) memcpy(pDstRow, pSrcRowBase, copyWidth * 4);
			}
		}
		else { // 기존 샘플링 루프
			for (uint32_t y = 0; y < tileSize; y++)
			{
				uint32_t currentSrcY = srcStartY + (y * lodScale);
				if (currentSrcY >= imageHeight) continue;
				uint32_t* pDstRow = pDestData + (y * tileSize);
				const uint32_t* pSrcRowBase = reinterpret_cast<const uint32_t*>(imageData + (currentSrcY * imageStride));
				for (uint32_t x = 0; x < tileSize; x++) {
					uint32_t currentSrcX = srcStartX + (x * lodScale);
					if (currentSrcX < imageWidth) pDstRow[x] = pSrcRowBase[currentSrcX];
				}
			}
		}
	}

	const uint32_t subresourceIndex = ::D3D11CalcSubresource(0, tile->arrayIndex, 1);

	m_contextD3D->UpdateSubresource(
		textureArray,
		subresourceIndex,
		nullptr,
		m_cpuScratchBuffer.data(),
		tileSize * 4,
		0
	);
}

void TileManager::UploadTileData_GPU(TilePool* pool, Tile* tile, const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel)
{
	if (m_uploadMode != UploadMode::Hybrid)
		return;

	if (!m_rawUploadBuffer || !m_contextD3D || !m_csConstantBuffer || !m_rawUploadSRV)
		return;

	const uint32_t lodScale = 1u << tile->key.lod;
	const uint32_t tileSize = m_tileSystemDesc.lods[tile->key.lod].tileSize;
	const uint32_t targetRegionSize = tileSize * lodScale;

	// 타일의 실제 원본 좌표
	const uint32_t targetStartX = tile->key.x * targetRegionSize;
	const uint32_t targetStartY = tile->key.y * targetRegionSize;

	// [1. 캐시 체크] 현재 GPU Raw 버퍼에 이 타일 영역이 이미 들어있는가?
	bool needUpload = true;
	if (m_currentGpuCache.isValid &&
		m_currentGpuCache.lod == tile->key.lod &&
		m_currentGpuCache.Contains(targetStartX, targetStartY, targetRegionSize))
	{
		needUpload = false; // 캐시 히트! 전송 생략
	}

	if (needUpload)
	{
		// 캐시 그리드(예: 8k 단위)에 맞춰 시작 좌표 정렬
		uint32_t cacheStartX = (targetStartX / m_gpuUploadTextureSize) * m_gpuUploadTextureSize;
		uint32_t cacheStartY = (targetStartY / m_gpuUploadTextureSize) * m_gpuUploadTextureSize;

		D3D11_MAPPED_SUBRESOURCE mapped;
		// D3D11_MAP_WRITE_DISCARD를 사용하여 이전 프레임의 데이터 장벽 제거
		if (SUCCEEDED(m_contextD3D->Map(m_rawUploadBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			uint8_t* pDst = static_cast<uint8_t*>(mapped.pData);

			// 캐시 크기(m_gpuUploadTextureSize)만큼 원본에서 복사
			for (uint32_t y = 0; y < m_gpuUploadTextureSize; ++y)
			{
				uint32_t currentSrcY = cacheStartY + y;
				if (currentSrcY >= imageHeight) break;

				// CPU는 단순히 메모리 복사만 수행 (매우 빠름)
				const uint8_t* pSrc = imageData + (currentSrcY * imageStride) + (cacheStartX * channel);

				// 경계 처리: 이미지 폭을 벗어나지 않게 복사
				uint32_t copyWidth = (cacheStartX + m_gpuUploadTextureSize <= imageWidth) ?
					m_gpuUploadTextureSize : (imageWidth > cacheStartX ? imageWidth - cacheStartX : 0);

				if (copyWidth > 0)
				{
					memcpy(pDst + (y * m_gpuUploadTextureSize * channel), pSrc, copyWidth * channel);
				}
			}
			m_contextD3D->Unmap(m_rawUploadBuffer, 0);

			// 캐시 정보 갱신
			m_currentGpuCache.lod = tile->key.lod;
			m_currentGpuCache.startX = cacheStartX;
			m_currentGpuCache.startY = cacheStartY;
			m_currentGpuCache.isValid = true;
		}
	}

	// [2. 쉐이더용 오프셋 계산] 8k 캐시 버퍼 내에서의 상대적 위치
	uint32_t relativeX = targetStartX - m_currentGpuCache.startX;
	uint32_t relativeY = targetStartY - m_currentGpuCache.startY;

	// [3. 상수 버퍼 업데이트]
	struct {
		uint32_t lodScale;
		uint32_t channels;
		uint32_t destIndex;
		uint32_t cacheWidth; // Raw Buffer의 가로폭 (m_gpuUploadTextureSize)
		uint32_t srcOffsetX; // 캐시 내 시작 X
		uint32_t srcOffsetY; // 캐시 내 시작 Y
		uint32_t padding[2]; // 16바이트 정렬용
	} cb;

	cb.lodScale = lodScale;
	cb.channels = channel;
	cb.destIndex = tile->arrayIndex;
	cb.cacheWidth = m_gpuUploadTextureSize;
	cb.srcOffsetX = relativeX;
	cb.srcOffsetY = relativeY;

	m_contextD3D->UpdateSubresource(m_csConstantBuffer, 0, nullptr, &cb, 0, 0);

	// [4. Dispatch]
	m_contextD3D->CSSetShader(m_rawUploadtileCS, nullptr, 0);
	m_contextD3D->CSSetConstantBuffers(0, 1, &m_csConstantBuffer);
	m_contextD3D->CSSetShaderResources(0, 1, &m_rawUploadSRV);

	ID3D11UnorderedAccessView* uav = pool->GetUavArray();
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	m_contextD3D->Dispatch((tileSize + 15) / 16, (tileSize + 15) / 16, 1);

	// [5. Unbind] (핵심!)
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_contextD3D->CSSetShaderResources(0, 1, &nullSRV);
	ID3D11Buffer* nullCB = nullptr;
	m_contextD3D->CSSetConstantBuffers(0, 1, &nullCB);
}

uint32_t TileManager::SelectLOD(float zoom) const
{
	if (zoom >= 1.0f)
		return 0;

	const float lodF = std::log2f(1.0f / zoom);
	uint32_t lod = std::clamp(
		static_cast<uint32_t>(std::floor(lodF)),
		0u, m_maxLOD);

	// 히스테리시스
	if (lod > m_lastLOD && lodF < m_lastLOD + 0.2f)
		lod = m_lastLOD;

	m_lastLOD = lod;

	return lod;
}

void TileManager::CalcVisibleKeys(uint32_t LODLevel, const Core::ShapeType::Rect2i& view, std::vector<TileKey>& outKeys) const
{
	const uint32_t scale = 1u << LODLevel;

	const auto& tileLODDesc = m_tileSystemDesc.lods[LODLevel];
	const uint32_t tileSize = tileLODDesc.tileSize;

	const uint32_t startX = (view.left / scale) / tileSize;
	const uint32_t endX = ((view.right - 1) / scale) / tileSize;
	const uint32_t startY = (view.top / scale) / tileSize;
	const uint32_t endY = ((view.bottom - 1) / scale) / tileSize;

	for (uint32_t y = startY; y <= endY; y++)
	{
		for (uint32_t x = startX; x <= endX; x++)
		{
			outKeys.push_back({ (uint16_t)LODLevel, x, y });
		}
	}
}

const std::vector<Tile*>& TileManager::GetVisibleTiles() const
{
	return m_visibleTiles;
}

const std::vector<TileRenderData>& TileManager::GetRenderDataList() const
{
	return m_renderDataList;
}

void TileManager::GetTileSize(uint32_t lodLevel, uint32_t& tileWidth, uint32_t& tileHeight) const
{
	tileWidth = m_tileSystemDesc.lods[lodLevel].tileSize;
	tileHeight = m_tileSystemDesc.lods[lodLevel].tileSize;
}

Core::ShapeType::Rect2i TileManager::CalcTilePixelRect(const TileKey& key) const
{
	const auto& tileLODDesc = m_tileSystemDesc.lods[key.lod];

	const uint32_t tileSize = tileLODDesc.tileSize;
	const uint32_t scale = 1u << key.lod;

	Core::ShapeType::Rect2i rect = {};
	rect.left = key.x * tileSize * scale;
	rect.top = key.y * tileSize * scale;
	rect.right = rect.left + tileSize * scale;
	rect.bottom = rect.top + tileSize * scale;

	return rect;
}

ID3D11ShaderResourceView* TileManager::GetPoolSRV(uint32_t lodLevel)
{
	if (m_pools.empty())
		return nullptr;

	if (m_pools.size() <= lodLevel)
		return nullptr;

	return m_pools[lodLevel]->GetSrvArray();
}

bool TileManager::HasPendingUploads() const
{
	return m_hasPendingUploads;
}




