#pragma once

#include <stdint.h>

#include <list>
#include <functional>
#include <cassert>

enum class UploadMode
{
	Hybrid,
	OnlyCPU,
};

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
	None,       // 초기 상태
	Ready,      // 데이터가 업로드되어 즉시 사용 가능
	Inactive,   // 가시 범위 밖
	Active      // 현재 렌더링 중
};

struct Tile
{
	TileKey key{};
	uint32_t arrayIndex = 0;

	TileState state = TileState::None;

	std::list<Tile*>::iterator usedIter;

	uint64_t lastFrameUsed = 0; // LRU
};