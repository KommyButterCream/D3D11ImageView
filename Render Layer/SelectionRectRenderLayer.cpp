#include "pch.h"
#include "SelectionRectRenderLayer.h"

#include "../../../Module/D3D11Engine/Core/D3D11RenderEngine.h"

#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11EngineInterface/IRenderStructures.h"

using namespace Core::ShapeType;

SelectionRectRenderLayer::SelectionRectRenderLayer()
{
}

SelectionRectRenderLayer::~SelectionRectRenderLayer()
{
	Shutdown();
}

bool SelectionRectRenderLayer::Initialize(IRenderContext* context)
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

void SelectionRectRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}
	
	ReleaseDeviceResources();

	m_context = nullptr;
	m_initialized = false;
}

bool SelectionRectRenderLayer::Prepare()
{
	return true;
}

bool SelectionRectRenderLayer::Render()
{
	if (!m_initialized || !m_dragging)
		return true;

	if (!m_d2dContext)
		return false;

	if (m_startScreen.x == m_currentScreen.x &&
		m_startScreen.y == m_currentScreen.y)
		return true;

	ID2D1DeviceContext* dc = m_d2dContext;

	FLOAT x1 = static_cast<FLOAT>(m_startScreen.x);
	FLOAT y1 = static_cast<FLOAT>(m_startScreen.y);
	FLOAT x2 = static_cast<FLOAT>(m_currentScreen.x);
	FLOAT y2 = static_cast<FLOAT>(m_currentScreen.y);

	D2D1_RECT_F rect = D2D1::RectF(
		min(x1, x2),
		min(y1, y2),
		max(x1, x2),
		max(y1, y2)
	);

	dc->FillRectangle(rect, m_fillBrush);
	dc->DrawRectangle(rect, m_outlineBrush, 1.0f, m_dashStroke);

	return true;
}

void SelectionRectRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();
}

void SelectionRectRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

void SelectionRectRenderLayer::OnLButtonDown(const Point2i& point)
{
	m_startScreen = point;
	m_currentScreen = point;
	m_dragging = true;
}

void SelectionRectRenderLayer::OnMouseMove(const Point2i& point)
{
	if (!m_dragging)
		return;

	m_currentScreen = point;
}

void SelectionRectRenderLayer::OnLButtonUp(const Point2i& point)
{
	if (!m_dragging)
		return;

	m_currentScreen = point;
	m_dragging = false;
}

bool SelectionRectRenderLayer::SetFillColor(const D2D1::ColorF& color)
{
	if (!m_initialized)
		return false;

	if (m_dragging)
		return false;

	m_fillColor = color; 

	if(m_fillBrush)
		m_fillBrush->SetColor(m_fillColor);

	return true;
}

bool SelectionRectRenderLayer::SetOutlineColor(const D2D1::ColorF& color)
{
	if (!m_initialized)
		return false;

	if (m_dragging)
		return false;

	m_outlineColor = color; 

	if (m_outlineBrush)
		m_outlineBrush->SetColor(m_outlineColor);

	return true;
}

bool SelectionRectRenderLayer::SetDashStyle(const D2D1_DASH_STYLE& style)
{
	if (!m_initialized)
		return false;

	if (m_dragging)
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

bool SelectionRectRenderLayer::AcquireDeviceResources()
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

void SelectionRectRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_fillBrush);
	SafeRelease(m_outlineBrush);
	SafeRelease(m_dashStroke);
}

bool SelectionRectRenderLayer::CreateBrushAndStyle()
{
	ReleaseDeviceResources();

	if (!m_d2dContext)
		return false;

	HRESULT hr = S_OK;

	hr = m_d2dContext->CreateSolidColorBrush(
		m_fillColor, &m_fillBrush);

	if (FAILED(hr))
		return false;

	hr = m_d2dContext->CreateSolidColorBrush(
		m_outlineColor, &m_outlineBrush);

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



