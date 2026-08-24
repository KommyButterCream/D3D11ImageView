#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <dxgiformat.h>

#include "../../../Module/Core/ShapeType/Rect2i.h"

#include "Tile.h"
#include "TileFormat.h"
#include "TileSampler.h"


struct TileKey;
struct Tile;
class TilePool;

struct TileLODDesc
{
	uint32_t tileSize = 512;
	uint32_t capacity = 0;
};

// 이미지 크기와 뷰포트로부터 산출되는 타일 시스템 구성.
//
// 예전에는 뷰어 초기화 시점에 maxLOD=4, 용량 {200,100,50,30,20} 을 하드코딩했다.
// LOD4 타일이 커버하는 원본은 512*16 = 8192px 뿐이어서 30만px 이미지를 fit 줌으로
// 보면 37x37 = 1369 장이 필요한데 풀 용량은 20 칸이라 영구 스래싱이 발생했다.
// 이제 이미지마다 계산한다.
struct TileSystemDesc
{
	uint32_t maxLOD = 0;
	std::vector<TileLODDesc> lods;

	DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;

	uint32_t imageWidth = 0;
	uint32_t imageHeight = 0;
	uint32_t sourceChannel = 0;

	// 참고용 산출값
	uint32_t workingSetTiles = 0;
};


struct TileRenderData
{
	Tile* tile = nullptr;   // 실제로 샘플링할 타일. 부모 fallback 일 수 있다
	TileKey targetKey = {}; // 화면상 그려질 위치/LOD
	float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // 텍스처 샘플링 좌표
};

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

class HighResolutionTimer;

class TileManager
{
public:
	TileManager();
	~TileManager();

public:
	void Initialize(ID3D11Device* device, ID3D11DeviceContext* contextD3D);

	// ── 디바이스 로스트 대응 ─────────────────────────────────
	//
	// TilePool 이 들고 있는 ID3D11Texture2D 는 디바이스 종속 리소스다.
	// 디바이스가 재생성되면 전부 버리고 다시 만들어야 한다.
	//
	// TileManager 는 IDeviceEventListener 를 직접 구현하지 않는다.
	// ImageRenderLayer 가 자기 OnDeviceLost/OnDeviceRestored 안에서 호출해
	// 순서(레이어 리소스 해제 -> 풀 해제, 디바이스 갱신 -> 레이어 재생성)를
	// 확실히 통제한다.
	void OnDeviceLost();
	void OnDeviceRestored(ID3D11Device* device, ID3D11DeviceContext* contextD3D);

	// 마지막 Configure 인자. 디바이스 복구 후 같은 구성으로 다시 만들 때 쓴다.
	bool HasLastConfig() const { return m_lastConfigValid; }
	bool ReapplyLastConfig();

	// 이미지가 바뀔 때마다 호출한다. maxLOD / 용량 / 포맷이 모두 이미지 의존이므로
	// 여기서 풀을 재구성한다.
	bool Configure(uint32_t imageWidth, uint32_t imageHeight,
		uint32_t channel, uint32_t bitDepth,
		uint32_t viewWidth, uint32_t viewHeight);

	// 뷰포트가 크게 바뀌면 작업세트가 달라지므로 재구성이 필요한지 알려준다.
	bool NeedsReconfigure(uint32_t viewWidth, uint32_t viewHeight) const;

	void ReleasePools();
	void ClearPools();

	// 가장 거친 LOD 전체를 예산 무시하고 한 번에 채운다.
	// 이게 끝나면 FindAvailableParent 가 항상 성공하므로 화면에 구멍이 생기지 않는다.
	// (GLViewer 가 캐시 준비 전까지 SwapBuffers 를 생략하는 것과 같은 효과이면서,
	//  카메라 행렬을 낡은 것으로 되돌리지 않으므로 위치 밀림이 없다.)
	void PrimeCoarsestLevel(const uint8_t* imageData, uint32_t imageWidth,
		uint32_t imageStride, uint32_t imageHeight, uint32_t channel);

	bool IsPrimed() const { return m_primed; }
	bool IsConfigured() const { return !m_pools.empty(); }

	void UpdateVisibleTiles(const Core::ShapeType::Rect2i& viewPixelRect, float zoom, uint64_t frameID,
		const uint8_t* imageData, uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel,
		bool cameraSettled);

	const std::vector<Tile*>& GetVisibleTiles() const;
	const std::vector<TileRenderData>& GetRenderDataList() const;

	void GetTileSize(uint32_t lodLevel, uint32_t& tileWidth, uint32_t& tileHeight) const;

	Core::ShapeType::Rect2i CalcTilePixelRect(const TileKey& key) const;

	ID3D11ShaderResourceView* GetPoolSRV(uint32_t lodLevel);
	bool HasPendingUploads() const;

	DXGI_FORMAT GetFormat() const { return m_tileSystemDesc.format; }
	const TileSystemDesc& GetDesc() const { return m_tileSystemDesc; }

	// 진단용
	uint32_t GetCurrentLOD() const { return m_lastLOD; }
	uint32_t GetMaxLOD() const { return m_maxLOD; }

private:
	uint32_t SelectLOD(float zoom) const;
	void CalcVisibleKeys(uint32_t LODLevel, const Core::ShapeType::Rect2i& view, std::vector<TileKey>& outKeys) const;
	Tile* FindAvailableParent(const TileKey& childKey, uint64_t frameID, TileKey& outParentKey);

	// 타일 하나의 샘플링 파라미터를 만든다.
	TileSampler::SampleDesc MakeSampleDesc(const Tile* tile, const uint8_t* imageData,
		uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel) const;

	// 스크래치 버퍼에 타일을 만든 뒤 텍스처로 올린다. 렌더 스레드 전용.
	bool ProduceTile(TilePool* pool, Tile* tile, const uint8_t* imageData,
		uint32_t imageWidth, uint32_t imageStride, uint32_t imageHeight, uint32_t channel);

	void UploadStagingToTile(Tile* tile, const uint8_t* src, uint32_t rowPitch);

private:
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_contextD3D = nullptr;

	TileSystemDesc m_tileSystemDesc = {};
	uint32_t m_maxLOD = 0;
	mutable uint32_t m_lastLOD = 0;

	uint32_t m_configuredViewWidth = 0;
	uint32_t m_configuredViewHeight = 0;

	// 디바이스 복구 시 동일 구성으로 재생성하기 위한 마지막 Configure 인자.
	struct LastConfig
	{
		uint32_t imageWidth = 0;
		uint32_t imageHeight = 0;
		uint32_t channel = 0;
		uint32_t bitDepth = 8;
		uint32_t viewWidth = 0;
		uint32_t viewHeight = 0;
	};
	LastConfig m_lastConfig = {};
	bool m_lastConfigValid = false;

	std::vector<std::unique_ptr<TilePool>> m_pools;
	std::vector<Tile*> m_visibleTiles;
	std::vector<Tile*> m_previousVisibleTiles;

	std::vector<uint8_t> m_cpuScratchBuffer;
	std::vector<TileRenderData> m_renderDataList;
	std::vector<TileKey> m_visibleKeys;

	std::unique_ptr<HighResolutionTimer> m_uploadTimer;

	bool m_hasPendingUploads = false;
	bool m_primed = false;
};
