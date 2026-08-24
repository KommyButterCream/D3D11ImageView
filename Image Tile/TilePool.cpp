#include "pch.h"
#include "TilePool.h"

TilePool::TilePool(ID3D11Device* device, uint32_t tileSize, uint32_t capacity, DXGI_FORMAT format)
	: m_device(device)
	, m_format(format)
{
	if (capacity == 0)
		return;

	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = tileSize;
	textureDesc.Height = tileSize;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = capacity;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// UpdateSubresource 로만 채운다. 컴퓨트 셰이더 경로를 폐지했으므로
	// UAV 바인딩이 필요 없고, 그 덕에 R8/R16 의 typed UAV store 지원 여부를
	// 신경 쓰지 않아도 된다.
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;

	HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &m_textureArray);
	if (FAILED(hr))
		return;

	hr = device->CreateShaderResourceView(m_textureArray, nullptr, &m_srvArray);
	if (FAILED(hr))
		return;

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
		tile = EvictLRU(frameID);
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

Tile* TilePool::EvictLRU(uint64_t frameID)
{
	// 이번 프레임에 이미 사용된 타일은 렌더 리스트에 들어가 있을 수 있으므로
	// 절대 빼앗지 않는다. 예전 구현은 m_usedList.back() 을 무조건 가져갔고,
	// TileManager 가 이전 프레임 타일들의 lastFrameUsed 를 갱신하면서
	// usedList 순서와 어긋나 현재 프레임 가시 타일이 축출될 수 있었다.
	for (auto it = m_usedList.rbegin(); it != m_usedList.rend(); ++it)
	{
		Tile* tile = *it;

		if (tile->lastFrameUsed == frameID)
			continue;

		auto eraseIt = std::next(it).base();
		m_lookup.erase(tile->key);
		m_usedList.erase(eraseIt);

		return tile;
	}

	// 전부 이번 프레임에 쓰이는 중이라면 용량이 부족한 것이다.
	// (용량 공식이 맞다면 여기 도달하지 않는다.)
	return nullptr;
}
