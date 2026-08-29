#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"
#include "../../../Module/D3D11EngineInterface/IResizeEventListener.h"
#include "../../../Module/Core/ShapeType/Rect2i.h"
#include "../../../Module/Core/ImageType/ImageBase.h"

#include "../Image Tile/TileFormat.h"
#include "../Lut/LutTable.h"

#include <vector>


class Camera2D;
class D3D11RenderContext;
class IRenderContext;
class TileManager;

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct IDXGIKeyedMutex;

namespace GRAPHICS
{
	struct BatchVertex;
}

enum class RenderMode
{
	Single,
	Tiled
};

enum class ImageInputSource
{
	None,
	RawImage,
	Texture,
	SharedTexture
};


class ImageRenderLayer
	: public IRenderLayer
	, public IResizeEventListener
	, public IDeviceEventListener
{
public:
	ImageRenderLayer();
	virtual ~ImageRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IResizeEventListener override
	void OnResize(uint32_t width, uint32_t height) override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;

public:
	void SetCamera2D(Camera2D* camera);
	void SetTileManager(TileManager* tileManager);
	void SetFrameID(uint64_t frameID);

	bool IsImageRenderDirty() const;

	// bitDepth: 채널당 비트 수. 8 또는 16.
	// 16 은 Gray(channel == 1) 만 지원한다. D3D11 에 16bit 3채널 포맷이 없고
	// 4채널(R16G16B16A16)은 타일 샘플러와 픽셀 셰이더가 아직 다루지 못한다.
	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel, uint32_t bitDepth = 8);
	bool UpdateTexture(ID3D11Texture2D* texture, uint32_t& width, uint32_t& height);
	bool UpdateSharedTexture(HANDLE sharedHandle, uint32_t& width, uint32_t& height);

	// Single 모드에서만 mip chain을 생성한다. Tiled 모드는 TileManager의
	// LOD를 사용하므로 이 설정과 관계없이 single mip chain을 만들지 않는다.
	// 기본값은 false다.
	bool SetMipMapGenerationEnabled(bool enable);
	bool IsMipMapGenerationEnabled() const;

	// Attach 된 원본 포인터 참조를 끊는다.
	// 호출자가 그 버퍼를 해제하기 전에 반드시 거쳐야 하는 경로다.
	void DetachImage();

	RenderMode GetRenderMode() const;
	const Core::ImageType::ImageBase* GetImage() const;

	// ── LUT ──────────────────────────────────────────────────────────
	//
	// 1채널(Gray) 이미지에만 적용된다. 컬러 이미지가 붙어 있으면
	// SupportsLut() 가 false 를 주고 켜도 아무 일도 일어나지 않는다.
	//
	// 켜는 순간과 프리셋을 바꾸는 순간에만 테이블을 다시 굽는다.
	// 토글 자체는 셰이더를 바꿔 끼우는 것뿐이라 재업로드가 없다.
	bool SupportsLut() const;
	void SetLutEnabled(bool enable);
	bool IsLutEnabled() const;
	void SetLutPreset(LutPreset preset);
	LutPreset GetLutPreset() const;

	// 어떤 경로로 이미지가 들어왔는지. 저장할 때 CPU 버퍼를 쓸지
	// GPU 텍스처를 읽어 내릴지 가르는 데 쓴다.
	ImageInputSource GetInputSource() const;

	// 텍스처/공유텍스처 입력일 때 실제 픽셀이 들어 있는 텍스처.
	// RawImage 입력에서는 원본이 CPU 에 있으므로 이걸 쓸 이유가 없다.
	ID3D11Texture2D* GetSingleTexture() const;
private:
	bool CreateDeviceResources();
	void ReleaseDeviceResources();

	bool CreateShaders();
	bool CreateConstantBuffer();
	bool CreateGeometry(uint32_t tileWidth, uint32_t tileHeight);
	bool CreateSampler();
	bool CreateRasterizerState();
	bool CreateTileDynamicBuffer(uint32_t maxTileCount);
	// 정점 개수로 직접 만든다. 용량이 부족할 때 UpdateVertexBuffer 가 호출한다.
	bool CreateTileVertexBuffer(uint32_t vertexCount);
	// needsComputeUpload: 컴퓨트 셰이더가 UAV 로 mip 0 에 써야 하는가.
	//   BIND_UNORDERED_ACCESS 가 붙으면 드라이버가 텍스처 무손실 압축을 끄는
	//   경우가 많아 샘플링 대역폭이 나빠지므로, 실제로 CS 를 쓸 때만 켠다.
	//   (3채널 BGR 확장만 해당. 1/4채널은 UpdateSubresource 직행)
	// 디바이스 복구 후 원본 CPU 버퍼로 화면을 되살린다.
	void RestoreImageAfterDeviceLoss();

	// 현재 Attach 된 이미지의 채널당 비트 수. ImageBase 는 PixelType 만
	// 노출하므로 거기서 되돌린다. 이미지가 없으면 8 을 준다.
	uint32_t GetAttachedBitDepth() const;

	bool CreateSingleBuffer(uint32_t width, uint32_t height, DXGI_FORMAT format,
		bool needsComputeUpload, bool generateMipMaps);
	bool RecreateSingleBufferForMipSetting(bool generateMipMaps);
	void GenerateSingleMips();
	void ReleaseUnusedModeResources(RenderMode activeMode);
	bool CreateRawUploadBuffer(uint32_t maxByteSize);
	bool OpenSharedResource(HANDLE sharedHandle);
	void UpdateImageState(ImageInputSource source, uint32_t width, uint32_t height, RenderMode mode, uint32_t channel);

	bool CheckViewChanged();

	void UploadSingleImage_GPU(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool RenderTiled();
	bool RenderSingle();
	void SetCommonShaderStates();

	// LUT
	bool EnsureLutTexture(uint32_t entryCount);
	bool RebuildLut();
	bool IsLutActive();
	void UpdateVertexBuffer(const std::vector<GRAPHICS::BatchVertex>& vertices);


private:
	// Context
	D3D11RenderContext* m_context = nullptr;

	// D3D
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_contextD3D = nullptr;

	// Pipeline
	ID3D11VertexShader* m_vs = nullptr;
	ID3D11PixelShader* m_ps = nullptr;      // 4채널(BGRA)
	ID3D11PixelShader* m_grayPS = nullptr;  // 1채널(R8/R16) — .r 을 3채널로 복제
	ID3D11PixelShader* m_grayLutPS = nullptr; // 1채널 + LUT
	ID3D11PixelShader* m_wirePS = nullptr;
	ID3D11InputLayout* m_inputLayout = nullptr;
	ID3D11ComputeShader* m_singleTextureCS = nullptr;

	// Geometry
	uint32_t m_tileVertexCount = 0;
	uint32_t m_maxTileVertexCount = 0;
	ID3D11Buffer* m_tileVertexBuffer = nullptr;

	ID3D11Buffer* m_indexBuffer = nullptr;
	ID3D11Buffer* m_constantBuffer = nullptr;
	ID3D11Buffer* m_wireColorBuffer = nullptr;

	// Texture
	// Tiled 는 POINT 고정. 타일 텍스처는 MipLevels=1 이고 배열 슬라이스 경계에서
	// CLAMP 되므로 LINEAR 축소를 걸면 타일마다 테두리 텍셀이 번져 이음새가 보인다.
	ID3D11SamplerState* m_samplerPoint = nullptr;
	// Single 은 밉 체인이 있으므로 축소는 LINEAR+MIP, 확대는 POINT(픽셀 경계 보존).
	// Single 축소는 항상 LINEAR + 밉 보간. 확대만 아래 두 개로 갈린다.
	ID3D11SamplerState* m_samplerSingleLinear = nullptr;    // 확대 LINEAR
	ID3D11SamplerState* m_samplerSingleMagPoint = nullptr;  // 확대 POINT

	// ── LUT ──────────────────────────────────────────────────────────
	// N x 1 RGBA8. 자동 대비와 컬러맵이 함께 구워져 있다.
	ID3D11Texture2D* m_lutTexture = nullptr;
	ID3D11ShaderResourceView* m_lutSRV = nullptr;
	ID3D11SamplerState* m_lutSampler = nullptr;
	uint32_t m_lutEntryCount = 0;

	bool m_lutEnabled = false;
	LutPreset m_lutPreset = LutPreset::Grayscale;
	// 테이블을 다시 구워야 하는가(이미지나 프리셋이 바뀜).
	bool m_lutDirty = true;

	// 현재 POINT 확대를 쓰고 있는가(히스테리시스 상태).
	bool m_magPointActive = false;

	// Rasterizer
	ID3D11RasterizerState* m_rasterizerSolid = nullptr;
	ID3D11RasterizerState* m_rasterizerWireFrame = nullptr;

	// TileManager
	uint64_t m_frameID = 0;
	TileManager* m_tileManager = nullptr;

	// State
	// 디버그용 타일 경계 표시. 매 프레임 드로우가 2배가 되므로 기본은 끔.
	bool m_renderWireFrame = false;
	uint32_t m_texWidth = 0;
	uint32_t m_texHeight = 0;
	DXGI_FORMAT m_texFormat = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
	bool m_initialized = false;

	// 디바이스 로스트 ~ 복구 사이에는 false. 이 구간에 Render 가 들어오면
	// 죽은 디바이스의 리소스를 참조하게 되므로 그리지 않는다.
	bool m_deviceResourcesReady = true;

	ImageInputSource m_inputSource = ImageInputSource::None;
	uint32_t m_inputChannel = 0;

	// Camera
	Camera2D* m_camera = nullptr;

	// Image
	Core::ImageType::ImageBase* m_image = nullptr;

	std::vector<GRAPHICS::BatchVertex> m_renderVertices;
	Core::ShapeType::Rect2i m_prevViewRect = {};
	float m_prevZoom = 0.0f;
	uint32_t m_prevTileCount = 0;

	RenderMode m_currentMode = RenderMode::Tiled;

	uint32_t m_singleTextureWidth = 0;
	uint32_t m_singleTextureHeight = 0;
	DXGI_FORMAT m_singleTextureFormat = DXGI_FORMAT_UNKNOWN;
	bool m_mipMapGenerationEnabled = false;
	bool m_singleTextureHasMipMaps = false;

	// Single 모드 VRAM 예산을 나눌 동시 뷰어 개수.
	// 검사 UI 에서 뷰어를 여러 개 띄우면 전량 상주 비용이 그 배수로 곱해진다.
	uint32_t m_concurrentViewCount = 1;
	ID3D11Texture2D* m_singleTexture = nullptr;
	ID3D11ShaderResourceView* m_singleSRV = nullptr;
	ID3D11UnorderedAccessView* m_singleUAV = nullptr;
	uint32_t m_maxByteSize = 0;
	ID3D11Buffer* m_rawUploadBuffer = nullptr;
	ID3D11ShaderResourceView* m_rawUploadSRV = nullptr;
	ID3D11Buffer* m_singleConvertCB = nullptr;

	std::vector<GRAPHICS::BatchVertex> m_singleVertices;

	// Shared Resource
	HANDLE m_sharedHandle = nullptr;
	ID3D11Texture2D* m_sharedTexture = nullptr;
	IDXGIKeyedMutex* m_sharedKeyedMutex = nullptr;
};
