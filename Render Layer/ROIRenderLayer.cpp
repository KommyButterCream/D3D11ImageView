#include "pch.h"
#include "ROIRenderLayer.h"

#include "../ROI Renderer/IROIObject.h"
#include "../ROI Renderer/ROICircleRenderer.h"
#include "../ROI Renderer/ROIEllipseRenderer.h"
#include "../ROI Renderer/ROILineRenderer.h"
#include "../ROI Renderer/ROIRenderContext.h"
#include "../ROI Renderer/ROIPolygonRenderer.h"
#include "../ROI Renderer/ROIRectangleRenderer.h"

#include "../../../Module/D3D11Engine/Camera/Camera2D.h"
#include "../../../Module/D3D11EngineInterface/IRenderContext.h"
#include "../../../Module/D3D11EngineInterface/IRenderEngine.h"
#include "../../../Module/D3D11Engine/Font/FontManager.h"

#include "../ROI Renderer/ROIUtilities.h"

using namespace Core::ShapeType;

ROIRenderLayer::ROIRenderLayer()
{
}

ROIRenderLayer::~ROIRenderLayer()
{
	Shutdown();
}

bool ROIRenderLayer::Initialize(IRenderContext* context)
{
	if (!context)
	{
		return false;
	}

	m_context = context;

	if (!AcquireDeviceResources())
	{
		return false;
	}

	m_context->AddDeviceListener(this);
	m_initialized = true;

	return true;
}

void ROIRenderLayer::Shutdown()
{
	if (m_context)
	{
		m_context->RemoveDeviceListener(this);
	}

	ROIClear();
	ReleaseDeviceResources();

	m_context = nullptr;
	m_camera = nullptr;
	m_hoveredObject = nullptr;
	m_selectedObject = nullptr;
	m_activeObject = nullptr;
	m_activeHit = {};
	m_isDragging = false;
	m_initialized = false;
}

bool ROIRenderLayer::Prepare()
{
	return true;
}

bool ROIRenderLayer::Render()
{
	if (!m_initialized)
	{
		return true;
	}

	if (!m_d2dContext || !m_camera)
	{
		return false;
	}

	const float zoom = max(m_camera->GetZoom(), 0.0001f);
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	ROIRenderContext renderContext = {};
	renderContext.d2dContext = m_d2dContext;
	renderContext.d2dFactory = m_d2dFactory;
	renderContext.strokeBrush = m_strokeBrush;
	renderContext.fillBrush = m_fillBrush;
	renderContext.handleFillBrush = m_handleFillBrush;
	renderContext.handleOutlineBrush = m_handleOutlineBrush;
	renderContext.zoom = zoom;
	renderContext.strokeWidth = 1.0f / zoom;
	renderContext.handleHalfSize = 4.0f / zoom;
	const Rect2f visibleRect = GetVisibleClientRect();
	const float visiblePadding = 6.0f;

	D2D1_MATRIX_3X2_F originalTransform = {};
	m_d2dContext->GetTransform(&originalTransform);
	m_d2dContext->SetTransform(
		D2D1::Matrix3x2F::Scale(zoom, zoom) *
		D2D1::Matrix3x2F::Translation(-offsetX * zoom, -offsetY * zoom));

	::AcquireSRWLockShared(&m_roiLock);

	for (const auto& roiObject : m_roiObjects)
	{
		if (roiObject.get() == m_selectedObject)
		{
			continue;
		}

		if (!IsVisibleOnClient(roiObject->GetBounds(), visibleRect, visiblePadding))
		{
			continue;
		}

		roiObject->Render(renderContext, false, roiObject.get() == m_hoveredObject);
	}

	if (m_selectedObject && IsVisibleOnClient(m_selectedObject->GetBounds(), visibleRect, visiblePadding))
	{
		m_selectedObject->Render(renderContext, true, m_selectedObject == m_hoveredObject);
	}

	// 라벨은 화면 좌표계에서 그린다. 변환을 먼저 되돌린다.
	// 도형을 전부 그린 뒤이므로 라벨이 항상 위에 온다.
	m_d2dContext->SetTransform(originalTransform);
	RenderNameLabels(visibleRect);

	::ReleaseSRWLockShared(&m_roiLock);

	return true;
}

void ROIRenderLayer::RenderNameLabels(const Rect2f& visibleRect)
{
	// 호출자가 m_roiLock 을 shared 로 쥐고 있다.
	if (!m_context || !m_d2dContext || !m_strokeBrush || !m_fillBrush)
	{
		return;
	}

	IRenderEngine* engine = m_context->GetEngine();
	FontManager* fontManager = engine ? engine->GetFontManager() : nullptr;
	if (!fontManager)
	{
		return;
	}

	IDWriteFactory* dwriteFactory = fontManager->GetDWriteFactory();
	if (!dwriteFactory)
	{
		return;
	}

	constexpr float kPaddingX = 5.0f;
	constexpr float kPaddingY = 2.0f;
	constexpr float kGap = 3.0f;        // 도형과 라벨 사이 간격
	constexpr float kCornerRadius = 3.0f;
	constexpr float kDefaultFontSize = 14.0f;

	for (const auto& roiObject : m_roiObjects)
	{
		const bool isLine = (roiObject->GetObjectType() == ROIObjectType::Line);

		// Line 은 이름 뒤에 측정 길이를 붙인다. 이름이 없으면 길이만 나온다.
		// 그래서 이름이 비어도 라벨을 그린다 — 측정 도구가 만든 선이 그 경우다.
		std::wstring label = roiObject->GetName();

		if (isLine)
		{
			const std::wstring length = FormatLength(roiObject.get());
			label = label.empty() ? length : (label + L"  " + length);
		}

		if (label.empty())
		{
			// 이름이 비면 라벨을 그리지 않는다. 호스트가 ROISet 에 L"" 를
			// 넘겨 끄는 방법이라, 이를 위한 별도 API 를 두지 않는다.
			continue;
		}

		if (!IsVisibleOnClient(roiObject->GetBounds(), visibleRect, 0.0f))
		{
			continue;
		}

		const int32_t rawFontSize = roiObject->GetFontSize();
		const float fontSize = (rawFontSize > 0)
			? static_cast<float>(rawFontSize)
			: kDefaultFontSize;

		// 캐시 조회. 이름이나 글자 크기가 바뀌면 다시 만든다.
		LabelCache& cache = m_labelCache[roiObject.get()];

		if (!cache.layout || cache.name != label || cache.fontSize != rawFontSize)
		{
			SafeRelease(cache.layout);
			cache.width = 0.0f;
			cache.height = 0.0f;

			IDWriteTextFormat* textFormat = fontManager->GetTextFormat(
				L"Segoe UI", fontSize, DWRITE_FONT_WEIGHT_NORMAL);
			if (!textFormat)
			{
				continue;
			}

			IDWriteTextLayout* textLayout = nullptr;
			const HRESULT hr = dwriteFactory->CreateTextLayout(
				label.c_str(), static_cast<UINT32>(label.size()),
				textFormat, FLT_MAX, FLT_MAX, &textLayout);

			if (FAILED(hr) || !textLayout)
			{
				continue;
			}

			textLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			textLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

			// 이름이 길어도 접지 않는다. 접히면 판 높이 계산과 어긋난다.
			textLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

			DWRITE_TEXT_METRICS metrics = {};
			if (FAILED(textLayout->GetMetrics(&metrics)))
			{
				textLayout->Release();
				continue;
			}

			cache.layout = textLayout;
			cache.name = label;
			cache.fontSize = rawFontSize;
			cache.width = metrics.width;
			cache.height = metrics.height;
		}

		IDWriteTextLayout* textLayout = cache.layout;

		const float plateWidth = cache.width + kPaddingX * 2.0f;
		const float plateHeight = cache.height + kPaddingY * 2.0f;

		const Rect2f screenBounds = ToScreenBounds(roiObject->GetBounds());

		// 기본은 도형 바깥 위쪽. 도형을 가리지 않는다.
		float plateLeft = screenBounds.left;
		float plateTop = screenBounds.top - plateHeight - kGap;

		if (isLine)
		{
			// 선은 바운딩 박스 좌상단이 도형과 동떨어진다(대각선이면 허공이다).
			// 길이 라벨은 선 위에 있어야 읽히므로 중점을 기준으로 놓는다.
			ROIShapeData shape = {};
			roiObject->GetShape(shape);

			const Point2f midImage = {
				(shape.u.line.x1 + shape.u.line.x2) * 0.5f,
				(shape.u.line.y1 + shape.u.line.y2) * 0.5f
			};

			const Rect2f midScreen = ToScreenBounds({ midImage.x, midImage.y, midImage.x, midImage.y });

			plateLeft = midScreen.left - plateWidth * 0.5f;
			plateTop = midScreen.top - plateHeight - kGap;
		}

		// 위쪽 공간이 없으면 도형 안쪽 상단으로 뒤집는다.
		// 도형이 뷰 상단에 걸쳐 있을 때 라벨이 잘리는 흔한 경우를 덮는다.
		if (plateTop < visibleRect.top)
		{
			plateTop = screenBounds.top + kGap;
		}

		// 화면 경계로 클램프하지는 않는다.
		//
		// 라벨을 뷰포트 모서리에 붙이면 내장 툴바(좌측)와 상태바(하단) 뒤로
		// 들어간다. UI 레이어가 ROI 레이어보다 뒤에 그려지기 때문인데,
		// 그렇다고 여기서 툴바 크기를 알아내는 것은 레이어 간 결합이다.
		// 라벨은 도형에 붙어 다니게 두고, 도형이 화면 밖으로 스크롤되면
		// 라벨도 같이 나간다. 뷰포트 안쪽 여유 영역을 따로 받아야 풀 수 있는
		// 문제라 지금은 그대로 둔다.

		const D2D1_ROUNDED_RECT plate = {
			{ plateLeft, plateTop, plateLeft + plateWidth, plateTop + plateHeight },
			kCornerRadius,
			kCornerRadius
		};

		// 배경 이미지가 밝은지 어두운지 알 수 없으므로 어두운 판을 깔고
		// 그 위에 ROI 색으로 쓴다. 라벨이 여러 개 겹쳐도 어느 ROI 것인지
		// 색으로 구분된다. 핸들의 흰 채움 + ROI색 테두리와 같은 전략이다.
		m_fillBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.62f));
		m_d2dContext->FillRoundedRectangle(plate, m_fillBrush);

		m_strokeBrush->SetColor(ROIUtilities::ConvertColor(
			static_cast<COLORREF>(roiObject->GetColorRGB())));

		m_d2dContext->DrawTextLayout(
			{ plateLeft + kPaddingX, plateTop + kPaddingY },
			textLayout,
			m_strokeBrush,
			D2D1_DRAW_TEXT_OPTIONS_NONE);
	}
}

const wchar_t* ROIRenderLayer::MeasureKey()
{
	// 밑줄 두 개로 시작해 호스트 키와 부딪히지 않게 한다.
	return L"__measure";
}

void ROIRenderLayer::SetPixelScale(double xScale, double yScale, const wchar_t* unit)
{
	if (xScale <= 0.0 || yScale <= 0.0)
	{
		return;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	m_pixelScaleX = xScale;
	m_pixelScaleY = yScale;
	m_pixelUnit = unit ? unit : L"";

	// 길이 라벨 문자열이 바뀌므로 캐시를 통째로 버린다.
	ReleaseLabelCache();

	::ReleaseSRWLockExclusive(&m_roiLock);
}

std::wstring ROIRenderLayer::FormatLength(const IROIObject* roiObject) const
{
	// 호출자가 m_roiLock 을 쥐고 있다.
	ROIShapeData shape = {};
	roiObject->GetShape(shape);

	const double dx = (static_cast<double>(shape.u.line.x2) - shape.u.line.x1) * m_pixelScaleX;
	const double dy = (static_cast<double>(shape.u.line.y2) - shape.u.line.y1) * m_pixelScaleY;
	const double length = sqrt(dx * dx + dy * dy);

	wchar_t buffer[64] = {};
	swprintf_s(buffer, 64, L"%.2f %s", length, m_pixelUnit.c_str());

	return buffer;
}

void ROIRenderLayer::BeginMeasure()
{
	// 버튼을 누를 때마다 기존 측정선을 지운다(켜든 끄든 리셋).
	::AcquireSRWLockExclusive(&m_roiLock);
	RemoveObjectByKey(MeasureKey());
	m_measureState = MeasureState::Armed;
	::ReleaseSRWLockExclusive(&m_roiLock);
}

void ROIRenderLayer::CancelMeasure()
{
	::AcquireSRWLockExclusive(&m_roiLock);
	RemoveObjectByKey(MeasureKey());
	m_measureState = MeasureState::Off;
	::ReleaseSRWLockExclusive(&m_roiLock);
}

bool ROIRenderLayer::IsMeasureArmed() const
{
	::AcquireSRWLockShared(&m_roiLock);
	const bool armed = (m_measureState == MeasureState::Armed);
	::ReleaseSRWLockShared(&m_roiLock);

	return armed;
}

bool ROIRenderLayer::IsMeasureRubber() const
{
	::AcquireSRWLockShared(&m_roiLock);
	const bool rubber = (m_measureState == MeasureState::Rubber);
	::ReleaseSRWLockShared(&m_roiLock);

	return rubber;
}

bool ROIRenderLayer::MeasureOnClick(float screenX, float screenY, bool& outCompleted)
{
	outCompleted = false;

	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);

	::AcquireSRWLockExclusive(&m_roiLock);

	bool consumed = false;

	if (m_measureState == MeasureState::Armed)
	{
		// 첫 점. 두 끝점이 겹친 선을 만들고 고무줄 단계로 넘어간다.
		RemoveObjectByKey(MeasureKey());

		auto lineObject = std::make_unique<ROILineRenderer>(MeasureKey());
		lineObject->UpdateDefinition(L"",
			Line2f(imagePoint.x, imagePoint.y, imagePoint.x, imagePoint.y),
			RGB(255, 220, 0), true, true, 14);

		IROIObject* raw = lineObject.get();
		m_roiObjects.push_back(std::move(lineObject));

		m_selectedObject = raw;
		m_hoveredObject = raw;
		m_measureState = MeasureState::Rubber;

		QueueEvent(ROIEvent::Selected, raw->GetKey());
		QueueEvent(ROIEvent::EditBegin, raw->GetKey());

		consumed = true;
	}
	else if (m_measureState == MeasureState::Rubber)
	{
		// 두 번째 점. 여기서 확정하고 모드를 내린다.
		if (auto* lineObject = static_cast<ROILineRenderer*>(
			FindObjectByKey(MeasureKey(), ROIObjectType::Line)))
		{
			lineObject->SetEndPoint(imagePoint);
			QueueEvent(ROIEvent::EditEnd, lineObject->GetKey());
		}

		m_measureState = MeasureState::Off;
		outCompleted = true;
		consumed = true;
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	DispatchPendingEvents();

	return consumed;
}

bool ROIRenderLayer::MeasureOnMouseMove(float screenX, float screenY)
{
	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);

	::AcquireSRWLockExclusive(&m_roiLock);

	bool changed = false;

	if (m_measureState == MeasureState::Rubber)
	{
		if (auto* lineObject = static_cast<ROILineRenderer*>(
			FindObjectByKey(MeasureKey(), ROIObjectType::Line)))
		{
			lineObject->SetEndPoint(imagePoint);
			QueueEvent(ROIEvent::EditChanged, lineObject->GetKey());
			changed = true;
		}
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	DispatchPendingEvents();

	return changed;
}

void ROIRenderLayer::InvalidateLabelCache(const IROIObject* roiObject)
{
	auto iterator = m_labelCache.find(roiObject);
	if (iterator == m_labelCache.end())
	{
		return;
	}

	SafeRelease(iterator->second.layout);
	m_labelCache.erase(iterator);
}

void ROIRenderLayer::ReleaseLabelCache()
{
	for (auto& entry : m_labelCache)
	{
		SafeRelease(entry.second.layout);
	}

	m_labelCache.clear();
}

void ROIRenderLayer::OnDeviceLost()
{
	ReleaseDeviceResources();

	// 오브젝트가 캐시한 D2D 지오메트리도 버려야 한다(위 Overlay 와 같은 이유).
	::AcquireSRWLockExclusive(&m_roiLock);
	for (auto& object : m_roiObjects)
	{
		if (object) object->OnDeviceLost();
	}
	::ReleaseSRWLockExclusive(&m_roiLock);
}

void ROIRenderLayer::OnDeviceRestored()
{
	if (!AcquireDeviceResources())
	{
		m_initialized = false;
	}
}

void ROIRenderLayer::SetCamera2D(const Camera2D* camera)
{
	m_camera = camera;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIRectangleRenderer* rectangleObject = static_cast<ROIRectangleRenderer*>(FindObjectByKey(key, ROIObjectType::Rectangle));

	if (!rectangleObject)
	{
		RemoveObjectByKey(key);
		auto newRectangleObject = std::make_unique<ROIRectangleRenderer>(key);
		rectangleObject = newRectangleObject.get();
		m_roiObjects.push_back(std::move(newRectangleObject));
	}

	const bool result = rectangleObject->UpdateDefinition(name, rect, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIEllipseRenderer* ellipseObject = static_cast<ROIEllipseRenderer*>(FindObjectByKey(key, ROIObjectType::Ellipse));

	if (!ellipseObject)
	{
		RemoveObjectByKey(key);
		auto newEllipseObject = std::make_unique<ROIEllipseRenderer>(key);
		ellipseObject = newEllipseObject.get();
		m_roiObjects.push_back(std::move(newEllipseObject));
	}

	const bool result = ellipseObject->UpdateDefinition(name, ellipse, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROICircleRenderer* circleObject = static_cast<ROICircleRenderer*>(FindObjectByKey(key, ROIObjectType::Circle));

	if (!circleObject)
	{
		RemoveObjectByKey(key);
		auto newCircleObject = std::make_unique<ROICircleRenderer>(key);
		circleObject = newCircleObject.get();
		m_roiObjects.push_back(std::move(newCircleObject));
	}

	const bool result = circleObject->UpdateDefinition(name, circle, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (!key || key[0] == L'\0' || !polygon.IsValid())
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIPolygonRenderer* polygonObject = static_cast<ROIPolygonRenderer*>(FindObjectByKey(key, ROIObjectType::Polygon));

	if (!polygonObject)
	{
		RemoveObjectByKey(key);
		auto newPolygonObject = std::make_unique<ROIPolygonRenderer>(key);
		polygonObject = newPolygonObject.get();
		m_roiObjects.push_back(std::move(newPolygonObject));
	}

	const bool result = polygonObject->UpdateDefinition(name, polygon, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

bool ROIRenderLayer::ROISet(const wchar_t* key, const wchar_t* name, const Line2f& line, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize)
{
	if (!key || key[0] == L'\0')
	{
		return false;
	}

	::AcquireSRWLockExclusive(&m_roiLock);

	ROILineRenderer* lineObject = static_cast<ROILineRenderer*>(FindObjectByKey(key, ROIObjectType::Line));

	if (!lineObject)
	{
		RemoveObjectByKey(key);
		auto newLineObject = std::make_unique<ROILineRenderer>(key);
		lineObject = newLineObject.get();
		m_roiObjects.push_back(std::move(newLineObject));
	}

	const bool result = lineObject->UpdateDefinition(name, line, rgb, isMovable, isResizable, fontSize);
	::ReleaseSRWLockExclusive(&m_roiLock);

	return result;
}

void ROIRenderLayer::ROIClear()
{
	::AcquireSRWLockExclusive(&m_roiLock);
	ReleaseLabelCache();
	m_roiObjects.clear();
	m_hoveredObject = nullptr;
	m_selectedObject = nullptr;
	m_activeObject = nullptr;
	m_activeHit = {};
	m_isDragging = false;
	::ReleaseSRWLockExclusive(&m_roiLock);
}

bool ROIRenderLayer::OnLButtonDown(float screenX, float screenY)
{
	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);
	const float tolerance = GetHitToleranceInImage();

	ROIHitResult hitResult = {};

	::AcquireSRWLockExclusive(&m_roiLock);
	IROIObject* hitObject = HitTest(imagePoint, tolerance, hitResult);

	// 선택이 바뀌었는지 먼저 판단해야 이벤트를 중복으로 내지 않는다.
	IROIObject* previousSelection = m_selectedObject;

	m_selectedObject = hitObject;
	m_activeObject = hitObject;
	m_activeHit = hitResult;
	m_isDragging = hitObject != nullptr;
	m_hoveredObject = hitObject;

	if (m_activeObject)
	{
		m_activeObject->BeginDrag(imagePoint, hitResult);
	}

	if (previousSelection != hitObject)
	{
		if (previousSelection)
		{
			QueueEvent(ROIEvent::Deselected, previousSelection->GetKey());
		}
		if (hitObject)
		{
			QueueEvent(ROIEvent::Selected, hitObject->GetKey());
		}
	}

	if (hitObject)
	{
		QueueEvent(ROIEvent::EditBegin, hitObject->GetKey());
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	// 반드시 락 밖에서 낸다. 호스트가 콜백에서 ROI API 를 불러도 안전하다.
	DispatchPendingEvents();

	return hitObject != nullptr;
}

bool ROIRenderLayer::OnMouseMove(float screenX, float screenY)
{
	if (!m_camera)
	{
		return false;
	}

	const Point2f imagePoint = ScreenToImage(screenX, screenY);
	bool changed = false;

	::AcquireSRWLockExclusive(&m_roiLock);

	if (m_isDragging && m_activeObject)
	{
		m_activeObject->UpdateDrag(imagePoint);
		changed = true;

		// 드래그 중 형상 변화. 프레임마다가 아니라 이동이 있을 때만 나간다.
		QueueEvent(ROIEvent::EditChanged, m_activeObject->GetKey());
	}
	else
	{
		ROIHitResult hitResult = {};
		IROIObject* hoveredObject = HitTest(imagePoint, GetHitToleranceInImage(), hitResult);
		changed = UpdateHoverObject(hoveredObject);
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	DispatchPendingEvents();

	return changed;
}

bool ROIRenderLayer::OnLButtonUp(float screenX, float screenY)
{
	UNREFERENCED_PARAMETER(screenX);
	UNREFERENCED_PARAMETER(screenY);

	bool changed = false;

	::AcquireSRWLockExclusive(&m_roiLock);

	if (m_activeObject)
	{
		m_activeObject->EndDrag();

		// 결과 확정 지점. 호스트는 보통 여기서 새 형상을 저장한다.
		QueueEvent(ROIEvent::EditEnd, m_activeObject->GetKey());

		m_activeObject = nullptr;
		m_activeHit = {};
		changed = true;
	}

	m_isDragging = false;

	::ReleaseSRWLockExclusive(&m_roiLock);

	DispatchPendingEvents();

	return changed;
}

bool ROIRenderLayer::AcquireDeviceResources()
{
	m_d2dContext = m_context ? m_context->GetD2DDeviceContext() : nullptr;
	if (!m_d2dContext)
	{
		return false;
	}

	// 이전 리소스를 먼저 놓고 나서 새로 얻는다.
	// 순서가 뒤바뀌면 방금 AddRef 한 팩토리를 그대로 놓아버린다.
	ReleaseDeviceResources();

	m_d2dContext->GetFactory(&m_d2dFactory);
	if (!m_d2dFactory)
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_strokeBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_fillBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_handleFillBrush)))
	{
		return false;
	}

	if (FAILED(m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_handleOutlineBrush)))
	{
		return false;
	}

	return true;
}

void ROIRenderLayer::ReleaseDeviceResources()
{
	SafeRelease(m_strokeBrush);
	SafeRelease(m_fillBrush);
	SafeRelease(m_handleFillBrush);
	SafeRelease(m_handleOutlineBrush);

	// GetFactory() 가 AddRef 하므로 여기서 놓아준다.
	SafeRelease(m_d2dFactory);
}

Rect2f ROIRenderLayer::GetVisibleClientRect() const
{
	if (!m_context)
	{
		return {};
	}

	return {
		0.0f,
		0.0f,
		static_cast<float>(m_context->GetWidth()),
		static_cast<float>(m_context->GetHeight())
	};
}

Rect2f ROIRenderLayer::ToScreenBounds(const Rect2f& bounds) const
{
	if (!m_camera)
	{
		return {};
	}

	const float zoom = max(m_camera->GetZoom(), 0.0001f);
	const float offsetX = m_camera->GetOffsetX();
	const float offsetY = m_camera->GetOffsetY();

	Rect2f screenBounds = {
		(bounds.left - offsetX) * zoom,
		(bounds.top - offsetY) * zoom,
		(bounds.right - offsetX) * zoom,
		(bounds.bottom - offsetY) * zoom
	};
	screenBounds.Normalize();
	return screenBounds;
}

bool ROIRenderLayer::IsVisibleOnClient(const Rect2f& bounds, const Rect2f& visibleRect, float padding) const
{
	Rect2f screenBounds = ToScreenBounds(bounds);
	screenBounds.Inflate(padding, padding);
	return screenBounds.Intersects(visibleRect);
}

Point2f ROIRenderLayer::ScreenToImage(float screenX, float screenY) const
{
	Point2f imagePoint = {};
	imagePoint.x = m_camera->GetOffsetX() + screenX / max(m_camera->GetZoom(), 0.0001f);
	imagePoint.y = m_camera->GetOffsetY() + screenY / max(m_camera->GetZoom(), 0.0001f);
	return imagePoint;
}

float ROIRenderLayer::GetHitToleranceInImage() const
{
	if (!m_camera)
	{
		return 6.0f;
	}

	return 6.0f / max(m_camera->GetZoom(), 0.0001f);
}

IROIObject* ROIRenderLayer::FindObjectByKey(const wchar_t* key) const
{
	if (!key)
	{
		return nullptr;
	}

	for (const auto& roiObject : m_roiObjects)
	{
		if (roiObject->GetKey() == key)
		{
			return roiObject.get();
		}
	}

	return nullptr;
}

IROIObject* ROIRenderLayer::FindObjectByKey(const wchar_t* key, ROIObjectType objectType) const
{
	IROIObject* roiObject = FindObjectByKey(key);
	if (!roiObject || roiObject->GetObjectType() != objectType)
	{
		return nullptr;
	}

	return roiObject;
}

void ROIRenderLayer::RemoveObjectByKey(const wchar_t* key)
{
	if (!key)
	{
		return;
	}

	for (auto iterator = m_roiObjects.begin(); iterator != m_roiObjects.end(); ++iterator)
	{
		if ((*iterator)->GetKey() != key)
		{
			continue;
		}

		IROIObject* roiObject = iterator->get();
		if (m_hoveredObject == roiObject)
		{
			m_hoveredObject = nullptr;
		}
		if (m_selectedObject == roiObject)
		{
			m_selectedObject = nullptr;
		}
		if (m_activeObject == roiObject)
		{
			m_activeObject = nullptr;
			m_activeHit = {};
			m_isDragging = false;
		}

		// 캐시는 객체 주소가 키다. 같은 주소에 새 객체가 잡히기 전에 지운다.
		InvalidateLabelCache(roiObject);

		m_roiObjects.erase(iterator);
		return;
	}
}

IROIObject* ROIRenderLayer::HitTest(const Point2f& imagePoint, float tolerance, ROIHitResult& hitResult) const
{
	IROIObject* hitObject = nullptr;
	float minDistance = FLT_MAX;

	for (auto iterator = m_roiObjects.rbegin(); iterator != m_roiObjects.rend(); ++iterator)
	{
		const Rect2f& bounds = (*iterator)->GetBounds();
		Rect2f expandedBounds = bounds;
		expandedBounds.Inflate(tolerance, tolerance);

		if (!expandedBounds.Contains(imagePoint))
		{
			continue;
		}

		ROIHitResult currentHit = (*iterator)->HitTest(imagePoint, tolerance);
		if (!currentHit.IsHit())
		{
			continue;
		}

		if (currentHit.distance < minDistance)
		{
			minDistance = currentHit.distance;
			hitResult = currentHit;
			hitObject = iterator->get();

			if (currentHit.type != ROIHitType::Body)
			{
				break;
			}
		}
	}

	return hitObject;
}

bool ROIRenderLayer::UpdateHoverObject(IROIObject* hoveredObject)
{
	if (m_hoveredObject == hoveredObject)
	{
		return false;
	}

	m_hoveredObject = hoveredObject;
	return true;
}





/*---------------------------------------------------------
	조회
---------------------------------------------------------*/
uint32_t ROIRenderLayer::CopyString(const std::wstring& source, wchar_t* buffer, uint32_t bufferChars)
{
	// 반환값은 종료 널을 포함한 필요 문자 수. 2회 호출 패턴용.
	const uint32_t needed = static_cast<uint32_t>(source.size()) + 1;

	if (!buffer || bufferChars < needed)
		return needed;

	::wcscpy_s(buffer, bufferChars, source.c_str());

	return needed;
}

uint32_t ROIRenderLayer::ROIGetCount() const
{
	::AcquireSRWLockShared(&m_roiLock);
	const uint32_t count = static_cast<uint32_t>(m_roiObjects.size());
	::ReleaseSRWLockShared(&m_roiLock);

	return count;
}

bool ROIRenderLayer::ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const
{
	if (!key)
		return false;

	::AcquireSRWLockShared(&m_roiLock);

	const IROIObject* object = FindObjectByKey(key);
	const bool found = (object != nullptr);
	if (found)
	{
		object->GetShape(outShape);
	}

	::ReleaseSRWLockShared(&m_roiLock);

	return found;
}

uint32_t ROIRenderLayer::ROIGetVertices(const wchar_t* key,	Core::ShapeType::Point2f* buffer, uint32_t capacity,
	uint32_t segmentsPerCurve) const
{
	if (!key)
		return 0;

	::AcquireSRWLockShared(&m_roiLock);

	const IROIObject* object = FindObjectByKey(key);
	const uint32_t needed = object
		? object->GetVertices(buffer, capacity, segmentsPerCurve)
		: 0u;

	::ReleaseSRWLockShared(&m_roiLock);

	return needed;
}

bool ROIRenderLayer::ROIGetBounds(const wchar_t* key, Core::ShapeType::Rect2f& outBounds) const
{
	if (!key)
		return false;

	::AcquireSRWLockShared(&m_roiLock);

	const IROIObject* object = FindObjectByKey(key);
	const bool found = (object != nullptr);
	if (found)
	{
		outBounds = object->GetBounds();
	}

	::ReleaseSRWLockShared(&m_roiLock);

	return found;
}

bool ROIRenderLayer::ROIGetInfo(const wchar_t* key, ROIInfoData& outInfo) const
{
	if (!key)
		return false;

	::AcquireSRWLockShared(&m_roiLock);

	const IROIObject* object = FindObjectByKey(key);
	const bool found = (object != nullptr);
	if (found)
	{
		outInfo.type = object->GetObjectType();
		outInfo.colorRGB = object->GetColorRGB();
		outInfo.isMovable = object->IsMovable();
		outInfo.isResizable = object->IsResizable();
		outInfo.isSelected = (object == m_selectedObject);
		outInfo.isHovered = (object == m_hoveredObject);
		outInfo.fontSize = object->GetFontSize();
	}

	::ReleaseSRWLockShared(&m_roiLock);

	return found;
}

uint32_t ROIRenderLayer::ROIGetName(const wchar_t* key,	wchar_t* buffer, uint32_t bufferChars) const
{
	if (!key)
		return 0;

	::AcquireSRWLockShared(&m_roiLock);

	const IROIObject* object = FindObjectByKey(key);
	const uint32_t needed = object
		? CopyString(object->GetName(), buffer, bufferChars)
		: 0u;

	::ReleaseSRWLockShared(&m_roiLock);

	return needed;
}

uint32_t ROIRenderLayer::ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const
{
	::AcquireSRWLockShared(&m_roiLock);

	uint32_t needed = 0;
	if (index < m_roiObjects.size() && m_roiObjects[index])
	{
		needed = CopyString(m_roiObjects[index]->GetKey(), buffer, bufferChars);
	}

	::ReleaseSRWLockShared(&m_roiLock);

	return needed;
}

uint32_t ROIRenderLayer::ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const
{
	::AcquireSRWLockShared(&m_roiLock);

	const uint32_t needed = m_selectedObject
		? CopyString(m_selectedObject->GetKey(), buffer, bufferChars)
		: 0u;

	::ReleaseSRWLockShared(&m_roiLock);

	return needed;
}

uint32_t ROIRenderLayer::ROIHitTestKey(float imageX, float imageY, float tolerance,	wchar_t* buffer, uint32_t bufferChars) const
{
	const Core::ShapeType::Point2f imagePoint{ imageX, imageY };

	// tolerance <= 0 이면 현재 배율 기준 기본값을 쓴다.
	const float actualTolerance = (tolerance > 0.0f)
		? tolerance
		: GetHitToleranceInImage();

	::AcquireSRWLockShared(&m_roiLock);

	ROIHitResult hitResult = {};
	const IROIObject* object = HitTest(imagePoint, actualTolerance, hitResult);
	const uint32_t needed = object
		? CopyString(object->GetKey(), buffer, bufferChars)
		: 0u;

	::ReleaseSRWLockShared(&m_roiLock);

	return needed;
}

bool ROIRenderLayer::ROIRemove(const wchar_t* key)
{
    if (!key)
        return false;

    ::AcquireSRWLockExclusive(&m_roiLock);

    const bool found = (FindObjectByKey(key) != nullptr);
    if (found)
    {
        RemoveObjectByKey(key);
    }

    ::ReleaseSRWLockExclusive(&m_roiLock);

    return found;
}

/*---------------------------------------------------------
	이벤트
---------------------------------------------------------*/
void ROIRenderLayer::SetROIEventHandler(ROIEventHandler handler, void* userData)
{
	::AcquireSRWLockExclusive(&m_roiLock);
	m_eventHandler = handler;
	m_eventUserData = userData;
	::ReleaseSRWLockExclusive(&m_roiLock);
}

void ROIRenderLayer::QueueEvent(ROIEvent event, const std::wstring& key)
{
	// 락 안에서 호출된다. 콜백을 여기서 부르면 호스트가 콜백에서 ROI API 를
	// 호출하는 순간 자기 자신을 기다린다.
	if (!m_eventHandler)
		return;

	m_pendingEvents.push_back(PendingEvent{ event, key });
}

void ROIRenderLayer::DispatchPendingEvents()
{
	// 락 밖에서 호출된다. 큐를 먼저 옮겨 담고 나서 콜백을 낸다.
	// (콜백이 다시 ROI API 를 불러 큐를 건드릴 수 있으므로)
	std::vector<PendingEvent> events;
	ROIEventHandler handler = nullptr;
	void* userData = nullptr;

	::AcquireSRWLockExclusive(&m_roiLock);
	if (!m_pendingEvents.empty())
	{
		events.swap(m_pendingEvents);
		handler = m_eventHandler;
		userData = m_eventUserData;
	}
	::ReleaseSRWLockExclusive(&m_roiLock);

	if (!handler)
		return;

	for (const PendingEvent& pending : events)
	{
		handler(pending.event, pending.key.c_str(), userData);
	}
}

bool ROIRenderLayer::OnLButtonDoubleClick(float screenX, float screenY)
{
	if (!m_camera)
		return false;

	const Core::ShapeType::Point2f imagePoint = ScreenToImage(screenX, screenY);

	::AcquireSRWLockExclusive(&m_roiLock);

	ROIHitResult hitResult = {};
	const IROIObject* object = HitTest(imagePoint, GetHitToleranceInImage(), hitResult);
	if (object)
	{
		QueueEvent(ROIEvent::DoubleClicked, object->GetKey());
	}

	::ReleaseSRWLockExclusive(&m_roiLock);

	DispatchPendingEvents();

	return object != nullptr;
}
