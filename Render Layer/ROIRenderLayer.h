#pragma once

#include "../../../Module/D3D11EngineInterface/IRenderLayer.h"
#include "../../../Module/D3D11EngineInterface/IDeviceEventListener.h"

#include "../ROI Renderer/IROIObject.h"

#include <string>
#include <vector>
#include <unordered_map>

#include "../../../Module/Core/ShapeType/Circle2f.h"
#include "../../../Module/Core/ShapeType/Ellipse2f.h"
#include "../../../Module/Core/ShapeType/Point2f.h"
#include "../../../Module/Core/ShapeType/Polygon2f.h"
#include "../../../Module/Core/ShapeType/Rect2f.h"

#include <memory>

class Camera2D;
class IRenderContext;
struct ID2D1DeviceContext;
struct ID2D1Factory;
struct ID2D1SolidColorBrush;

class ROIRenderLayer
	: public IRenderLayer
	, public IDeviceEventListener
{
public:
	ROIRenderLayer();
	virtual ~ROIRenderLayer();

	// IRenderLayer override
	bool Initialize(IRenderContext* context) override;
	void Shutdown() override;

	bool Prepare() override;
	bool Render() override;

	// IDeviceEventListener override
	void OnDeviceLost() override;
	void OnDeviceRestored() override;

public:
	void SetCamera2D(const Camera2D* camera);

	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Rect2f& rect, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Ellipse2f& ellipse, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Circle2f& circle, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	bool ROISet(const wchar_t* key, const wchar_t* name, const Core::ShapeType::Polygon2f& polygon, COLORREF rgb, bool isMovable, bool isResizable, int32_t fontSize);
	void ROIClear();
	bool ROIRemove(const wchar_t* key);

	// ── 조회
	//
	// 전부 m_roiLock 을 shared 로 잡고 값을 복사해 나간다. 호출자에게
	// 내부 포인터를 넘기지 않는다(DLL 경계를 넘을 수 있으므로).
	uint32_t ROIGetCount() const;
	bool ROIGetShape(const wchar_t* key, ROIShapeData& outShape) const;
	uint32_t ROIGetVertices(const wchar_t* key, Core::ShapeType::Point2f* buffer,
		uint32_t capacity, uint32_t segmentsPerCurve) const;
	bool ROIGetBounds(const wchar_t* key, Core::ShapeType::Rect2f& outBounds) const;

	struct ROIInfoData
	{
		ROIObjectType type = ROIObjectType::Rectangle;
		uint32_t colorRGB = 0;
		bool isMovable = false;
		bool isResizable = false;
		bool isSelected = false;
		bool isHovered = false;
		int32_t fontSize = 0;
	};
	bool ROIGetInfo(const wchar_t* key, ROIInfoData& outInfo) const;

	// 문자열은 호출자 버퍼로만 나간다. 반환값은 종료 널을 포함한 필요 문자 수.
	uint32_t ROIGetName(const wchar_t* key, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetKeyAt(uint32_t index, wchar_t* buffer, uint32_t bufferChars) const;
	uint32_t ROIGetSelectedKey(wchar_t* buffer, uint32_t bufferChars) const;

	// 이미지 좌표 히트 테스트. 맞은 ROI 의 키를 채운다. 없으면 0 반환.
	uint32_t ROIHitTestKey(float imageX, float imageY, float tolerance,
		wchar_t* buffer, uint32_t bufferChars) const;

	// ── 이벤트
	//
	// 콜백은 반드시 m_roiLock 밖에서 호출해야 한다. 호스트가 콜백에서
	// ROISet/ROIGetShape 를 부르면 자기 자신을 기다리게 되기 때문이다.
	// 그래서 마우스 핸들러가 큐에 쌓고 락을 푼 뒤 DispatchPendingEvents 가 낸다.
	enum class ROIEvent : uint32_t
	{
		Selected = 0,
		Deselected,
		EditBegin,
		EditChanged,
		EditEnd,
		DoubleClicked
	};

	using ROIEventHandler = void (*)(ROIEvent event, const wchar_t* key, void* userData);
	void SetROIEventHandler(ROIEventHandler handler, void* userData);

	bool OnLButtonDown(float screenX, float screenY);
	bool OnMouseMove(float screenX, float screenY);
	bool OnLButtonUp(float screenX, float screenY);
	bool OnLButtonDoubleClick(float screenX, float screenY);

private:
	bool AcquireDeviceResources();
	void ReleaseDeviceResources();

	Core::ShapeType::Rect2f GetVisibleClientRect() const;
	Core::ShapeType::Rect2f ToScreenBounds(const Core::ShapeType::Rect2f& bounds) const;
	bool IsVisibleOnClient(const Core::ShapeType::Rect2f& bounds, const Core::ShapeType::Rect2f& visibleRect, float padding) const;

	// 이름 라벨. 도형과 달리 화면 좌표계에서 그린다.
	//
	// 도형 패스는 이미지 좌표계 변환(Scale(zoom) * Translate) 아래에서 그리는데,
	// 그 상태로 텍스트를 그리면 배율에 따라 글자 크기가 같이 변한다. fontSize 를
	// zoom 으로 나눠 보정하는 방법도 있지만, 고배율에서 폰트 크기가 1 이하로
	// 내려가 글리프가 뭉개진다. 그래서 변환을 되돌리고 앵커만 화면 좌표로
	// 옮겨서 실제 픽셀 크기로 그린다. fontSize 는 화면 픽셀 단위다.
	void RenderNameLabels(const Core::ShapeType::Rect2f& visibleRect);

	Core::ShapeType::Point2f ScreenToImage(float screenX, float screenY) const;
	float GetHitToleranceInImage() const;

	IROIObject* FindObjectByKey(const wchar_t* key) const;
	IROIObject* FindObjectByKey(const wchar_t* key, ROIObjectType objectType) const;
	void RemoveObjectByKey(const wchar_t* key);
	IROIObject* HitTest(const Core::ShapeType::Point2f& imagePoint, float tolerance, ROIHitResult& hitResult) const;
	bool UpdateHoverObject(IROIObject* hoveredObject);

	// 라벨 텍스트 레이아웃 캐시.
	//
	// CreateTextLayout 은 문자 분류/셰이핑/줄바꿈을 실제로 수행해서 라벨 하나당
	// 약 12us 든다(D2D 프리미티브 드로우 420ns 의 28배). ROI 100개면 프레임당
	// 0.95ms 로 240fps 예산의 23% 다. 캐시하면 50배 빨라진다.
	//
	// IDWriteTextLayout 은 DWrite 객체라 D3D 디바이스와 무관하다. 디바이스
	// 로스트에도 살아남으므로 OnDeviceLost 에서 버릴 필요가 없다.
	void InvalidateLabelCache(const IROIObject* roiObject);
	void ReleaseLabelCache();

	// 락 안에서 호출한다. 큐에만 쌓는다.
	void QueueEvent(ROIEvent event, const std::wstring& key);

	// 락을 푼 뒤 호출한다. 큐를 비우면서 콜백을 낸다.
	void DispatchPendingEvents();

	// 문자열 복사 공통 처리. 반환값은 종료 널 포함 필요 문자 수.
	static uint32_t CopyString(const std::wstring& source,
		wchar_t* buffer, uint32_t bufferChars);

private:
	IRenderContext* m_context = nullptr;
	ID2D1DeviceContext* m_d2dContext = nullptr;

	// 경로 지오메트리 생성용. 예전에는 ROIPolygonRenderer 가 매 프레임
	// d2dContext->GetFactory() 로 얻어 쓰고 Release 했다.
	ID2D1Factory* m_d2dFactory = nullptr;
	const Camera2D* m_camera = nullptr;

	ID2D1SolidColorBrush* m_strokeBrush = nullptr;
	ID2D1SolidColorBrush* m_fillBrush = nullptr;
	ID2D1SolidColorBrush* m_handleFillBrush = nullptr;
	ID2D1SolidColorBrush* m_handleOutlineBrush = nullptr;

	std::vector<std::unique_ptr<IROIObject>> m_roiObjects;
	IROIObject* m_hoveredObject = nullptr;
	IROIObject* m_selectedObject = nullptr;
	IROIObject* m_activeObject = nullptr;
	ROIHitResult m_activeHit = {};
	bool m_isDragging = false;

	// const 조회 메서드도 락을 잡아야 하므로 mutable.
	mutable SRWLOCK m_roiLock = SRWLOCK_INIT;
	bool m_initialized = false;

	// 이벤트 통지. 큐는 락 안에서 채우고 락 밖에서 비운다.
	ROIEventHandler m_eventHandler = nullptr;
	void* m_eventUserData = nullptr;

	struct PendingEvent
	{
		ROIEvent event = ROIEvent::Selected;
		std::wstring key;
	};
	std::vector<PendingEvent> m_pendingEvents;

	// 라벨 캐시. 키는 ROI 객체 주소다.
	//
	// 소유권이 m_roiObjects 에 있으므로 객체가 사라질 때 반드시 같이 지운다.
	// 안 지우면 같은 주소에 새 객체가 잡혔을 때 옛 라벨이 붙는다(ABA).
	// 지우는 곳은 RemoveObjectByKey / ROIClear / Shutdown 세 군데다.
	//
	// 스레드: 렌더 스레드만 채우고 읽는다(shared 락 안). 지우는 쪽은 전부
	// exclusive 락이라 렌더와 배타적이다.
	struct LabelCache
	{
		std::wstring name;
		int32_t fontSize = 0;
		IDWriteTextLayout* layout = nullptr;
		float width = 0.0f;
		float height = 0.0f;
	};
	std::unordered_map<const IROIObject*, LabelCache> m_labelCache;
};




