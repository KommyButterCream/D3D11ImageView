#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include "../../../Module/Core/ShapeType/Rect2i.h"

#include "Tile.h"


struct TileKey;
struct Tile;
class TilePool;

struct TileLODDesc
{
	uint32_t tileSize = 512;
	uint32_t capacity = 0;
};

struct TileSystemDesc
{
	uint32_t maxLOD = 0;
	std::vector<TileLODDesc> lods;
};


//TileSystemDesc desc;
//desc.maxLOD = 4;
//desc.lods =
//{
//	{512, 1024}, // LOD 0 (����, ���� ����)
//	{512, 768},  // LOD 1
//	{512, 512},  // LOD 2
//	{512, 256},  // LOD 3
//	{512, 128},  // LOD 4 (���� �����)
//};


struct CachedRegion
{
	uint32_t lod = 0xFFFFFFFF; // ���� �ε�� LOD
	uint32_t startX = 0;       // ���� ���� ���� �ȼ� X
	uint32_t startY = 0;       // ���� ���� ���� �ȼ� Y
	uint32_t size = 0;      // 8k ����
	bool isValid = false;

	// ���� ��û�� Ÿ���� �� ���� �ȿ� ������ ���ԵǴ��� Ȯ��
	bool Contains(uint32_t targetX, uint32_t targetY, uint32_t targetSize) const
	{
		if (!isValid) return false;
		return (targetX >= startX && targetY >= startY &&
			(targetX + targetSize) <= (startX + size) &&
			(targetY + targetSize) <= (startY + size));
	}
};

struct TileRenderData
{
	Tile* tile = nullptr;        // ����� �ؽ�ó�� �ִ� Ÿ�� (�θ��� ���� ����)
	TileKey targetKey = {}; // ���� ȭ����� ��ġ ������ �Ǵ� Key
	float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // �ؽ�ó ���ø� ����
};

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11ComputeShader;
struct ID3D11Buffer;

class TileManager
{
public:
	TileManager() = default;
	~TileManager();

public:
	void Initialize(ID3D11Device* device, ID3D11DeviceContext* contextD3D, const TileSystemDesc& desc);
	bool InitializeGPUResources(ID3D11Device* device);
	void ReleaseGPUResources();

	void ClearPools();

	void SetUploadMode(UploadMode mode);

	void UpdateVisibleTiles(const Core::ShapeType::Rect2i& viewPixelRect, float zoom, uint64_t frameID,
		const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel);

	const std::vector<Tile*>& GetVisibleTiles() const;
	const std::vector<TileRenderData>& GetRenderDataList() const;

	void GetTileSize(uint32_t lodLevel, uint32_t& tileWidth, uint32_t& tileHeight) const;

	Core::ShapeType::Rect2i CalcTilePixelRect(const TileKey& key) const;

	ID3D11ShaderResourceView* GetPoolSRV(uint32_t lodLevel);
	bool HasPendingUploads() const;


private:
	uint32_t SelectLOD(float zoom) const;
	void CalcVisibleKeys(uint32_t LODLevel, const Core::ShapeType::Rect2i& view, std::vector<TileKey>& outKeys) const;
	Tile* FindAvailableParent(const TileKey& childKey, uint64_t frameID, TileKey& outParentKey);

	void UploadTileData_CPU(TilePool* pool, Tile* tile, const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel);
	void UploadTileData_GPU(TilePool* pool, Tile* tile, const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel);

private:
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_contextD3D = nullptr;

	TileSystemDesc m_tileSystemDesc = {};
	uint32_t m_maxLOD = 6;
	mutable uint32_t m_lastLOD = 0;

	std::vector<std::unique_ptr<TilePool>> m_pools;
	std::vector<Tile*> m_visibleTiles;
	std::vector<Tile*> m_previousVisibleTiles;

	UploadMode m_uploadMode = UploadMode::Hybrid; // Hybrid, OnlyCPU, OnlyGPU

	std::vector<uint8_t> m_cpuScratchBuffer;
	std::vector<TileRenderData> m_renderDataList;

	// GPU ���� ���ε带 ���� ���ҽ�
	static constexpr uint32_t m_gpuUploadTextureSize = 4096;
	CachedRegion m_currentGpuCache = {};
	ID3D11Buffer* m_rawUploadBuffer = nullptr;
	ID3D11ShaderResourceView* m_rawUploadSRV = nullptr;
	ID3D11ComputeShader* m_rawUploadtileCS = nullptr;
	ID3D11Buffer* m_csConstantBuffer = nullptr;

	bool m_hasPendingUploads = false;
};




