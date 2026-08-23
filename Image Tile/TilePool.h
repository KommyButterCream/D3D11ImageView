#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <list>
#include <unordered_map>
#include <dxgiformat.h>

#include "Tile.h"

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;


class TilePool
{
public:
	TilePool(ID3D11Device* device, uint32_t tileSize, uint32_t capacity, DXGI_FORMAT format);
	~TilePool();

public:
	ID3D11Texture2D* GetTextureArray();
	ID3D11ShaderResourceView* GetSrvArray();

	DXGI_FORMAT GetFormat() const { return m_format; }
	uint32_t GetCapacity() const { return static_cast<uint32_t>(m_storage.size()); }

	Tile* Acquire(const TileKey& key, uint64_t frameID);
	void Evict(uint64_t frameID, uint64_t frameThreshold);
	Tile* Find(const TileKey& key);
	void Clear();

private:
	Tile* AllocateNew(const TileKey& key, uint64_t frameID);

	// 현재 프레임에 쓰이는 타일을 빼앗지 않도록 frameID 를 받는다.
	Tile* EvictLRU(uint64_t frameID);

private:
	ID3D11Device* m_device = nullptr;

	DXGI_FORMAT m_format = DXGI_FORMAT_B8G8R8A8_UNORM;

	std::vector<std::unique_ptr<Tile>> m_storage;

	std::list<Tile*> m_freeList;

	// front -> 가장 최근 사용 (MRU)
	// back -> 가장 오래 안씀 (LRU)
	std::list<Tile*> m_usedList;

	std::unordered_map<TileKey, Tile*> m_lookup;

	ID3D11Texture2D* m_textureArray = nullptr;
	ID3D11ShaderResourceView* m_srvArray = nullptr;
};
