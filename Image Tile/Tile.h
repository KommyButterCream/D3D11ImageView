#pragma once

#include <stdint.h>

#include <list>
#include <functional>
#include <cassert>

struct TileKey
{
	uint16_t lod = 0;
	uint32_t x = 0;
	uint32_t y = 0;

	bool operator==(const TileKey& rhs) const
	{
		return lod == rhs.lod && x == rhs.x && y == rhs.y;
	}
};

static_assert(std::is_trivially_copyable_v<TileKey>, "TileKey must be trivially copyable");

namespace std
{
	template<>
	struct hash<TileKey>
	{
		size_t operator()(const TileKey& key) const noexcept
		{
			// 64bit packet(LOD | X | Y)
			return
				(static_cast<size_t>(key.lod) << 48) ^
				(static_cast<size_t>(key.x) << 24) ^
				(static_cast<size_t>(key.y));

		}
	};
}

struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

enum class TileState
{
	None,       // 슬롯만 할당됨. 데이터 없음
	Resident,   // 텍스처에 데이터가 올라감. 렌더 및 부모 fallback 가능
};

struct Tile
{
	TileKey key{};
	uint32_t arrayIndex = 0;

	TileState state = TileState::None;

	std::list<Tile*>::iterator usedIter;

	uint64_t lastFrameUsed = 0; // LRU
};
