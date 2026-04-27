#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <list>
#include <unordered_map>

#include "Tile.h"

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;


class TilePool
{
public:
	TilePool(ID3D11Device* device, uint32_t tileSize, uint32_t capacity, UploadMode mode);
	~TilePool();

public:
	ID3D11Texture2D* GetTextureArray();
	ID3D11ShaderResourceView* GetSrvArray();
	ID3D11UnorderedAccessView* GetUavArray();

	Tile* Acquire(const TileKey& key, uint64_t frameID);
	void Evict(uint64_t frameID, uint64_t frameThreshold);
	Tile* Find(const TileKey& key);
	void Clear();

private:
	Tile* AllocateNew(const TileKey& key, uint64_t frameID);
	Tile* EvictLRU();

private:
	UploadMode m_uploadMode = UploadMode::OnlyCPU;

	ID3D11Device* m_device = nullptr;

	std::vector<std::unique_ptr<Tile>> m_storage;

	std::list<Tile*> m_freeList;

	// front -> 가장 최근 사용 (MRU)
	// back -> 가장 오래 안씀 (LRU)
	std::list<Tile*> m_usedList;

	std::unordered_map<TileKey, Tile*> m_lookup;

	ID3D11Texture2D* m_textureArray = nullptr;
	ID3D11ShaderResourceView* m_srvArray = nullptr;
	ID3D11UnorderedAccessView* m_uavArray = nullptr;
};

