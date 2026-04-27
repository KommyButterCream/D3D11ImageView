#include "pch.h"
#include "ImageCenterRenderLayer.h"

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"

ImageCenterRenderLayer::ImageCenterRenderLayer()
{
}

ImageCenterRenderLayer::~ImageCenterRenderLayer()
{
	Shutdown();
}

bool ImageCenterRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
		return false;

	m_context = context;

	if (!AcquireDeviceResources())
		return false;

	m_context->AddDeviceListener(this);

	m_initialized = true;

	return true;
}

void ImageCenterRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}

	ReleaseDeviceResources();

	m_context = nullptr;
	m_initialized = false;
}

bool ImageCenterRenderLayer::Prepare()
{
	return true;
}

bool ImageCenterRenderLayer::Render()
{
	if (!m_initialized)
		return true;

	if (!m_d2dContext)
		return false;

	if (!m_camera)
		return false;

	ID2D1DeviceContext* dc = m_d2dContext;

	const float zoomScale = m_camera->GetZoom();
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	D2D1_MATRIX_3X2_F oldTransform = {};
	dc->GetTransform(&oldTransform);

	dc->SetTransform(
		D2D1::Matrix3x2F::Scale(zoomScale, zoomScale) *
		D2D1::Matrix3x2F::Translation(-offsetX * zoomScale, -offsetY * zoomScale));

	// scale & overlay mode 반영
	const float strokeWidth = 1.0f / zoomScale;
	//const float strokeWidth = max(1.0f / zoomScale, 1.0f);

	uint32_t imageWidth = 0;
	uint32_t imageHeight = 0;
	m_camera->GetImageSize(imageWidth, imageHeight);

	const float centerX = (static_cast<float>(imageWidth - 1)) * 0.5f;
	const float centerY = (static_cast<float>(imageHeight - 1)) * 0.5f;

	const D2D1_POINT_2F point1 = { 0.0f, centerY };
	const D2D1_POINT_2F point2 = { static_cast<float>(imageWidth), centerY };
	const D2D1_POINT_2F point3 = { centerX, 0.0f };
	const D2D1_POINT_2F point4 = { centerX, static_cast<float>(imageHeight) };

	dc->DrawLine(point1, point2, m_lineBrush, strokeWidth);
	dc->DrawLine(point3, point4, m_lineBrush, strokeWidth);

	dc->SetTransform(oldTransform);

	return true;
}

void ImageCenterRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void ImageCenterRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

void ImageCenterRenderLayer::SetCamera2D(const Camera2D* camera)
{
	m_camera = camera;
}

bool ImageCenterRenderLayer::SetLineColor(const D2D1::ColorF& color)
{
	if (!m_initialized)
		return false;

	m_lineColor = color;

	if (m_lineBrush)
		m_lineBrush->SetColor(m_lineColor);

	return true;
}

bool ImageCenterRenderLayer::SetDashStyle(const D2D1_DASH_STYLE& style)
{
	if (!m_initialized)
		return false;

	if (!m_d2dFactory)
		return false;

	SafeRelease(m_dashStroke);

	m_dashStyle = style;

	D2D1_STROKE_STYLE_PROPERTIES strokeProps = {};
	strokeProps.dashStyle = m_dashStyle;

	HRESULT hr = m_d2dFactory->CreateStrokeStyle(
		strokeProps,
		nullptr,
		0,
		&m_dashStroke);

	if (FAILED(hr))
		return false;

	return true;
}

bool ImageCenterRenderLayer::AcquireDeviceResources()
{
	m_d2dContext = m_context->GetD2DDeviceContext();
	if (!m_d2dContext)
		return false;

	D3D11RenderEngine* engine = static_cast<D3D11RenderEngine*>(m_context->GetEngine());
	if (!engine)
		return false;

	m_d2dFactory = engine->GetD2DFactory();
	if (!m_d2dFactory)
		return false;

	return CreateBrushAndStyle();
}

void ImageCenterRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_lineBrush);
	SafeRelease(m_dashStroke);
}

bool ImageCenterRenderLayer::CreateBrushAndStyle()
{
	ReleaseDeviceResources();

	if (!m_d2dContext)
		return false;

	HRESULT hr = S_OK;

	hr = m_d2dContext->CreateSolidColorBrush(
		m_lineColor, &m_lineBrush);

	if (FAILED(hr))
		return false;

	D2D1_STROKE_STYLE_PROPERTIES strokeProps = {};
	strokeProps.dashStyle = m_dashStyle;

	hr = m_d2dFactory->CreateStrokeStyle(
		strokeProps,
		nullptr,
		0,
		&m_dashStroke);

	if (FAILED(hr))
		return false;

	return true;
}



