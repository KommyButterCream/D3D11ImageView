// D3D11ImageView — C# 바인딩
//
// D3D11ImageViewC.h 의 flat C ABI 를 1:1 로 P/Invoke 한다.
// C++ 클래스(D3D11ImageView.h)는 맹글링된 심볼과 C++ 타입을 쓰므로
// P/Invoke 로 부를 수 없다. 반드시 C ABI 쪽을 쓴다.
//
// 전제
//   - x64 전용. 프로젝트 플랫폼을 AnyCPU 로 두면 32bit 로 떨어져
//     BadImageFormatException 이 난다. x64 로 고정할 것.
//   - D3D11ImageView.dll 이 실행 파일과 같은 폴더에 있어야 한다.
//   - 모든 콜백은 뷰어의 UI 스레드(WndProc)에서 온다. 그 스레드가
//     WPF/WinForms 디스패처 스레드이므로 핸들러에서 컨트롤을 직접 만져도 된다.
//
// ★ 델리게이트 수명
//   네이티브에 넘긴 델리게이트는 GC 루트가 아니다. 필드로 잡아두지 않으면
//   수거된 뒤 다음 콜백에서 프로세스가 죽는다. 이 클래스가 필드로 붙든다.
//
// ★ 이미지 버퍼 수명
//   UpdateImage 는 버퍼를 복사하지 않고 빌려 쓴다. 렌더 스레드가 비동기로
//   읽으므로 GCHandle.Alloc(..., Pinned) 로 고정해야 하고, 해제 전에
//   DetachImage 를 불러야 한다. 이 클래스가 둘 다 처리한다.

using System;
using System.Runtime.InteropServices;

namespace D3D11ImageViewInterop
{
    #region 열거형

    public enum D3IVResult
    {
        Ok = 0,
        InvalidArg,
        InvalidHandle,
        NotFound,
        BufferTooSmall,
        Unsupported,
        NotInitialized,
        Failed
    }

    public enum PixelFormat
    {
        Gray8 = 0,
        Gray16,
        Bgr8,
        Bgra8
    }

    public enum ShapeType
    {
        Point2i = 0, Point2f, Point2d,
        Line2i, Line2f, Line2d,
        Rect2i, Rect2f, Rect2d,
        QuadRect2i, QuadRect2f, QuadRect2d,
        RotatedRect2i, RotatedRect2f, RotatedRect2d,
        Circle2f, Circle2d,
        Ellipse2f, Ellipse2d,
        Polyline2f, Polygon2f
    }

    public enum RoiShapeType
    {
        Rectangle = 0,
        Ellipse,
        Circle,
        Polygon,
        Line
    }

    public enum MouseEventType
    {
        Move = 0,
        LButtonDown,
        LButtonUp,
        LButtonDoubleClick,
        RButtonDown,
        RButtonUp,
        MButtonDown,
        MButtonUp,
        Wheel,
        Leave
    }

    public enum RoiEventType
    {
        Selected = 0,
        Deselected,
        EditBegin,
        EditChanged,
        EditEnd,
        DoubleClicked
    }

    [Flags]
    public enum MouseModifiers
    {
        None = 0,
        Ctrl = 0x0001,
        Shift = 0x0002,
        Alt = 0x0004
    }

    [Flags]
    public enum MouseButtons
    {
        None = 0,
        Left = 0x0001,
        Right = 0x0002,
        Middle = 0x0004
    }

    #endregion

    #region POD 구조체
    //
    // C 쪽 D3IV_* 미러. 전부 blittable 이라 마샬링 비용이 없다.
    // 필드 순서/타입을 바꾸면 즉시 깨진다.

    [StructLayout(LayoutKind.Sequential)]
    public struct Point2i
    {
        public int X;
        public int Y;

        public Point2i(int x, int y) { X = x; Y = y; }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point2f
    {
        public float X;
        public float Y;

        public Point2f(float x, float y) { X = x; Y = y; }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point2d
    {
        public double X;
        public double Y;

        public Point2d(double x, double y) { X = x; Y = y; }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2i
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;

        public Rect2i(int left, int top, int right, int bottom)
        {
            Left = left; Top = top; Right = right; Bottom = bottom;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2f
    {
        public float Left;
        public float Top;
        public float Right;
        public float Bottom;

        public Rect2f(float left, float top, float right, float bottom)
        {
            Left = left; Top = top; Right = right; Bottom = bottom;
        }

        public float Width { get { return Right - Left; } }
        public float Height { get { return Bottom - Top; } }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect2d
    {
        public double Left;
        public double Top;
        public double Right;
        public double Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Line2f
    {
        public float X1;
        public float Y1;
        public float X2;
        public float Y2;

        public Line2f(float x1, float y1, float x2, float y2)
        {
            X1 = x1; Y1 = y1; X2 = x2; Y2 = y2;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Circle2f
    {
        public float CenterX;
        public float CenterY;
        public float Radius;

        public Circle2f(float cx, float cy, float radius)
        {
            CenterX = cx; CenterY = cy; Radius = radius;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Ellipse2f
    {
        public float CenterX;
        public float CenterY;
        public float RadiusX;
        public float RadiusY;
        public float AngleRad;

        public Ellipse2f(float cx, float cy, float rx, float ry, float angleRad)
        {
            CenterX = cx; CenterY = cy; RadiusX = rx; RadiusY = ry; AngleRad = angleRad;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ColorRgba8
    {
        public byte R;
        public byte G;
        public byte B;
        public byte A;

        public ColorRgba8(byte r, byte g, byte b, byte a)
        {
            R = r; G = g; B = b; A = a;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OverlayStyle
    {
        public uint StructSize;
        public ColorRgba8 FillColor;
        public ColorRgba8 StrokeColor;
        public float StrokeWidth;
        public int TransparentFill;   // 0 / 1

        public static OverlayStyle Create(ColorRgba8 stroke, float strokeWidth,
            ColorRgba8 fill, bool transparentFill)
        {
            OverlayStyle style = new OverlayStyle();
            style.StructSize = (uint)Marshal.SizeOf(typeof(OverlayStyle));
            style.StrokeColor = stroke;
            style.StrokeWidth = strokeWidth;
            style.FillColor = fill;
            style.TransparentFill = transparentFill ? 1 : 0;

            return style;
        }

        // 테두리만 그리는 흔한 경우.
        public static OverlayStyle Outline(byte r, byte g, byte b, float strokeWidth)
        {
            return Create(new ColorRgba8(r, g, b, 255), strokeWidth,
                new ColorRgba8(0, 0, 0, 0), true);
        }
    }

    // C 쪽 union 을 담기 위한 평면 배치. 직접 쓰기보다 RoiShape 를 쓴다.
    [StructLayout(LayoutKind.Sequential)]
    internal struct RoiShapeNative
    {
        public uint StructSize;
        public int Type;
        public uint VertexCount;

        // union { rect{l,t,r,b} | ellipse{cx,cy,rx,ry,ang} | circle{cx,cy,r} }
        public float F0;
        public float F1;
        public float F2;
        public float F3;
        public float F4;
    }

    // 해석적 형상. Type 에 해당하는 필드만 유효하다.
    public struct RoiShape
    {
        public RoiShapeType Type;
        public uint VertexCount;    // Polygon 일 때만 유효
        public Rect2f Rect;         // Type == Rectangle
        public Ellipse2f Ellipse;   // Type == Ellipse
        public Circle2f Circle;     // Type == Circle
        public Line2f Line;         // Type == Line
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct RoiInfoNative
    {
        public uint StructSize;
        public int Type;
        public uint ColorRgb;
        public int IsMovable;
        public int IsResizable;
        public int IsSelected;
        public int IsHovered;
        public int FontSize;
    }

    public struct RoiInfo
    {
        public RoiShapeType Type;
        public uint ColorRgb;       // COLORREF 0x00BBGGRR
        public bool IsMovable;
        public bool IsResizable;
        public bool IsSelected;
        public bool IsHovered;
        public int FontSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MouseEventNative
    {
        public uint StructSize;
        public int Type;
        public int ScreenX;
        public int ScreenY;
        public float ImageX;
        public float ImageY;
        public int IsInsideImage;
        public int WheelDelta;
        public int Modifiers;
        public int Buttons;
    }

    #endregion

    #region 이벤트 인자

    public class ViewerMouseEventArgs : EventArgs
    {
        public MouseEventType Type { get; internal set; }

        // 뷰어 클라이언트 좌표, 물리 픽셀. WPF DIP 로 쓰려면 DPI 배율로 나눈다.
        public int ScreenX { get; internal set; }
        public int ScreenY { get; internal set; }

        // IsInsideImage == false 면 이미지 밖이라 외삽된 값이다.
        public float ImageX { get; internal set; }
        public float ImageY { get; internal set; }
        public bool IsInsideImage { get; internal set; }

        public int WheelDelta { get; internal set; }
        public MouseModifiers Modifiers { get; internal set; }
        public MouseButtons Buttons { get; internal set; }

        // true 로 두면 뷰어는 이 이벤트를 처리하지 않는다.
        // 팬/줌/ROI 편집을 모두 건너뛴다.
        public bool Handled { get; set; }
    }

    public class RoiEventArgs : EventArgs
    {
        public RoiEventType Type { get; internal set; }
        public string Key { get; internal set; }
    }

    #endregion

    public class D3IVException : Exception
    {
        public D3IVResult Result { get; private set; }

        public D3IVException(D3IVResult result, string operation)
            : base(string.Format("{0} 실패: {1}", operation, result))
        {
            Result = result;
        }
    }

    /// <summary>
    /// D3D11ImageView 네이티브 뷰어 래퍼.
    ///
    /// Win32/WinForms 는 이 클래스를 직접 쓰고, WPF 는 ImageViewHost 를 쓴다.
    /// </summary>
    public sealed class D3D11ImageView : IDisposable
    {
        #region P/Invoke

        private const string Dll = "D3D11ImageView.dll";
        private const CallingConvention Conv = CallingConvention.StdCall;

        [UnmanagedFunctionPointer(Conv)]
        private delegate int NativeMouseCallback(ref MouseEventNative e, IntPtr userData);

        [UnmanagedFunctionPointer(Conv, CharSet = CharSet.Unicode)]
        private delegate void NativeRoiEventCallback(int eventType,
            [MarshalAs(UnmanagedType.LPWStr)] string key, IntPtr userData);

        private static class Native
        {
            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern void D3IV_GetVersion(out int major, out int minor, out int patch);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_Create(out IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_Destroy(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_Initialize(IntPtr viewer, IntPtr parentHwnd,
                ref Rect2i rect, uint style);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetHwnd(IntPtr viewer, out IntPtr hwnd);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_InvalidateFrame(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_UpdateImage(IntPtr viewer, IntPtr data,
                uint width, uint height, uint stride, int pixelFormat);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_DetachImage(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetImageSize(IntPtr viewer,
                out uint width, out uint height);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetImageFormat(IntPtr viewer, out int pixelFormat);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetPixelValue(IntPtr viewer,
                int imageX, int imageY, [Out] double[] values, uint valueCapacity,
                out uint channelCount);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ImageOverlayAdd(IntPtr viewer, int shapeType,
                IntPtr items, uint count, ref OverlayStyle style, IntPtr outHandles);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_WindowOverlayAdd(IntPtr viewer, int shapeType,
                IntPtr items, uint count, ref OverlayStyle style, IntPtr outHandles);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ImageOverlayClear(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ImageOverlayShow(IntPtr viewer, int show);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_WindowOverlayClear(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_WindowOverlayShow(IntPtr viewer, int show);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROISetRect(IntPtr viewer, string key, string name,
                ref Rect2f rect, uint colorRgb, int isMovable, int isResizable, int fontSize);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROISetEllipse(IntPtr viewer, string key, string name,
                ref Ellipse2f ellipse, uint colorRgb, int isMovable, int isResizable, int fontSize);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROISetCircle(IntPtr viewer, string key, string name,
                ref Circle2f circle, uint colorRgb, int isMovable, int isResizable, int fontSize);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROISetPolygon(IntPtr viewer, string key, string name,
                [In] Point2f[] vertices, uint vertexCount, uint colorRgb,
                int isMovable, int isResizable, int fontSize);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROISetLine(IntPtr viewer, string key, string name,
                ref Line2f line, uint colorRgb, int isMovable, int isResizable, int fontSize);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_SetPixelScale(IntPtr viewer,
                double xScale, double yScale, string unit);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ToggleMeasureDistance(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_IsMeasureActive(IntPtr viewer, out int active);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ROIClear(IntPtr viewer);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIRemove(IntPtr viewer, string key);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetShape(IntPtr viewer, string key,
                ref RoiShapeNative shape);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetVertices(IntPtr viewer, string key,
                [Out] Point2f[] buffer, uint bufferCount, out uint outCount, uint segmentsPerCurve);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetInfo(IntPtr viewer, string key,
                ref RoiInfoNative info);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetName(IntPtr viewer, string key,
                [Out] char[] buffer, uint bufferChars, out uint outChars);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetBounds(IntPtr viewer, string key,
                out Rect2f bounds);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ROIGetCount(IntPtr viewer, out uint count);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetKeys(IntPtr viewer, [Out] char[] buffer,
                uint charsPerKey, uint bufferCount, out uint outCount);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIGetSelectedKey(IntPtr viewer,
                [Out] char[] buffer, uint bufferChars, out uint outChars);

            [DllImport(Dll, CallingConvention = Conv, CharSet = CharSet.Unicode)]
            internal static extern D3IVResult D3IV_ROIHitTest(IntPtr viewer,
                float imageX, float imageY, float tolerance,
                [Out] char[] keyBuffer, uint bufferChars, out uint outChars);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetMouseCallback(IntPtr viewer,
                NativeMouseCallback callback, IntPtr userData);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetROIEventCallback(IntPtr viewer,
                NativeRoiEventCallback callback, IntPtr userData);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetZoom(IntPtr viewer, float zoom, int animate);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetZoom(IntPtr viewer, out float zoom);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ZoomFit(IntPtr viewer, int animate);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_Zoom1To1(IntPtr viewer, int animate);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetCenter(IntPtr viewer,
                float imageX, float imageY, int animate);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetCenter(IntPtr viewer,
                out float x, out float y);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ZoomToRect(IntPtr viewer,
                ref Rect2f imageRect, float marginRatio, int animate);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_GetVisibleImageRect(IntPtr viewer,
                out Rect2f rect);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ScreenToImage(IntPtr viewer,
                int screenX, int screenY, out float imageX, out float imageY);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_ImageToScreen(IntPtr viewer,
                float imageX, float imageY, out int screenX, out int screenY);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetToolbarVisible(IntPtr viewer, int visible);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetStatusBarVisible(IntPtr viewer, int visible);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetBackgroundColor(IntPtr viewer, uint colorRgb);

            [DllImport(Dll, CallingConvention = Conv)]
            internal static extern D3IVResult D3IV_SetVSyncEnabled(IntPtr viewer, int enable);
        }

        #endregion

        #region 필드

        private IntPtr m_viewer = IntPtr.Zero;

        // ★ GC 가 수거하지 못하도록 델리게이트를 필드로 붙든다.
        private readonly NativeMouseCallback m_mouseTrampoline;
        private readonly NativeRoiEventCallback m_roiTrampoline;

        // UpdateImage 로 넘긴 관리 배열 고정 핸들.
        private GCHandle m_pinnedImage;

        private bool m_disposed;

        #endregion

        #region 이벤트

        /// <summary>
        /// 마우스 이벤트. 뷰어 내장 UI 다음, ROI 처리 앞에서 온다.
        /// e.Handled = true 로 두면 뷰어는 그 이벤트를 처리하지 않는다.
        /// </summary>
        public event EventHandler<ViewerMouseEventArgs> MouseEvent;

        /// <summary>
        /// ROI 선택/편집 이벤트. 결과 확정은 보통 EditEnd 에서 한다.
        /// 핸들러 안에서 다른 API 를 불러도 데드락이 없다.
        /// </summary>
        public event EventHandler<RoiEventArgs> RoiEvent;

        #endregion

        #region 생성 / 해제

        public D3D11ImageView()
        {
            D3IVResult result = Native.D3IV_Create(out m_viewer);
            if (result != D3IVResult.Ok)
            {
                throw new D3IVException(result, "D3IV_Create");
            }

            // 트램폴린을 먼저 만들어 필드에 담고 등록한다.
            m_mouseTrampoline = OnNativeMouse;
            m_roiTrampoline = OnNativeRoiEvent;

            Native.D3IV_SetMouseCallback(m_viewer, m_mouseTrampoline, IntPtr.Zero);
            Native.D3IV_SetROIEventCallback(m_viewer, m_roiTrampoline, IntPtr.Zero);
        }

        ~D3D11ImageView()
        {
            // 파이널라이저에서는 네이티브 핸들만 정리한다.
            // 창 파괴는 UI 스레드에서 해야 하므로 Dispose 를 명시 호출하는 게 정석이다.
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (m_disposed)
            {
                return;
            }

            m_disposed = true;

            if (m_viewer != IntPtr.Zero)
            {
                // 순서가 중요하다. 콜백을 먼저 떼고, 이미지 참조를 끊고, 파괴한다.
                Native.D3IV_SetMouseCallback(m_viewer, null, IntPtr.Zero);
                Native.D3IV_SetROIEventCallback(m_viewer, null, IntPtr.Zero);
                Native.D3IV_DetachImage(m_viewer);
                Native.D3IV_Destroy(m_viewer);

                m_viewer = IntPtr.Zero;
            }

            if (m_pinnedImage.IsAllocated)
            {
                m_pinnedImage.Free();
            }
        }

        public static Version GetNativeVersion()
        {
            int major, minor, patch;
            Native.D3IV_GetVersion(out major, out minor, out patch);

            return new Version(major, minor, patch);
        }

        #endregion

        #region 초기화

        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_CLIPCHILDREN = 0x02000000;
        private const uint WS_CLIPSIBLINGS = 0x04000000;

        public const uint DefaultChildStyle =
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

        /// <summary>
        /// parentHwnd 아래에 자식 창을 만든다. rect 는 부모 클라이언트 좌표,
        /// 물리 픽셀 단위다(DIP 아님).
        /// </summary>
        public void Initialize(IntPtr parentHwnd, Rect2i rect, uint style = DefaultChildStyle)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_Initialize(m_viewer, parentHwnd, ref rect, style), "D3IV_Initialize");
        }

        public IntPtr Hwnd
        {
            get
            {
                ThrowIfDisposed();

                IntPtr hwnd;
                Check(Native.D3IV_GetHwnd(m_viewer, out hwnd), "D3IV_GetHwnd");

                return hwnd;
            }
        }

        public void InvalidateFrame()
        {
            ThrowIfDisposed();
            Native.D3IV_InvalidateFrame(m_viewer);
        }

        #endregion

        #region 이미지

        /// <summary>
        /// 관리 배열을 그대로 표시한다. 배열은 내부에서 고정(pin)되며,
        /// 다음 UpdateImage 또는 DetachImage 까지 해제하면 안 된다.
        /// </summary>
        public void UpdateImage(byte[] data, int width, int height, int stride, PixelFormat format)
        {
            ThrowIfDisposed();

            if (data == null)
            {
                throw new ArgumentNullException("data");
            }

            long required = (long)stride * height;
            if (stride < MinimumStride(width, format) || data.LongLength < required)
            {
                throw new ArgumentException("stride 또는 배열 크기가 이미지에 비해 작습니다.");
            }

            // 새 버퍼를 고정하기 전에 뷰어가 이전 버퍼를 놓게 한다.
            Native.D3IV_DetachImage(m_viewer);

            if (m_pinnedImage.IsAllocated)
            {
                m_pinnedImage.Free();
            }

            m_pinnedImage = GCHandle.Alloc(data, GCHandleType.Pinned);

            D3IVResult result = Native.D3IV_UpdateImage(m_viewer,
                m_pinnedImage.AddrOfPinnedObject(),
                (uint)width, (uint)height, (uint)stride, (int)format);

            if (result != D3IVResult.Ok)
            {
                m_pinnedImage.Free();
                throw new D3IVException(result, "D3IV_UpdateImage");
            }
        }

        /// <summary>
        /// 이미 고정된 네이티브 버퍼(카메라 SDK 버퍼 등)를 직접 넘긴다.
        /// 수명 관리는 호출자 책임이다.
        /// </summary>
        public void UpdateImage(IntPtr data, int width, int height, int stride, PixelFormat format)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_UpdateImage(m_viewer, data, (uint)width, (uint)height,
                (uint)stride, (int)format), "D3IV_UpdateImage");
        }

        /// <summary>
        /// 뷰어가 원본 버퍼 참조를 놓게 한다. 반환 후에는 버퍼를 해제해도 안전하다.
        /// </summary>
        public void DetachImage()
        {
            ThrowIfDisposed();
            Native.D3IV_DetachImage(m_viewer);

            if (m_pinnedImage.IsAllocated)
            {
                m_pinnedImage.Free();
            }
        }

        public static int MinimumStride(int width, PixelFormat format)
        {
            switch (format)
            {
                case PixelFormat.Gray8: return width;
                case PixelFormat.Gray16: return width * 2;
                case PixelFormat.Bgr8: return width * 3;
                case PixelFormat.Bgra8: return width * 4;
                default: throw new ArgumentOutOfRangeException("format");
            }
        }

        public bool TryGetImageSize(out int width, out int height)
        {
            ThrowIfDisposed();

            uint w, h;
            bool ok = Native.D3IV_GetImageSize(m_viewer, out w, out h) == D3IVResult.Ok;

            width = (int)w;
            height = (int)h;

            return ok;
        }

        public bool TryGetImageFormat(out PixelFormat format)
        {
            ThrowIfDisposed();

            int raw;
            bool ok = Native.D3IV_GetImageFormat(m_viewer, out raw) == D3IVResult.Ok;
            format = (PixelFormat)raw;

            return ok;
        }

        /// <summary>
        /// 원본 픽셀값. 8bit 은 0~255, 16bit 은 0~65535 스케일이다.
        /// 이미지 밖이거나 이미지가 없으면 null.
        /// </summary>
        public double[] GetPixelValue(int imageX, int imageY)
        {
            ThrowIfDisposed();

            double[] values = new double[4];
            uint channelCount;

            if (Native.D3IV_GetPixelValue(m_viewer, imageX, imageY, values,
                    (uint)values.Length, out channelCount) != D3IVResult.Ok)
            {
                return null;
            }

            double[] trimmed = new double[channelCount];
            Array.Copy(values, trimmed, (int)channelCount);

            return trimmed;
        }

        #endregion

        #region 오버레이

        /// <summary>
        /// 이미지 좌표계 오버레이 추가. items 는 shapeType 에 대응하는 구조체 배열이다.
        /// (예: ShapeType.Rect2f -> Rect2f[])
        ///
        /// 좌표가 이미지에 붙으므로 줌/팬을 따라 움직인다. 결함 마커 등에 쓴다.
        /// </summary>
        public void AddImageOverlay<T>(ShapeType shapeType, T[] items, OverlayStyle style)
            where T : struct
        {
            AddOverlay(shapeType, items, style, true);
        }

        /// <summary>
        /// 화면 좌표계 오버레이 추가. 줌/팬과 무관하게 화면에 고정된다.
        /// 스케일 바, 십자선 등에 쓴다.
        /// </summary>
        public void AddWindowOverlay<T>(ShapeType shapeType, T[] items, OverlayStyle style)
            where T : struct
        {
            AddOverlay(shapeType, items, style, false);
        }

        private void AddOverlay<T>(ShapeType shapeType, T[] items, OverlayStyle style,
            bool imageSpace) where T : struct
        {
            ThrowIfDisposed();

            if (items == null || items.Length == 0)
            {
                return;
            }

            if (style.StructSize == 0)
            {
                style.StructSize = (uint)Marshal.SizeOf(typeof(OverlayStyle));
            }

            // 구조체 배열은 blittable 이라 고정만 하면 그대로 넘길 수 있다.
            GCHandle pin = GCHandle.Alloc(items, GCHandleType.Pinned);
            try
            {
                IntPtr ptr = pin.AddrOfPinnedObject();

                D3IVResult result = imageSpace
                    ? Native.D3IV_ImageOverlayAdd(m_viewer, (int)shapeType, ptr,
                        (uint)items.Length, ref style, IntPtr.Zero)
                    : Native.D3IV_WindowOverlayAdd(m_viewer, (int)shapeType, ptr,
                        (uint)items.Length, ref style, IntPtr.Zero);

                Check(result, imageSpace ? "D3IV_ImageOverlayAdd" : "D3IV_WindowOverlayAdd");
            }
            finally
            {
                pin.Free();
            }
        }

        public void ClearImageOverlay()
        {
            ThrowIfDisposed();
            Native.D3IV_ImageOverlayClear(m_viewer);
        }

        public void ShowImageOverlay(bool show)
        {
            ThrowIfDisposed();
            Native.D3IV_ImageOverlayShow(m_viewer, show ? 1 : 0);
        }

        public void ClearWindowOverlay()
        {
            ThrowIfDisposed();
            Native.D3IV_WindowOverlayClear(m_viewer);
        }

        public void ShowWindowOverlay(bool show)
        {
            ThrowIfDisposed();
            Native.D3IV_WindowOverlayShow(m_viewer, show ? 1 : 0);
        }

        #endregion

        #region ROI — 설정

        // colorRgb 는 COLORREF 와 같은 0x00BBGGRR 배치다.
        public static uint Rgb(byte r, byte g, byte b)
        {
            return (uint)(r | (g << 8) | (b << 16));
        }

        public void SetRoi(string key, string name, Rect2f rect, uint colorRgb,
            bool isMovable = true, bool isResizable = true, int fontSize = 14)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_ROISetRect(m_viewer, key, name, ref rect, colorRgb,
                isMovable ? 1 : 0, isResizable ? 1 : 0, fontSize), "D3IV_ROISetRect");
        }

        public void SetRoi(string key, string name, Ellipse2f ellipse, uint colorRgb,
            bool isMovable = true, bool isResizable = true, int fontSize = 14)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_ROISetEllipse(m_viewer, key, name, ref ellipse, colorRgb,
                isMovable ? 1 : 0, isResizable ? 1 : 0, fontSize), "D3IV_ROISetEllipse");
        }

        public void SetRoi(string key, string name, Circle2f circle, uint colorRgb,
            bool isMovable = true, bool isResizable = true, int fontSize = 14)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_ROISetCircle(m_viewer, key, name, ref circle, colorRgb,
                isMovable ? 1 : 0, isResizable ? 1 : 0, fontSize), "D3IV_ROISetCircle");
        }

        public void SetRoi(string key, string name, Point2f[] polygon, uint colorRgb,
            bool isMovable = true, bool isResizable = true, int fontSize = 14)
        {
            ThrowIfDisposed();

            if (polygon == null || polygon.Length < 3)
            {
                throw new ArgumentException("다각형은 3점 이상이어야 합니다.", "polygon");
            }

            Check(Native.D3IV_ROISetPolygon(m_viewer, key, name, polygon, (uint)polygon.Length,
                colorRgb, isMovable ? 1 : 0, isResizable ? 1 : 0, fontSize), "D3IV_ROISetPolygon");
        }

        public void SetRoi(string key, string name, Line2f line, uint colorRgb,
            bool isMovable = true, bool isResizable = true, int fontSize = 14)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_ROISetLine(m_viewer, key, name, ref line, colorRgb,
                isMovable ? 1 : 0, isResizable ? 1 : 0, fontSize), "D3IV_ROISetLine");
        }

        public bool RemoveRoi(string key)
        {
            ThrowIfDisposed();
            return Native.D3IV_ROIRemove(m_viewer, key) == D3IVResult.Ok;
        }

        public void ClearRoi()
        {
            ThrowIfDisposed();
            Native.D3IV_ROIClear(m_viewer);
        }

        #endregion

        #region ROI — 조회

        public int RoiCount
        {
            get
            {
                ThrowIfDisposed();

                uint count;
                if (Native.D3IV_ROIGetCount(m_viewer, out count) != D3IVResult.Ok)
                {
                    return 0;
                }

                return (int)count;
            }
        }

        /// <summary>
        /// 해석적 형상. 원/타원을 원본 파라미터로 받아 무손실 왕복이 된다.
        /// 계측(원 면적 = pi r^2)은 이쪽을 써야 정확하다.
        /// </summary>
        public bool TryGetRoiShape(string key, out RoiShape shape)
        {
            ThrowIfDisposed();

            RoiShapeNative native = new RoiShapeNative();
            native.StructSize = (uint)Marshal.SizeOf(typeof(RoiShapeNative));

            shape = new RoiShape();

            if (Native.D3IV_ROIGetShape(m_viewer, key, ref native) != D3IVResult.Ok)
            {
                return false;
            }

            shape.Type = (RoiShapeType)native.Type;
            shape.VertexCount = native.VertexCount;

            switch (shape.Type)
            {
                case RoiShapeType.Rectangle:
                    shape.Rect = new Rect2f(native.F0, native.F1, native.F2, native.F3);
                    break;

                case RoiShapeType.Ellipse:
                    shape.Ellipse = new Ellipse2f(native.F0, native.F1, native.F2,
                        native.F3, native.F4);
                    break;

                case RoiShapeType.Circle:
                    shape.Circle = new Circle2f(native.F0, native.F1, native.F2);
                    break;

                case RoiShapeType.Line:
                    shape.Line = new Line2f(native.F0, native.F1, native.F2, native.F3);
                    break;

                case RoiShapeType.Polygon:
                    // 정점은 GetRoiVertices 로 받는다.
                    break;
            }

            return true;
        }

        /// <summary>
        /// 정점 배열(이미지 좌표). 마스킹/픽셀 순회용.
        ///   Rectangle -> 4점, Polygon -> 원본 정점
        ///   Circle / Ellipse -> segmentsPerCurve 등분 근사
        /// 대상이 없으면 null.
        /// </summary>
        public Point2f[] GetRoiVertices(string key, int segmentsPerCurve = 64)
        {
            ThrowIfDisposed();

            // 1회차: 필요한 개수만 받는다.
            uint needed;
            D3IVResult probe = Native.D3IV_ROIGetVertices(m_viewer, key, null, 0,
                out needed, (uint)segmentsPerCurve);

            if (needed == 0 || probe == D3IVResult.NotFound)
            {
                return null;
            }

            Point2f[] buffer = new Point2f[needed];

            uint written;
            if (Native.D3IV_ROIGetVertices(m_viewer, key, buffer, needed, out written,
                    (uint)segmentsPerCurve) != D3IVResult.Ok)
            {
                return null;
            }

            return buffer;
        }

        public bool TryGetRoiBounds(string key, out Rect2f bounds)
        {
            ThrowIfDisposed();
            return Native.D3IV_ROIGetBounds(m_viewer, key, out bounds) == D3IVResult.Ok;
        }

        public bool TryGetRoiInfo(string key, out RoiInfo info)
        {
            ThrowIfDisposed();

            RoiInfoNative native = new RoiInfoNative();
            native.StructSize = (uint)Marshal.SizeOf(typeof(RoiInfoNative));

            info = new RoiInfo();

            if (Native.D3IV_ROIGetInfo(m_viewer, key, ref native) != D3IVResult.Ok)
            {
                return false;
            }

            info.Type = (RoiShapeType)native.Type;
            info.ColorRgb = native.ColorRgb;
            info.IsMovable = native.IsMovable != 0;
            info.IsResizable = native.IsResizable != 0;
            info.IsSelected = native.IsSelected != 0;
            info.IsHovered = native.IsHovered != 0;
            info.FontSize = native.FontSize;

            return true;
        }

        public string GetRoiName(string key)
        {
            ThrowIfDisposed();

            uint needed;
            Native.D3IV_ROIGetName(m_viewer, key, null, 0, out needed);

            if (needed == 0)
            {
                return null;
            }

            char[] buffer = new char[needed];
            if (Native.D3IV_ROIGetName(m_viewer, key, buffer, needed, out needed) != D3IVResult.Ok)
            {
                return null;
            }

            return TrimToNull(buffer);
        }

        /// <summary>현재 선택된 ROI 키. 없으면 null.</summary>
        public string GetSelectedRoiKey()
        {
            ThrowIfDisposed();

            uint needed;
            Native.D3IV_ROIGetSelectedKey(m_viewer, null, 0, out needed);

            if (needed == 0)
            {
                return null;
            }

            char[] buffer = new char[needed];
            if (Native.D3IV_ROIGetSelectedKey(m_viewer, buffer, needed, out needed) != D3IVResult.Ok)
            {
                return null;
            }

            return TrimToNull(buffer);
        }

        /// <summary>이미지 좌표에서 히트 테스트. 없으면 null.</summary>
        public string HitTestRoi(float imageX, float imageY, float tolerance = 4.0f)
        {
            ThrowIfDisposed();

            uint needed;
            Native.D3IV_ROIHitTest(m_viewer, imageX, imageY, tolerance, null, 0, out needed);

            if (needed == 0)
            {
                return null;
            }

            char[] buffer = new char[needed];
            if (Native.D3IV_ROIHitTest(m_viewer, imageX, imageY, tolerance, buffer, needed,
                    out needed) != D3IVResult.Ok)
            {
                return null;
            }

            return TrimToNull(buffer);
        }

        /// <summary>
        /// 전체 ROI 키 목록.
        ///
        /// 네이티브는 [bufferCount][charsPerKey] 2차원 블록을 쓰므로 키 하나의
        /// 최대 길이를 미리 정해야 한다. 기본 256자로 잡는다.
        /// </summary>
        public string[] GetRoiKeys(int charsPerKey = 256)
        {
            ThrowIfDisposed();

            uint needed;
            Native.D3IV_ROIGetKeys(m_viewer, null, (uint)charsPerKey, 0, out needed);

            if (needed == 0)
            {
                return new string[0];
            }

            char[] block = new char[needed * charsPerKey];

            uint written;
            D3IVResult result = Native.D3IV_ROIGetKeys(m_viewer, block, (uint)charsPerKey,
                needed, out written);

            if (result != D3IVResult.Ok && result != D3IVResult.BufferTooSmall)
            {
                return new string[0];
            }

            int count = (int)Math.Min(written, needed);
            string[] keys = new string[count];

            for (int i = 0; i < count; i++)
            {
                keys[i] = TrimToNull(block, i * charsPerKey, charsPerKey);
            }

            return keys;
        }

        #endregion

        #region 뷰 제어

        /// <summary>배율. 1.0 이 1픽셀 = 1픽셀.</summary>
        public float Zoom
        {
            get
            {
                ThrowIfDisposed();

                float zoom;
                if (Native.D3IV_GetZoom(m_viewer, out zoom) != D3IVResult.Ok)
                {
                    return 0.0f;
                }

                return zoom;
            }
        }

        public void SetZoom(float zoom, bool animate = true)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_SetZoom(m_viewer, zoom, animate ? 1 : 0), "D3IV_SetZoom");
        }

        public void ZoomFit(bool animate = true)
        {
            ThrowIfDisposed();
            Native.D3IV_ZoomFit(m_viewer, animate ? 1 : 0);
        }

        public void Zoom1To1(bool animate = true)
        {
            ThrowIfDisposed();
            Native.D3IV_Zoom1To1(m_viewer, animate ? 1 : 0);
        }

        /// <summary>뷰 중심에 놓을 이미지 좌표.</summary>
        public void SetCenter(float imageX, float imageY, bool animate = true)
        {
            ThrowIfDisposed();
            Native.D3IV_SetCenter(m_viewer, imageX, imageY, animate ? 1 : 0);
        }

        public bool TryGetCenter(out float imageX, out float imageY)
        {
            ThrowIfDisposed();
            return Native.D3IV_GetCenter(m_viewer, out imageX, out imageY) == D3IVResult.Ok;
        }

        /// <summary>
        /// 지정한 이미지 영역이 화면에 꽉 차도록 줌/중심을 맞춘다.
        /// 결함 좌표로 바로 날아가는 데 쓴다.
        /// </summary>
        public void ZoomToRect(Rect2f imageRect, float marginRatio = 0.1f, bool animate = true)
        {
            ThrowIfDisposed();
            Check(Native.D3IV_ZoomToRect(m_viewer, ref imageRect, marginRatio, animate ? 1 : 0),
                "D3IV_ZoomToRect");
        }

        public bool TryGetVisibleImageRect(out Rect2f rect)
        {
            ThrowIfDisposed();
            return Native.D3IV_GetVisibleImageRect(m_viewer, out rect) == D3IVResult.Ok;
        }

        #endregion

        #region 좌표 변환

        /// <summary>
        /// 뷰어 클라이언트 픽셀 -> 이미지 좌표.
        /// 반환값은 결과가 이미지 안인지 여부다. 밖이어도 out 값은 채워진다.
        /// </summary>
        public bool ScreenToImage(int screenX, int screenY, out float imageX, out float imageY)
        {
            ThrowIfDisposed();
            return Native.D3IV_ScreenToImage(m_viewer, screenX, screenY,
                out imageX, out imageY) == D3IVResult.Ok;
        }

        public bool ImageToScreen(float imageX, float imageY, out int screenX, out int screenY)
        {
            ThrowIfDisposed();
            return Native.D3IV_ImageToScreen(m_viewer, imageX, imageY,
                out screenX, out screenY) == D3IVResult.Ok;
        }

        #endregion

        #region 표시 옵션

        /// <summary>
        /// 뷰어 내장 툴바. WPF 처럼 호스트가 자기 UI 를 얹을 수 없는 환경에서는
        /// 이걸 끄고 호스트가 이미지 바깥에 버튼을 두는 구성이 낫다.
        /// </summary>
        public void SetToolbarVisible(bool visible)
        {
            ThrowIfDisposed();
            Native.D3IV_SetToolbarVisible(m_viewer, visible ? 1 : 0);
        }

        public void SetStatusBarVisible(bool visible)
        {
            ThrowIfDisposed();
            Native.D3IV_SetStatusBarVisible(m_viewer, visible ? 1 : 0);
        }

        /// <summary>이미지 바깥 배경색. COLORREF 0x00BBGGRR (Rgb 헬퍼 참고).</summary>
        public void SetBackgroundColor(uint colorRgb)
        {
            ThrowIfDisposed();
            Native.D3IV_SetBackgroundColor(m_viewer, colorRgb);
        }

        /// <summary>
        /// 이미지 1픽셀이 실제로 몇 단위인지. 기본 1px = 1 unit, 단위 "px".
        /// Line ROI 의 길이 라벨이 이 값을 적용해 표시한다.
        /// </summary>
        public void SetPixelScale(double xScale, double yScale, string unit = "px")
        {
            ThrowIfDisposed();
            Check(Native.D3IV_SetPixelScale(m_viewer, xScale, yScale, unit), "D3IV_SetPixelScale");
        }

        /// <summary>
        /// 거리 측정 도구 토글. 누를 때마다 기존 측정선을 리셋한다.
        /// 활성 상태에서 이미지를 클릭하면 첫 점, 다시 클릭하면 확정된다.
        /// </summary>
        public void ToggleMeasureDistance()
        {
            ThrowIfDisposed();
            Native.D3IV_ToggleMeasureDistance(m_viewer);
        }

        public bool IsMeasureActive
        {
            get
            {
                ThrowIfDisposed();

                int active;
                if (Native.D3IV_IsMeasureActive(m_viewer, out active) != D3IVResult.Ok)
                {
                    return false;
                }

                return active != 0;
            }
        }

        /// <summary>기본 true. false 로 두면 티어링 대신 프레임이 버려진다.</summary>
        public void SetVSyncEnabled(bool enable)
        {
            ThrowIfDisposed();
            Native.D3IV_SetVSyncEnabled(m_viewer, enable ? 1 : 0);
        }

        #endregion

        #region 콜백 트램폴린

        private int OnNativeMouse(ref MouseEventNative e, IntPtr userData)
        {
            // 네이티브로 예외가 새어 나가면 동작이 정의되지 않는다. 전부 삼킨다.
            try
            {
                EventHandler<ViewerMouseEventArgs> handler = MouseEvent;
                if (handler == null)
                {
                    return 0;
                }

                ViewerMouseEventArgs args = new ViewerMouseEventArgs();
                args.Type = (MouseEventType)e.Type;
                args.ScreenX = e.ScreenX;
                args.ScreenY = e.ScreenY;
                args.ImageX = e.ImageX;
                args.ImageY = e.ImageY;
                args.IsInsideImage = e.IsInsideImage != 0;
                args.WheelDelta = e.WheelDelta;
                args.Modifiers = (MouseModifiers)e.Modifiers;
                args.Buttons = (MouseButtons)e.Buttons;

                handler(this, args);

                return args.Handled ? 1 : 0;
            }
            catch
            {
                return 0;
            }
        }

        private void OnNativeRoiEvent(int eventType, string key, IntPtr userData)
        {
            try
            {
                EventHandler<RoiEventArgs> handler = RoiEvent;
                if (handler == null)
                {
                    return;
                }

                RoiEventArgs args = new RoiEventArgs();
                args.Type = (RoiEventType)eventType;
                args.Key = key;

                handler(this, args);
            }
            catch
            {
                // 무시. 호스트 핸들러 예외를 네이티브로 던지지 않는다.
            }
        }

        #endregion

        #region 내부 헬퍼

        private void ThrowIfDisposed()
        {
            if (m_disposed || m_viewer == IntPtr.Zero)
            {
                throw new ObjectDisposedException("D3D11ImageView");
            }
        }

        private static void Check(D3IVResult result, string operation)
        {
            if (result != D3IVResult.Ok)
            {
                throw new D3IVException(result, operation);
            }
        }

        private static string TrimToNull(char[] buffer)
        {
            return TrimToNull(buffer, 0, buffer.Length);
        }

        private static string TrimToNull(char[] buffer, int offset, int length)
        {
            int end = offset;
            int limit = Math.Min(offset + length, buffer.Length);

            while (end < limit && buffer[end] != '\0')
            {
                end++;
            }

            return new string(buffer, offset, end - offset);
        }

        #endregion
    }
}
