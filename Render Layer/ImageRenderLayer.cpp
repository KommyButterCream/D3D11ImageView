#include "pch.h"
#include "ImageRenderLayer.h"

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderContext.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11EngineInterface/IRenderStructures.h"

#include "../../../Module/Core/ImageType/ImageBase.h"

#include "../Image Tile/Tile.h"
#include "../Image Tile/TileManager.h"

static const uint16_t kQuadIndices[] = { 0,1,2, 0,2,3 };

ImageRenderLayer::ImageRenderLayer()
{

}

ImageRenderLayer::~ImageRenderLayer()
{
	Shutdown();
}

bool ImageRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
		return false;

	m_context = dynamic_cast<D3D11RenderContext*>(context);
	if (!m_context)
		return false;

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return false;

	m_device = engine->GetD3DDevice();
	m_contextD3D = engine->GetD3DDeviceContext();
	ID3D11RenderTargetView* rtv = m_context->GetD3DRenderTargetView();

	if (!m_device || !m_contextD3D || !rtv)
		return false;

	m_image = new Core::ImageType::ImageBase;
	if (!m_image)
		return false;

	if (!CreateDeviceResources())
		return false;

	m_context->AddResizeListener(this);
	m_context->AddDeviceListener(this);

	m_initialized = true;

	return true;
}

void ImageRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveResizeListener(this);
		m_context->RemoveDeviceListener(this);
	}

	ReleaseDeviceResources();

	m_sharedHandle = nullptr;
	m_tileManager = nullptr;
	m_initialized = false;
}

bool ImageRenderLayer::Prepare()
{
	return true;
}

bool ImageRenderLayer::Render()
{
	if (!m_initialized)
		return true;

	if (!m_context)
		return false;

	if (GetRenderMode() == RenderMode::Tiled)
	{
		if (!m_image->IsEmpty())
		{
			const Core::ShapeType::Rect2i rect = m_camera->GetViewImageRect();

			m_tileManager->UpdateVisibleTiles(rect, m_camera->GetZoom(), m_frameID,
				m_image->ImageBuffer(), m_image->Width(), m_image->Stride(), m_image->Height(), m_image->Channel());
		}
	}

	m_context->GetEngine();

	ID3D11RenderTargetView* rtv = m_context->GetD3DRenderTargetView();

	if (!rtv)
		return false;

	// OM
	m_contextD3D->OMSetRenderTargets(1, &rtv, nullptr);

	// Viewport
	uint32_t viewWidth = m_context->GetWidth();
	uint32_t viewHeight = m_context->GetHeight();

	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<FLOAT>(viewWidth);
	vp.Height = static_cast<FLOAT>(viewHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	m_contextD3D->RSSetViewports(1, &vp);

	ViewParams viewParams;
	m_camera->GetViewParams(viewParams);
	D3D11_MAPPED_SUBRESOURCE mappedCB{};
	if (SUCCEEDED(m_contextD3D->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
	{
		memcpy(mappedCB.pData, &viewParams, sizeof(ViewParams));
		m_contextD3D->Unmap(m_constantBuffer, 0);
	}
	m_contextD3D->VSSetConstantBuffers(0, 1, &m_constantBuffer);

	bool result = false;
	if (m_currentMode == RenderMode::Tiled)
	{
		result = RenderTiled();
	}
	else
	{
		result = RenderSingle();
	}

	return true;
}

void ImageRenderLayer::OnResize(uint32_t width, uint32_t height)
{
	if (!m_camera)
		return;

	m_camera->SetViewSize(width, height);
	m_camera->Fit();
}

void ImageRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void ImageRenderLayer::OnDeviceRestored()
{
	if (!m_context)
		return;

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return;

	m_device = engine->GetD3DDevice();
	m_contextD3D = engine->GetD3DDeviceContext();

	if (!m_device || !m_contextD3D)
		return;

	if (!CreateDeviceResources())
	{
		ReleaseDeviceResources();
		return;
	}
}

void ImageRenderLayer::SetCamera2D(Camera2D* camera)
{
	m_camera = camera;
}

void ImageRenderLayer::SetTileManager(TileManager* tileManager)
{
	m_tileManager = tileManager;
}

void ImageRenderLayer::SetFrameID(uint64_t frameID)
{
	m_frameID = frameID;
}

bool ImageRenderLayer::IsImageRenderDirty() const
{
	bool isDirty = false;
	if (m_tileManager->HasPendingUploads())
	{
		isDirty = true;
	}

	return isDirty;
}

bool ImageRenderLayer::UpdateImage(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	if (!data || width == 0 || height == 0 || stride == 0)
		return false;

	if (width > 8192 || height > 8192)
	{
		m_currentMode = RenderMode::Tiled;

		if (m_tileManager)
		{
			m_tileManager->ClearPools();
		}
	}
	else
	{
		m_currentMode = RenderMode::Single;

		if (!CreateSingleBuffer(width, height))
			return false;

		if (channel == 4)
		{
			m_contextD3D->UpdateSubresource(m_singleTexture, 0, nullptr, data, stride, 0);
		}
		else if (channel == 1 || channel == 3)
		{
			UploadSingleImage_GPU(data, width, height, stride, channel);
		}
	}

	// 1. ?ъ씤?곗? 硫뷀??곗씠?곕쭔 ???
	m_texWidth = width;
	m_texHeight = height;

	if (m_image)
	{
		m_image->Attach(const_cast<uint8_t*>(data), width, height, stride, channel, 8);
	}

	// 2. 移대찓???ㅼ젙
	if (m_camera)
	{
		m_camera->SetImageSize(width, height);
		m_camera->FitInstant();
	}

	return true;
}

bool ImageRenderLayer::UpdateTexture(ID3D11Texture2D* texture, uint32_t& width, uint32_t& height)
{
	if (!texture)
		return false;

	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);

	if (desc.Width == 0 || desc.Height == 0 || desc.Width > 8192 || desc.Height > 8192)
		return false;

	ID3D11Device* sourceDevice = nullptr;
	texture->GetDevice(&sourceDevice);
	const bool sameDevice = (sourceDevice == m_device);
	SafeRelease(sourceDevice);
	if (!sameDevice)
		return false;

	m_currentMode = RenderMode::Single;
	m_texWidth = width = desc.Width;
	m_texHeight = height = desc.Height;

	if (!CreateSingleBuffer(width, height))
		return false;

	m_contextD3D->CopyResource(m_singleTexture, texture);

	if (m_image)
	{
		m_image->ReleaseBuffer();
	}

	if (m_camera)
	{
		m_camera->SetImageSize(width, height);
		m_camera->FitInstant();
	}

	return true;
}

bool ImageRenderLayer::UpdateSharedTexture(HANDLE sharedHandle, uint32_t& width, uint32_t& height)
{
	if (!sharedHandle) return false;

	if (width > 8192 || height > 8192)
	{
		return false;
	}
	else
	{
		m_currentMode = RenderMode::Single;

		if (!OpenSharedResource(sharedHandle))
			return false;

		D3D11_TEXTURE2D_DESC desc = {};
		m_sharedTexture->GetDesc(&desc);

		m_texWidth = width = desc.Width;
		m_texHeight = height = desc.Height;

		if (!CreateSingleBuffer(width, height))
			return false;

		m_contextD3D->CopyResource(m_singleTexture, m_sharedTexture);
	}


	// 1. ?ъ씤?곗? 硫뷀??곗씠?곕쭔 ???
	m_texWidth = width;
	m_texHeight = height;

	if (m_image)
	{
		m_image->ReleaseBuffer();
		//m_image->Attach(const_cast<uint8_t*>(data), width, height, stride, channel, 8);
	}

	// 2. 移대찓???ㅼ젙
	if (m_camera)
	{
		m_camera->SetImageSize(width, height);
		m_camera->FitInstant();
	}

	return true;
}

RenderMode ImageRenderLayer::GetRenderMode() const
{
	return m_currentMode;
}

const Core::ImageType::ImageBase* ImageRenderLayer::GetImage() const
{
	return m_image;
}

bool ImageRenderLayer::CreateDeviceResources()
{
	ReleaseDeviceResources();

	uint32_t tileWidth(0), tileHeight(0);
	m_tileManager->GetTileSize(0, tileWidth, tileHeight);

	if (!CreateShaders()) goto FAIL;
	if (!CreateGeometry(tileWidth, tileHeight)) goto FAIL;
	if (!CreateSampler()) goto FAIL;
	if (!CreateConstantBuffer()) goto FAIL;
	if (!CreateRasterizerState()) goto FAIL;
	if (!CreateTileDynamicBuffer(128)) goto FAIL;
	if (!CreateRawUploadBuffer(8192 * 8192 * 4)) goto FAIL;

	return true;

FAIL:
	ReleaseDeviceResources();
	return false;
}

void ImageRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_sampler);

	SafeRelease(m_constantBuffer);
	SafeRelease(m_wireColorBuffer);

	SafeRelease(m_tileVertexBuffer);
	SafeRelease(m_indexBuffer);

	SafeRelease(m_rasterizerWireFrame);
	SafeRelease(m_rasterizerSolid);

	SafeRelease(m_singleTextureCS);

	SafeRelease(m_inputLayout);
	SafeRelease(m_vs);
	SafeRelease(m_ps);
	SafeRelease(m_wirePS);

	SafeRelease(m_singleSRV);
	SafeRelease(m_singleTexture);
	SafeRelease(m_singleUAV);
	SafeRelease(m_rawUploadBuffer);
	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_singleConvertCB);

	SafeRelease(m_sharedTexture);
}

bool ImageRenderLayer::CreateShaders()
{
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* wirePSBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* csBlob = nullptr;

	HRESULT hr = ::D3DReadFileToBlob(L"../Shaders/ImageVS.cso", &vsBlob);

	//HRESULT hr = ::D3DCompileFromFile(
	//	L"ImageVS.hlsl",
	//	nullptr,
	//	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	//	"main",
	//	"vs_5_0",
	//	D3DCOMPILE_ENABLE_STRICTNESS,
	//	0,
	//	&vsBlob,
	//	&errorBlob
	//);

	if (FAILED(hr))
	{
		SafeRelease(errorBlob);
		return false;
	}

	hr = ::D3DReadFileToBlob(L"../Shaders/ImagePS.cso", &psBlob);

	//hr = ::D3DCompileFromFile(
	//	L"ImagePS.hlsl",
	//	nullptr,
	//	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	//	"main",
	//	"ps_5_0",
	//	D3DCOMPILE_ENABLE_STRICTNESS,
	//	0,
	//	&psBlob,
	//	&errorBlob
	//);

	if (FAILED(hr))
	{
		SafeRelease(vsBlob);
		SafeRelease(errorBlob);
		return false;
	}

	hr = ::D3DReadFileToBlob(L"../Shaders/WireFramePS.cso", &wirePSBlob);

	if (FAILED(hr))
	{
		SafeRelease(vsBlob);
		SafeRelease(errorBlob);
		SafeRelease(wirePSBlob);
		return false;
	}

	hr = ::D3DReadFileToBlob(L"../Shaders/SingleConvertCS.cso", &csBlob);

	// Create shaders
	if (SUCCEEDED(hr))
	{
		hr = m_device->CreateComputeShader(
			csBlob->GetBufferPointer(),
			csBlob->GetBufferSize(),
			nullptr,
			&m_singleTextureCS
		);

		SafeRelease(csBlob);
	}


	hr = m_device->CreateVertexShader(
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		nullptr,
		&m_vs
	);

	if (FAILED(hr))
		goto CLEANUP;

	hr = m_device->CreatePixelShader(
		psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(),
		nullptr,
		&m_ps
	);

	if (FAILED(hr))
		goto CLEANUP;

	hr = m_device->CreatePixelShader(
		wirePSBlob->GetBufferPointer(),
		wirePSBlob->GetBufferSize(),
		nullptr,
		&m_wirePS
	);

	if (FAILED(hr))
		goto CLEANUP;

	{
		// Input Layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		hr = m_device->CreateInputLayout(
			layout,
			ARRAYSIZE(layout),
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			&m_inputLayout
		);
	}

CLEANUP:
	SafeRelease(vsBlob);
	SafeRelease(psBlob);
	SafeRelease(errorBlob);
	SafeRelease(wirePSBlob);
	SafeRelease(csBlob);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateConstantBuffer()
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(ViewParams);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbWireFrameDesc = {};
	cbWireFrameDesc.ByteWidth = 16; // float4
	cbWireFrameDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbWireFrameDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbWireFrameDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = m_device->CreateBuffer(&cbWireFrameDesc, nullptr, &m_wireColorBuffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = 32;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_device->CreateBuffer(&cbd, nullptr, &m_singleConvertCB);
	if (FAILED(hr))
		return false;

	return true;
}

bool ImageRenderLayer::CreateGeometry(uint32_t tileWidth, uint32_t tileHeight)
{
	HRESULT hr = S_OK;

	// Index Buffer
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(kQuadIndices);
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = kQuadIndices;

	hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
	if (FAILED(hr))
		return false;

	return true;
}

bool ImageRenderLayer::CreateSampler()
{
	D3D11_SAMPLER_DESC sd = {};
	//sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0.0;
	sd.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = m_device->CreateSamplerState(&sd, &m_sampler);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateRasterizerState()
{
	HRESULT hr = S_OK;
	D3D11_RASTERIZER_DESC rd = {};

	rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;

	hr = m_device->CreateRasterizerState(&rd, &m_rasterizerWireFrame);

	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;

	hr = m_device->CreateRasterizerState(&rd, &m_rasterizerSolid);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::CreateTileDynamicBuffer(uint32_t maxTileCount)
{
	SafeRelease(m_tileVertexBuffer);

	m_maxTileVertexCount = maxTileCount * 6;

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = static_cast<UINT>(sizeof(GRAPHICS::BatchVertex) * m_maxTileVertexCount);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	return SUCCEEDED(m_device->CreateBuffer(&bd, nullptr, &m_tileVertexBuffer));
}

bool ImageRenderLayer::CreateSingleBuffer(uint32_t width, uint32_t height)
{
	if (m_singleTexture && m_singleTextureWidth == width && m_singleTextureHeight == height)
		return true;

	// 湲곗〈 由ъ냼???댁젣
	SafeRelease(m_singleSRV);
	SafeRelease(m_singleTexture);

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 理쒖쥌 異쒕젰??BGRA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT; // CPU?먯꽌 GPU濡?蹂듭궗?????ъ슜
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_singleTexture);
	if (FAILED(hr))
		return false;

	hr = m_device->CreateShaderResourceView(m_singleTexture, nullptr, &m_singleSRV);
	if (FAILED(hr))
		return false;

	hr = m_device->CreateUnorderedAccessView(m_singleTexture, nullptr, &m_singleUAV);
	if (FAILED(hr))
		return false;

	m_singleTextureWidth = width;
	m_singleTextureHeight = height;

	return true;
}

bool ImageRenderLayer::CreateRawUploadBuffer(uint32_t maxByteSize)
{
	if (m_maxByteSize == maxByteSize)
		return true;

	SafeRelease(m_rawUploadSRV);
	SafeRelease(m_rawUploadBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = maxByteSize; // 8k x 8k x 3梨꾨꼸 = ??192MB ?댁긽 沅뚯옣
	desc.Usage = D3D11_USAGE_DYNAMIC; // 留??꾨젅??Map/Unmap???꾪빐 Dynamic
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; // Raw Access ?덉슜

	HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &m_rawUploadBuffer);
	if (FAILED(hr))
		return false;

	// Shader Resource View (SRV) ?앹꽦
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Raw Buffer??Typeless濡??ㅼ젙
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = maxByteSize / 4; // 4諛붿씠???⑥쐞 媛쒖닔
	srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW; // ?듭떖: RAW ?뚮옒洹?

	hr = m_device->CreateShaderResourceView(m_rawUploadBuffer, &srvDesc, &m_rawUploadSRV);

	return SUCCEEDED(hr);
}

bool ImageRenderLayer::OpenSharedResource(HANDLE sharedHandle)
{
	HRESULT hr = S_OK;

	if (sharedHandle != m_sharedHandle || m_sharedTexture == nullptr)
	{
		SafeRelease(m_sharedTexture);

		hr = m_device->OpenSharedResource(
			sharedHandle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_sharedTexture));

		if (SUCCEEDED(hr))
		{
			m_sharedHandle = sharedHandle;
		}
		else
		{
			sharedHandle = nullptr;
			return false; // ?닿린 ?ㅽ뙣 ??以묐떒
		}
	}

	return true;
}

bool ImageRenderLayer::CheckViewChanged()
{
	Core::ShapeType::Rect2i currentRect = m_camera->GetViewImageRect();
	float currentZoom = m_camera->GetZoom();
	uint32_t currentTileCount = (uint32_t)m_tileManager->GetVisibleTiles().size();

	bool changed = false;

	// 1. ?꾩튂/?ш린 蹂??泥댄겕
	if (m_prevViewRect.left != currentRect.left || m_prevViewRect.top != currentRect.top ||
		m_prevViewRect.right != currentRect.right || m_prevViewRect.bottom != currentRect.bottom)
	{
		changed = true;
	}

	// 2. 以?蹂??泥댄겕 (遺?숈냼?섏젏 ?ㅼ감 怨좊젮)
	if (std::abs(m_prevZoom - currentZoom) > 1e-6f)
	{
		changed = true;
	}

	// 3. ???媛쒖닔 蹂??泥댄겕
	if (m_prevTileCount != currentTileCount)
	{
		changed = true;
	}

	// 蹂寃쎈릺?덈떎硫??ㅼ쓬 ?꾨젅?꾩쓣 ?꾪빐 ?곹깭 ?낅뜲?댄듃
	if (changed)
	{
		m_prevViewRect = currentRect;
		m_prevZoom = currentZoom;
		m_prevTileCount = currentTileCount;
	}

	return changed;
}

void ImageRenderLayer::UploadSingleImage_GPU(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint32_t channel)
{
	// [1] Raw ?곗씠?곕? GPU 踰꾪띁(m_rawUploadBuffer)???낅줈??
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_contextD3D->Map(m_rawUploadBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);

		if (stride == width * channel)
		{
			memcpy(dst, data, height * stride);
		}
		else
		{
			// stride瑜?怨좊젮?섏뿬 以??⑥쐞 蹂듭궗 (?대?吏 ??쭔?쇰쭔 蹂듭궗)
			for (uint32_t y = 0; y < height; ++y)
			{
				memcpy(dst + (y * width * channel),
					data + y * stride,
					width * channel);
			}
		}

		m_contextD3D->Unmap(m_rawUploadBuffer, 0);
	}

	// [2] ?곸닔 踰꾪띁 ?ㅼ젙 (蹂?섏슜 ?뚮씪誘명꽣)
	struct {
		uint32_t width;
		uint32_t height;
		uint32_t stride; // ?ш린?쒕뒗 GPU 踰꾪띁 湲곗??대?濡?width * channel
		uint32_t channel;
	} cb;
	cb.width = width;
	cb.height = height;
	cb.stride = width * channel;
	cb.channel = channel;

	m_contextD3D->UpdateSubresource(m_singleConvertCB, 0, nullptr, &cb, 0, 0);

	// [3] CS ?ㅽ뻾
	m_contextD3D->CSSetShader(m_singleTextureCS, nullptr, 0);
	m_contextD3D->CSSetConstantBuffers(0, 1, &m_singleConvertCB);
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &m_singleUAV, nullptr);
	m_contextD3D->CSSetShaderResources(0, 1, &m_rawUploadSRV);

	// 16x16 洹몃９ ?ㅼ?以꾨쭅
	m_contextD3D->Dispatch((width + 15) / 16, (height + 15) / 16, 1);

	// [4] ?먯썝 ?댁젣 (以묒슂)
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	m_contextD3D->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}

bool ImageRenderLayer::RenderTiled()
{
	const auto& renderDataList = m_tileManager->GetRenderDataList();
	if (renderDataList.empty()) return true;

	bool viewChanged = CheckViewChanged();
	if (viewChanged || m_tileManager->HasPendingUploads() || m_renderVertices.empty())
	{
		m_renderVertices.clear();
		const float imageWidth = static_cast<float>(m_texWidth);
		const float imageHeight = static_cast<float>(m_texHeight);

		for (const auto& data : renderDataList)
		{
			const Core::ShapeType::Rect2i rect = m_tileManager->CalcTilePixelRect(data.targetKey);
			const float x0 = (float)rect.left;
			const float y0 = (float)rect.top;
			const float x1 = min((float)rect.right, imageWidth);
			const float y1 = min((float)rect.bottom, imageHeight);

			uint32_t tw, th;
			m_tileManager->GetTileSize(data.targetKey.lod, tw, th);
			const float scale = (float)(1 << data.targetKey.lod);

			float localU = (x1 - x0) / (tw * scale);
			float localV = (y1 - y0) / (th * scale);
			float finalU1 = data.u0 + (data.u1 - data.u0) * localU;
			float finalV1 = data.v0 + (data.v1 - data.v0) * localV;

			const uint32_t texIdx = data.tile->arrayIndex;
			m_renderVertices.push_back({ {x0, y0, 0.f}, {data.u0, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y0, 0.f}, {finalU1, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y1, 0.f}, {finalU1, finalV1}, texIdx });
			m_renderVertices.push_back({ {x0, y0, 0.f}, {data.u0, data.v0}, texIdx });
			m_renderVertices.push_back({ {x1, y1, 0.f}, {finalU1, finalV1}, texIdx });
			m_renderVertices.push_back({ {x0, y1, 0.f}, {data.u0, finalV1}, texIdx });
		}

		UpdateVertexBuffer(m_renderVertices);
	}

	// ?뚯씠?꾨씪??諛붿씤??諛?洹몃━湲?
	SetCommonShaderStates();

	uint32_t vertexOffset = 0;
	for (uint32_t i = 0; i < renderDataList.size(); )
	{
		uint32_t currentLOD = renderDataList[i].tile->key.lod;
		ID3D11ShaderResourceView* poolSRV = m_tileManager->GetPoolSRV(currentLOD);
		m_contextD3D->PSSetShaderResources(0, 1, &poolSRV);

		uint32_t batchCount = 0;
		while (i + batchCount < renderDataList.size() && renderDataList[i + batchCount].tile->key.lod == currentLOD)
			batchCount++;

		m_contextD3D->Draw(batchCount * 6, vertexOffset);
		vertexOffset += batchCount * 6;
		i += batchCount;
	}

	if (m_renderWireFrame)
	{
		m_contextD3D->RSSetState(m_rasterizerWireFrame);
		m_contextD3D->VSSetShader(m_vs, nullptr, 0);
		m_contextD3D->PSSetShader(m_wirePS, nullptr, 0);

		// Wire Frame Color
		constexpr float wireColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

		D3D11_MAPPED_SUBRESOURCE mappedCB{};
		if (SUCCEEDED(m_contextD3D->Map(m_wireColorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			memcpy(mappedCB.pData, &wireColor, sizeof(wireColor));
			m_contextD3D->Unmap(m_wireColorBuffer, 0);
		}

		// Wiare Frame Pixel Shader Constant Buffer(register b1)
		m_contextD3D->PSSetConstantBuffers(1, 1, &m_wireColorBuffer);

		// SRV 遺덊븘???섎?濡?Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_contextD3D->PSSetShaderResources(0, 1, &nullSRV);

		m_contextD3D->Draw(static_cast<UINT>(m_renderVertices.size()), 0);
	}

	return true;
}

bool ImageRenderLayer::RenderSingle()
{
	if (!m_singleSRV) return false;

	bool viewChanged = CheckViewChanged();
	if (viewChanged || m_renderVertices.empty())
	{
		m_renderVertices.clear();
		float w = (float)m_texWidth;
		float h = (float)m_texHeight;

		// ?띿뒪泥??몃뜳?ㅻ뒗 ?섎? ?놁쑝誘濡?0?쇰줈 ?ㅼ젙
		m_renderVertices.push_back({ {0.f, 0.f, 0.f}, {0.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, 0.f, 0.f}, {1.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, h, 0.f}, {1.f, 1.f}, 0 });
		m_renderVertices.push_back({ {0.f, 0.f, 0.f}, {0.f, 0.f}, 0 });
		m_renderVertices.push_back({ {w, h, 0.f}, {1.f, 1.f}, 0 });
		m_renderVertices.push_back({ {0.f, h, 0.f}, {0.f, 1.f}, 0 });

		UpdateVertexBuffer(m_renderVertices);
	}

	SetCommonShaderStates();
	m_contextD3D->PSSetShaderResources(0, 1, &m_singleSRV);
	m_contextD3D->Draw(6, 0);

	return true;
}

void ImageRenderLayer::SetCommonShaderStates()
{
	uint32_t stride = sizeof(GRAPHICS::BatchVertex);
	uint32_t offset = 0;
	m_contextD3D->IASetInputLayout(m_inputLayout);
	m_contextD3D->IASetVertexBuffers(0, 1, &m_tileVertexBuffer, &stride, &offset);
	m_contextD3D->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_contextD3D->VSSetShader(m_vs, nullptr, 0);
	m_contextD3D->RSSetState(m_rasterizerSolid);
	m_contextD3D->PSSetShader(m_ps, nullptr, 0);
	m_contextD3D->PSSetSamplers(0, 1, &m_sampler);
}

void ImageRenderLayer::UpdateVertexBuffer(const std::vector<GRAPHICS::BatchVertex>& vertices)
{
	if (vertices.empty()) return;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(m_contextD3D->Map(m_tileVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, vertices.data(), sizeof(GRAPHICS::BatchVertex) * vertices.size());
		m_contextD3D->Unmap(m_tileVertexBuffer, 0);
	}
}






