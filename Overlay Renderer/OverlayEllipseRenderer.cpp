#include "pch.h"
#include "OverlayEllipseRenderer.h"

#include "OverlayRenderContext.h"
#include "OverlayUtilities.h"

using namespace Core::ShapeType;

OverlayEllipseRenderer::OverlayEllipseRenderer(const OverlayEllipse& ellipse)
	: m_ellipse(ellipse)
{
	m_bounds = {
		m_ellipse.cx - m_ellipse.rx,
		m_ellipse.cy - m_ellipse.ry,
		m_ellipse.cx + m_ellipse.rx,
		m_ellipse.cy + m_ellipse.ry
	};
}

OverlayShapeType OverlayEllipseRenderer::GetShapeType() const
{
	return OverlayShapeType::Ellipse;
}

const Rect2f& OverlayEllipseRenderer::GetBounds() const
{
	return m_bounds;
}

void OverlayEllipseRenderer::Render(const OverlayRenderContext& context) const
{
	if (!context.d2dContext || !context.strokeBrush || !context.fillBrush)
	{
		return;
	}

	const OverlayStyle& style = m_ellipse.style;
	const float strokeWidth = OverlayUtilities::ResolveStrokeWidth(context, style);

	context.strokeBrush->SetColor(style.strokeColorD2D);
	context.fillBrush->SetColor(style.fillColorD2D);

	const D2D1_POINT_2F center = { m_ellipse.cx, m_ellipse.cy };

	const D2D1_ELLIPSE d2dEllipse = D2D1::Ellipse(center, m_ellipse.rx, m_ellipse.ry);
	const float angleDeg = Core::Util::RadToDeg(m_ellipse.angleRad);

	D2D1_MATRIX_3X2_F originalTransform = {};
	context.d2dContext->GetTransform(&originalTransform);
	context.d2dContext->SetTransform(
		D2D1::Matrix3x2F::Rotation(angleDeg, center) * originalTransform);

	if (!style.transparentFill)
	{
		context.d2dContext->FillEllipse(d2dEllipse, context.fillBrush);
	}

	context.d2dContext->DrawEllipse(d2dEllipse, context.strokeBrush, strokeWidth);
	context.d2dContext->SetTransform(originalTransform);
}

