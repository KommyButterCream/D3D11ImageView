#include "pch.h"
#include "TilePool.h"

TilePool::TilePool(ID3D11Device* device, uint32_t tileSize, uint32_t capacity, UploadMode mode)
	: m_device(device)
	, m_uploadMode(mode)
{
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = tileSize;
	textureDesc.Height = tileSize;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = capacity;
	textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (m_uploadMode == UploadMode::OnlyCPU)
	{
		// Map/Unmap에 최적화된 설정
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}
	else if (m_uploadMode == UploadMode::Hybrid)
	{
		// Compute Shader 가속에 최적화된 설정
		
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	}

	HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &m_textureArray);
	if (FAILED(hr))
		return;

	hr = device->CreateShaderResourceView(m_textureArray, nullptr, &m_srvArray);
	if (FAILED(hr))
		return;

	if (m_uploadMode == UploadMode::Hybrid)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = capacity;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.MipSlice = 0;

		hr = m_device->CreateUnorderedAccessView(m_textureArray, &uavDesc, &m_uavArray);
		if (FAILED(hr))
			return;
	}

	m_storage.reserve(capacity);
	for (uint32_t i = 0; i < capacity; i++)
	{
		auto tile = std::make_unique<Tile>();

		tile->arrayIndex = i;
		tile->state = TileState::None;

		m_freeList.push_back(tile.get());
		m_storage.push_back(std::move(tile));
	}
}

TilePool::~TilePool()
{
	SafeRelease(m_srvArray);
	SafeRelease(m_textureArray);
	SafeRelease(m_uavArray);

	m_storage.clear();
}

ID3D11Texture2D* TilePool::GetTextureArray()
{
	return m_textureArray;
}

ID3D11ShaderResourceView* TilePool::GetSrvArray()
{
	return m_srvArray;
}

ID3D11UnorderedAccessView* TilePool::GetUavArray()
{
	return m_uavArray;
}

Tile* TilePool::Acquire(const TileKey& key, uint64_t frameID)
{
	auto it = m_lookup.find(key);
	if (it != m_lookup.end())
	{
		Tile* tile = it->second;
		tile->lastFrameUsed = frameID;

		// LRU 갱신
		// 맨 앞으로 옮겨야한다.
		m_usedList.erase(tile->usedIter);
		m_usedList.push_front(tile);
		tile->usedIter = m_usedList.begin();

		return tile;
	}

	return AllocateNew(key, frameID);
}

void TilePool::Evict(uint64_t frameID, uint64_t frameThreshold)
{
	auto it = m_usedList.rbegin();
	while (it != m_usedList.rend())
	{
		Tile* tile = *it;
		if (frameID - tile->lastFrameUsed < frameThreshold)
			break;

		auto eraseIt = std::next(it).base();
		m_lookup.erase(tile->key);
		m_usedList.erase(eraseIt);
		m_freeList.push_back(tile);

		it = m_usedList.rbegin();
	}
}

Tile* TilePool::Find(const TileKey& key)
{
	auto it = m_lookup.find(key);
	if (it != m_lookup.end())
	{
		return it->second;
	}
	return nullptr;
}

void TilePool::Clear()
{
	m_lookup.clear();
	m_usedList.clear();
	m_freeList.clear();

	for (auto& tilePtr : m_storage)
	{
		Tile* tile = tilePtr.get();
		tile->key = TileKey{};
		tile->state = TileState::None;
		tile->lastFrameUsed = 0;
		// arrayIndex는 고정값이므로 초기화하지 않습니다.

		m_freeList.push_back(tile);
	}
}

Tile* TilePool::AllocateNew(const TileKey& key, uint64_t frameID)
{
	Tile* tile = nullptr;

	if (!m_freeList.empty())
	{
		tile = m_freeList.front();
		m_freeList.pop_front();
	}
	else
	{
		tile = EvictLRU();
	}

	if (!tile)
	{
		return nullptr;
	}

	tile->key = key;
	tile->lastFrameUsed = frameID;
	tile->state = TileState::None;

	m_usedList.push_front(tile);
	tile->usedIter = m_usedList.begin();
	m_lookup[key] = tile;

	return tile;
}

Tile* TilePool::EvictLRU()
{
	if (m_usedList.empty())
	{
		return nullptr; // 뺏어올 타일이 없음
	}

	Tile* tile = m_usedList.back();

	m_lookup.erase(tile->key);
	m_usedList.pop_back();

	return tile;
}
