#include "pch.h"
#include "TileManager.h"
#include "TilePool.h"
#include "TileFormat.h"

#include "../D3D11ImageView/HighResolutionTimer.h"

#include "../../../Module/Core/ShapeType/Rect2i.h"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace
{
	constexpr uint32_t kTileSize = 512;

	// 팬/줌 왕복 시 재업로드를 막기 위한 작업세트 유지 계수.
	constexpr uint32_t kRetainFactor = 2;

	// 프레임당 타일 업로드에 허용할 시간.
	// 예전에는 MAX_UPLOAD_PER_FRAME=4 로 장수를 셌지만, LOD0 의 연속 memcpy 와
	// 고LOD 의 스트라이드 샘플링은 비용이 수십 배 차이나서 프레임 시간을
	// 예측할 수 없었다.
	constexpr double kUploadBudgetMs = 2.0;

	// Evict 임계값(프레임 수). 이보다 오래 안 쓰인 타일은 풀에 반납한다.
	constexpr uint64_t kEvictFrameThreshold = 600;

	inline uint32_t CeilDiv(uint32_t a, uint32_t b) noexcept
	{
		return (b == 0) ? 0 : ((a + b - 1) / b);
	}
}

TileManager::TileManager()
	: m_uploadTimer(std::make_unique<HighResolutionTimer>())
{
}

TileManager::~TileManager()
{
	ReleasePools();
}

void TileManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* contextD3D)
{
	m_device = device;
	m_contextD3D = contextD3D;
}

bool TileManager::NeedsReconfigure(uint32_t viewWidth, uint32_t viewHeight) const
{
	if (m_pools.empty())
		return true;

	// 작업세트는 ceil(view/tileSize)+1 로 결정되므로, 그 값이 달라질 때만
	// 재구성한다. 창을 몇 픽셀 끄는 것으로 풀을 다시 만들지 않는다.
	const uint32_t oldGridW = CeilDiv(m_configuredViewWidth, kTileSize) + 1;
	const uint32_t oldGridH = CeilDiv(m_configuredViewHeight, kTileSize) + 1;
	const uint32_t newGridW = CeilDiv(viewWidth, kTileSize) + 1;
	const uint32_t newGridH = CeilDiv(viewHeight, kTileSize) + 1;

	if (newGridW > oldGridW || newGridH > oldGridH)
		return true;

	// maxLOD 는 뷰포트의 짧은 변에 의존한다.
	const uint32_t imageMax = (std::max)(m_tileSystemDesc.imageWidth, m_tileSystemDesc.imageHeight);
	const uint32_t oldViewMin = (std::max)(1u, (std::min)(m_configuredViewWidth, m_configuredViewHeight));
	const uint32_t newViewMin = (std::max)(1u, (std::min)(viewWidth, viewHeight));

	if (imageMax == 0)
		return false;

	const auto lodFor = [imageMax](uint32_t viewMin)
		{
			const double ratio = static_cast<double>(imageMax) / static_cast<double>(viewMin);
			return (ratio <= 1.0) ? 0u : static_cast<uint32_t>(std::ceil(std::log2(ratio)));
		};

	return lodFor(newViewMin) != lodFor(oldViewMin);
}

bool TileManager::Configure(uint32_t imageWidth, uint32_t imageHeight,
	uint32_t channel, uint32_t bitDepth,
	uint32_t viewWidth, uint32_t viewHeight)
{
	if (!m_device || imageWidth == 0 || imageHeight == 0)
		return false;

	ReleasePools();

	TileSystemDesc desc = {};
	desc.imageWidth = imageWidth;
	desc.imageHeight = imageHeight;
	desc.sourceChannel = channel;
	desc.format = TileFormat::ResolveTextureFormat(channel, bitDepth);

	// ── maxLOD: "이미지 전체가 뷰포트에 들어오는 LOD"
	//
	// 이 값이어야 fit 줌에서 SelectLOD 가 clamp 되지 않고, 1 텍셀 ≈ 1 화면 픽셀이
	// 유지된다. 타일 텍스처는 MipLevels=1 이므로 clamp 되어 추가 축소가 걸리면
	// 축소 에일리어싱이 그대로 보인다.
	const uint32_t imageMax = (std::max)(imageWidth, imageHeight);
	const uint32_t viewMin = (std::max)(1u, (std::min)(viewWidth, viewHeight));

	const double lodRatio = static_cast<double>(imageMax) / static_cast<double>(viewMin);
	desc.maxLOD = (lodRatio <= 1.0)
		? 0u
		: static_cast<uint32_t>(std::ceil(std::log2(lodRatio)));

	// ── 작업세트
	//
	// 활성 LOD 에서 타일 1 장은 화면에서 항상 tileSize 픽셀을 덮는다
	// (zoom ≈ 2^-L 이므로 tileSize * 2^L * zoom = tileSize).
	// 따라서 필요 타일 수는 이미지 크기와 무관하게 뷰포트만의 함수다.
	const uint32_t gridW = CeilDiv(viewWidth, kTileSize) + 1;
	const uint32_t gridH = CeilDiv(viewHeight, kTileSize) + 1;
	const uint32_t workingSet = (std::max)(1u, gridW * gridH);

	desc.workingSetTiles = workingSet;

	// ── LOD 별 용량
	//
	// min(전체 타일 수, 작업세트 * 유지계수).
	// maxLOD 정의상 그 레벨의 전체 타일 수는 작업세트 이하이므로,
	// 이 식이 maxLOD 를 자동으로 전량 상주시킨다 -> 부모 fallback 항상 성공.
	desc.lods.resize(desc.maxLOD + 1);
	for (uint32_t lod = 0; lod <= desc.maxLOD; ++lod)
	{
		const uint32_t span = kTileSize << lod;
		const uint32_t whole = (std::max)(1u, CeilDiv(imageWidth, span) * CeilDiv(imageHeight, span));

		desc.lods[lod].tileSize = kTileSize;
		desc.lods[lod].capacity = (std::min)(whole, workingSet * kRetainFactor);
	}

	m_tileSystemDesc = desc;
	m_maxLOD = desc.maxLOD;
	m_lastLOD = 0;
	m_configuredViewWidth = viewWidth;
	m_configuredViewHeight = viewHeight;

	m_pools.resize(desc.maxLOD + 1);
	for (uint32_t lod = 0; lod <= desc.maxLOD; ++lod)
	{
		m_pools[lod] = std::make_unique<TilePool>(
			m_device, desc.lods[lod].tileSize, desc.lods[lod].capacity, desc.format);

		if (!m_pools[lod]->GetSrvArray())
		{
			ReleasePools();
			return false;
		}
	}

	m_primed = false;
	m_hasPendingUploads = false;

	return true;
}

void TileManager::ReleasePools()
{
	m_pools.clear();
	m_visibleTiles.clear();
	m_previousVisibleTiles.clear();
	m_renderDataList.clear();
	m_visibleKeys.clear();
	m_primed = false;
	m_hasPendingUploads = false;
}

void TileManager::ClearPools()
{
	for (auto& pool : m_pools)
	{
		if (pool)
			pool->Clear();
	}

	m_visibleTiles.clear();
	m_previousVisibleTiles.clear();
	m_renderDataList.clear();
	m_primed = false;
}

void TileManager::PrimeCoarsestLevel(const uint8_t* imageData, uint32_t imageWidth,
	uint32_t imageStride, uint32_t imageHeight, uint32_t channel)
{
	if (m_pools.empty() || !imageData)
		return;

	TilePool* pool = m_pools[m_maxLOD].get();
	if (!pool)
		return;

	const uint32_t span = m_tileSystemDesc.lods[m_maxLOD].tileSize << m_maxLOD;
	const uint32_t countX = CeilDiv(imageWidth, span);
	const uint32_t countY = CeilDiv(imageHeight, span);

	// 채울 타일 목록을 먼저 모은다.
	std::vector<Tile*> targets;
	targets.reserve(static_cast<size_t>(countX) * countY);

	for (uint32_t ty = 0; ty < countY; ++ty)
	{
		for (uint32_t tx = 0; tx < countX; ++tx)
		{
			const TileKey key{ static_cast<uint16_t>(m_maxLOD), tx, ty };

			Tile* tile = pool->Acquire(key, 0);
			if (tile && tile->state != TileState::Resident)
				targets.push_back(tile);
		}
	}

	// 프라이밍은 "끝날 때까지 안 그리는" 동기 작업이다.
	// 예산을 무시하고 한 번에 채운다.
	for (Tile* tile : targets)
	{
		ProduceTile(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);
	}

	m_primed = true;
}

void TileManager::UpdateVisibleTiles(const Core::ShapeType::Rect2i& viewPixelRect, float zoom, uint64_t frameID,
	const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel,
	bool cameraSettled)
{
	if (m_pools.empty())
		return;

	m_previousVisibleTiles.swap(m_visibleTiles);

	m_visibleTiles.clear();
	m_renderDataList.clear();
	m_hasPendingUploads = false;

	const uint32_t LODLevel = SelectLOD(zoom);
	TilePool* pool = m_pools[LODLevel].get();
	if (!pool)
		return;

	// ── 프리페치 마진
	// GetViewImageRect() 는 보이는 영역으로 clamp 되어 마진이 0 이다.
	// 한 타일만큼 넓혀서 조금만 움직여도 새 타일이 필요해지는 것을 막는다.
	const int32_t margin = static_cast<int32_t>(m_tileSystemDesc.lods[LODLevel].tileSize << LODLevel);

	Core::ShapeType::Rect2i prefetchRect = viewPixelRect;
	prefetchRect.left = (std::max)(0, prefetchRect.left - margin);
	prefetchRect.top = (std::max)(0, prefetchRect.top - margin);
	prefetchRect.right = (std::min)(static_cast<int32_t>(imageWidth), prefetchRect.right + margin);
	prefetchRect.bottom = (std::min)(static_cast<int32_t>(imageHeight), prefetchRect.bottom + margin);

	m_visibleKeys.clear();
	m_visibleKeys.reserve(64);
	CalcVisibleKeys(LODLevel, prefetchRect, m_visibleKeys);

	// ── 로드 순서: 뷰포트 중심에서 가까운 것부터
	// GLViewer 의 ReUseCheck_SingleBuffer_WorkOrder(이동 방향 우선)에 대응하며
	// 더 단순하다. 예산이 걸려 일부만 올라갈 때 중앙이 먼저 채워진다.
	const double centerX = (static_cast<double>(viewPixelRect.left) + viewPixelRect.right) * 0.5;
	const double centerY = (static_cast<double>(viewPixelRect.top) + viewPixelRect.bottom) * 0.5;

	std::sort(m_visibleKeys.begin(), m_visibleKeys.end(),
		[this, centerX, centerY](const TileKey& a, const TileKey& b)
		{
			const Core::ShapeType::Rect2i ra = CalcTilePixelRect(a);
			const Core::ShapeType::Rect2i rb = CalcTilePixelRect(b);

			const double ax = (static_cast<double>(ra.left) + ra.right) * 0.5 - centerX;
			const double ay = (static_cast<double>(ra.top) + ra.bottom) * 0.5 - centerY;
			const double bx = (static_cast<double>(rb.left) + rb.right) * 0.5 - centerX;
			const double by = (static_cast<double>(rb.top) + rb.bottom) * 0.5 - centerY;

			return (ax * ax + ay * ay) < (bx * bx + by * by);
		});

	const double uploadStartMs = m_uploadTimer->GetTotalTimeMiliSeconds();

	for (const TileKey& key : m_visibleKeys)
	{
		Tile* tile = pool->Acquire(key, frameID);
		if (!tile)
		{
			// 용량 부족. 부모 fallback 으로 그린다.
			m_hasPendingUploads = true;
		}

		if (tile && tile->state == TileState::None)
		{
			// ── 모션 게이팅
			// 카메라가 움직이는 중이면 새 타일을 만들지 않는다. 드래그 중에
			// 스치는 타일 대부분은 업로드가 끝나기도 전에 화면을 벗어나므로
			// 그 비용이 순수 낭비다. 멈추면 정확한 LOD 로 채운다.
			//
			// ── 시간 예산
			// 프레임당 kUploadBudgetMs 만큼만 생산한다. 넘으면 부모 fallback 으로
			// 그리고 다음 프레임에 이어서 채운다.
			const bool budgetLeft =
				(m_uploadTimer->GetTotalTimeMiliSeconds() - uploadStartMs) < kUploadBudgetMs;

			if (cameraSettled && budgetLeft)
			{
				ProduceTile(pool, tile, imageData, imageWidth, imageStride, imageHeight, channel);
			}
			else
			{
				m_hasPendingUploads = true;
			}
		}

		TileRenderData renderData;
		renderData.targetKey = key;

		if (tile && tile->state == TileState::Resident)
		{
			renderData.tile = tile;
			renderData.u0 = 0.0f; renderData.v0 = 0.0f;
			renderData.u1 = 1.0f; renderData.v1 = 1.0f;
		}
		else
		{
			// 아직 없으면 상위(저해상도) 부모 타일의 해당 영역을 잘라 쓴다.
			// maxLOD 가 전량 상주하므로 프라이밍 이후에는 항상 성공한다.
			TileKey parentKey;
			Tile* parent = FindAvailableParent(key, frameID, parentKey);
			if (!parent)
				continue;

			renderData.tile = parent;

			const uint32_t ratio = 1u << (parentKey.lod - key.lod);
			const float size = 1.0f / static_cast<float>(ratio);

			renderData.u0 = (key.x % ratio) * size;
			renderData.v0 = (key.y % ratio) * size;
			renderData.u1 = renderData.u0 + size;
			renderData.v1 = renderData.v0 + size;
		}

		renderData.tile->lastFrameUsed = frameID;
		m_visibleTiles.push_back(renderData.tile);
		m_renderDataList.push_back(renderData);
	}

	// 직전 프레임에 보였던 타일도 살려둔다(짧은 왕복에서 재업로드 방지).
	// 단 EvictLRU 는 lastFrameUsed == frameID 인 타일을 건너뛰므로,
	// 이 갱신 때문에 현재 프레임 가시 타일이 축출되는 일은 없다.
	for (Tile* oldTile : m_previousVisibleTiles)
	{
		if (oldTile->lastFrameUsed != frameID)
			oldTile->lastFrameUsed = frameID - 1;
	}

	// 오래 안 쓰인 타일 반납. 예전에는 이 호출이 주석 처리되어 풀이 절대
	// 줄지 않았다. maxLOD 풀은 전량 상주가 목적이므로 제외한다.
	for (uint32_t lod = 0; lod < m_maxLOD; ++lod)
	{
		if (m_pools[lod])
			m_pools[lod]->Evict(frameID, kEvictFrameThreshold);
	}
}

Tile* TileManager::FindAvailableParent(const TileKey& childKey, uint64_t frameID, TileKey& outParentKey)
{
	TileKey currentKey = childKey;

	// 현재 LOD보다 숫자가 큰(저해상도) 쪽으로 탐색
	for (uint32_t l = childKey.lod + 1; l <= m_maxLOD; ++l)
	{
		currentKey.lod = static_cast<uint16_t>(l);
		currentKey.x /= 2; // 부모는 자식의 절반 좌표
		currentKey.y /= 2;

		if (!m_pools[l])
			continue;

		Tile* parent = m_pools[l]->Find(currentKey);
		if (parent && parent->state == TileState::Resident)
		{
			parent->lastFrameUsed = frameID; // 사용 중임을 표시해 캐시 유지
			outParentKey = currentKey;
			return parent;
		}
	}
	return nullptr;
}

TileSampler::SampleDesc TileManager::MakeSampleDesc(const Tile* tile, const uint8_t* imageData,
	uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel) const
{
	TileSampler::SampleDesc desc = {};

	const uint32_t lodScale = 1u << tile->key.lod;
	const uint32_t tileSize = m_tileSystemDesc.lods[tile->key.lod].tileSize;

	desc.imageData = imageData;
	desc.imageWidth = imageWidth;
	desc.imageHeight = imageHeight;
	desc.imageStride = imageStride;
	desc.channel = channel;

	desc.tileSize = tileSize;
	desc.lodScale = lodScale;
	desc.srcStartX = tile->key.x * tileSize * lodScale;
	desc.srcStartY = tile->key.y * tileSize * lodScale;

	desc.format = m_tileSystemDesc.format;

	return desc;
}

bool TileManager::ProduceTile(TilePool* pool, Tile* tile, const uint8_t* imageData,
	uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel)
{
	if (!pool || !tile || !imageData)
		return false;

	const TileSampler::SampleDesc desc =
		MakeSampleDesc(tile, imageData, imageWidth, imageStride, imageHeight, channel);

	const size_t needed = TileSampler::RequiredBytes(desc);
	if (m_cpuScratchBuffer.size() < needed)
		m_cpuScratchBuffer.resize(needed);

	if (!TileSampler::Sample(desc, m_cpuScratchBuffer.data()))
		return false;

	UploadStagingToTile(tile, m_cpuScratchBuffer.data(), TileSampler::RowPitch(desc));

	return true;
}

void TileManager::UploadStagingToTile(Tile* tile, const uint8_t* src, uint32_t rowPitch)
{
	if (!tile || !src || !m_contextD3D)
		return;

	if (tile->key.lod >= m_pools.size() || !m_pools[tile->key.lod])
		return;

	ID3D11Texture2D* textureArray = m_pools[tile->key.lod]->GetTextureArray();
	if (!textureArray)
		return;

	const uint32_t subresourceIndex = ::D3D11CalcSubresource(0, tile->arrayIndex, 1);

	m_contextD3D->UpdateSubresource(
		textureArray,
		subresourceIndex,
		nullptr,
		src,
		rowPitch,
		0
	);

	tile->state = TileState::Resident;
}

uint32_t TileManager::SelectLOD(float zoom) const
{
	if (zoom >= 1.0f)
	{
		m_lastLOD = 0;
		return 0;
	}

	const float lodF = std::log2f(1.0f / zoom);
	uint32_t lod = (std::clamp)(
		static_cast<uint32_t>(std::floor(lodF)),
		0u, m_maxLOD);

	// 히스테리시스: 경계에서 LOD 가 떨리는 것을 막는다.
	if (lod > m_lastLOD && lodF < m_lastLOD + 0.2f)
		lod = m_lastLOD;

	m_lastLOD = lod;

	return lod;
}

void TileManager::CalcVisibleKeys(uint32_t LODLevel, const Core::ShapeType::Rect2i& view, std::vector<TileKey>& outKeys) const
{
	if (view.right <= view.left || view.bottom <= view.top)
		return;

	const uint32_t scale = 1u << LODLevel;
	const uint32_t tileSize = m_tileSystemDesc.lods[LODLevel].tileSize;

	// view 는 Camera2D 에서 0 이상으로 clamp 되어 오지만, 음수가 들어오면
	// unsigned 변환으로 거대한 값이 되므로 방어한다.
	const uint32_t left = static_cast<uint32_t>((std::max)(0, view.left));
	const uint32_t top = static_cast<uint32_t>((std::max)(0, view.top));
	const uint32_t right = static_cast<uint32_t>((std::max)(0, view.right));
	const uint32_t bottom = static_cast<uint32_t>((std::max)(0, view.bottom));

	const uint32_t startX = (left / scale) / tileSize;
	const uint32_t endX = ((right - 1) / scale) / tileSize;
	const uint32_t startY = (top / scale) / tileSize;
	const uint32_t endY = ((bottom - 1) / scale) / tileSize;

	for (uint32_t y = startY; y <= endY; y++)
	{
		for (uint32_t x = startX; x <= endX; x++)
		{
			outKeys.push_back({ static_cast<uint16_t>(LODLevel), x, y });
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
	if (lodLevel < m_tileSystemDesc.lods.size())
	{
		tileWidth = m_tileSystemDesc.lods[lodLevel].tileSize;
		tileHeight = m_tileSystemDesc.lods[lodLevel].tileSize;
	}
	else
	{
		tileWidth = kTileSize;
		tileHeight = kTileSize;
	}
}

Core::ShapeType::Rect2i TileManager::CalcTilePixelRect(const TileKey& key) const
{
	uint32_t tileSize = kTileSize;
	if (key.lod < m_tileSystemDesc.lods.size())
		tileSize = m_tileSystemDesc.lods[key.lod].tileSize;

	const uint32_t scale = 1u << key.lod;

	Core::ShapeType::Rect2i rect = {};
	rect.left = static_cast<int32_t>(key.x * tileSize * scale);
	rect.top = static_cast<int32_t>(key.y * tileSize * scale);
	rect.right = rect.left + static_cast<int32_t>(tileSize * scale);
	rect.bottom = rect.top + static_cast<int32_t>(tileSize * scale);

	return rect;
}

ID3D11ShaderResourceView* TileManager::GetPoolSRV(uint32_t lodLevel)
{
	if (m_pools.empty())
		return nullptr;

	if (m_pools.size() <= lodLevel)
		return nullptr;

	return m_pools[lodLevel] ? m_pools[lodLevel]->GetSrvArray() : nullptr;
}

bool TileManager::HasPendingUploads() const
{
	return m_hasPendingUploads;
}
