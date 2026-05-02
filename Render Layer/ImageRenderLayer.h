#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"
#include "../../../Module/D3D11EngineInterface/IResizeEventListener.h"
#include "../../../Module/Core/ShapeType/Rect2i.h"
#include "../../../Module/Core/ImageType/ImageBase.h"

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

namespace GRAPHICS
{
	struct BatchVertex;
}

enum class RenderMode
{
	Single,
	Tiled
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

	bool UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool UpdateTexture(ID3D11Texture2D* texture, uint32_t& width, uint32_t& height);
	bool UpdateSharedTexture(HANDLE sharedHandle, uint32_t& width, uint32_t& height);

	RenderMode GetRenderMode() const;
	const Core::ImageType::ImageBase* GetImage() const;
private:
	bool CreateDeviceResources();
	void ReleaseDeviceResources();

	bool CreateShaders();
	bool CreateConstantBuffer();
	bool CreateGeometry(uint32_t tileWidth, uint32_t tileHeight);
	bool CreateSampler();
	bool CreateRasterizerState();
	bool CreateTileDynamicBuffer(uint32_t maxTileCount);
	bool CreateSingleBuffer(uint32_t width, uint32_t height);
	bool CreateRawUploadBuffer(uint32_t maxByteSize);
	bool OpenSharedResource(HANDLE sharedHandle);

	bool CheckViewChanged();

	void UploadSingleImage_GPU(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel);
	bool RenderTiled();
	bool RenderSingle();
	void SetCommonShaderStates();
	void UpdateVertexBuffer(const std::vector<GRAPHICS::BatchVertex>& vertices);


private:
	// Context
	D3D11RenderContext* m_context = nullptr;

	// D3D
	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_contextD3D = nullptr;

	// Pipeline
	ID3D11VertexShader* m_vs = nullptr;
	ID3D11PixelShader* m_ps = nullptr;
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
	ID3D11SamplerState* m_sampler = nullptr;

	// Rasterizer
	ID3D11RasterizerState* m_rasterizerSolid = nullptr;
	ID3D11RasterizerState* m_rasterizerWireFrame = nullptr;

	// TileManager
	uint64_t m_frameID = 0;
	TileManager* m_tileManager = nullptr;

	// State
	bool m_renderWireFrame = true;
	uint32_t m_texWidth = 0;
	uint32_t m_texHeight = 0;
	DXGI_FORMAT m_texFormat = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
	bool m_initialized = false;

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
};









