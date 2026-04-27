#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"
#include "../../../Module/Core/ShapeType/Point2i.h"

class IRenderContext;

struct ID2D1DeviceContext;
struct ID2D1Factory1;
struct ID2D1SolidColorBrush;
struct ID2D1StrokeStyle;

class SelectionRectRenderLayer 
	: public IRenderLayer
	, public IDeviceEventListener
{
public:
	SelectionRectRenderLayer();
	virtual ~SelectionRectRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;

public:
	void OnLButtonDown(const Core::ShapeType::Point2i& point);
	void OnMouseMove(const Core::ShapeType::Point2i& point);
	void OnLButtonUp(const Core::ShapeType::Point2i& point);

public:
	bool SetFillColor(const D2D1::ColorF& color);
	bool SetOutlineColor(const D2D1::ColorF& color);
	bool SetDashStyle(const D2D1_DASH_STYLE& style);

private:
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();
	bool CreateBrushAndStyle();

private:
	// Context
	IRenderContext* m_context = nullptr;

	// D2D target
	ID2D1DeviceContext* m_d2dContext = nullptr;
	ID2D1Factory1* m_d2dFactory = nullptr;
	
	// D2D Resources
	ID2D1SolidColorBrush* m_fillBrush = nullptr;
	ID2D1SolidColorBrush* m_outlineBrush = nullptr;
	ID2D1StrokeStyle* m_dashStroke = nullptr;

	// D2D Resource Color & Type
	D2D1::ColorF m_fillColor = D2D1::ColorF(0.15f, 0.80f, 0.78f, 0.30f);
	D2D1::ColorF m_outlineColor = D2D1::ColorF(0.15f, 0.80f, 0.78f, 1.0f);
	D2D1_DASH_STYLE	m_dashStyle = D2D1_DASH_STYLE::D2D1_DASH_STYLE_DASH;

	// State
	bool m_initialized = false;
	bool m_dragging = false;

	// Mouse Position
	Core::ShapeType::Point2i m_startScreen = {};
	Core::ShapeType::Point2i m_currentScreen = {};
};






