// D3D11ImageView — WPF 호스트
//
// WPF 는 D3D11 자식 창을 직접 담을 수 없다. HwndHost 로 감싸야 한다.
//
// ★ Airspace 제약 (WPF + HWND 의 근본 한계)
//   HwndHost 안의 자식 HWND 는 WPF 렌더 트리 밖에서 그려지므로 항상
//   WPF 컨텐츠 위에 올라온다. 따라서 이 컨트롤 위에는
//     - Popup / ToolTip / ContextMenu / Adorner
//     - Opacity, RenderTransform, Clip, Effect
//   가 먹지 않는다. 뷰어 영역 위에 뭘 얹으려면 두 가지 길뿐이다.
//     1) 뷰어 내장 오버레이/ROI/툴바를 쓴다  ← 권장
//     2) 호스트 UI 를 뷰어 바깥(위/아래/옆)에 둔다
//   그래서 이 호스트는 기본값으로 내장 툴바/상태바를 켜 둔다.
//
// ★ DPI
//   HwndHost 는 레이아웃 크기를 장치 픽셀로 변환해 자식 창을 배치해 준다.
//   따라서 크기 계산은 손댈 필요가 없다. 다만 뷰어가 콜백으로 주는
//   ScreenX/Y 는 장치 픽셀이므로, WPF DIP 와 맞추려면 DpiScale 로 나눈다.
//
// 사용 예 (XAML)
//   xmlns:viewer="clr-namespace:D3D11ImageViewInterop;assembly=YourAssembly"
//   <viewer:ImageViewHost x:Name="Viewer" ViewerCreated="OnViewerCreated" />

using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace D3D11ImageViewInterop
{
    public class ImageViewHost : HwndHost
    {
        #region Win32

        private const int WM_SIZE = 0x0005;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyWindow(IntPtr hwnd);

        #endregion

        private D3D11ImageView m_viewer;
        private IntPtr m_hwnd = IntPtr.Zero;

        /// <summary>
        /// 네이티브 뷰어. BuildWindowCore 전에는 null 이다.
        /// 초기 설정은 ViewerCreated 에서 하는 것이 안전하다.
        /// </summary>
        public D3D11ImageView Viewer
        {
            get { return m_viewer; }
        }

        /// <summary>
        /// 뷰어 생성 직후. 여기서 이미지/ROI/옵션을 설정한다.
        /// XAML 로드 시점에는 아직 창이 없으므로 생성자에서 하면 안 된다.
        /// </summary>
        public event EventHandler ViewerCreated;

        /// <summary>뷰어 마우스 이벤트를 그대로 중계한다.</summary>
        public event EventHandler<ViewerMouseEventArgs> ViewerMouseEvent;

        /// <summary>ROI 선택/편집 이벤트를 그대로 중계한다.</summary>
        public event EventHandler<RoiEventArgs> ViewerRoiEvent;

        #region 표시 옵션 (의존 속성)

        public static readonly DependencyProperty ToolbarVisibleProperty =
            DependencyProperty.Register("ToolbarVisible", typeof(bool), typeof(ImageViewHost),
                new PropertyMetadata(true, OnToolbarVisibleChanged));

        /// <summary>뷰어 내장 툴바 표시. 기본 true.</summary>
        public bool ToolbarVisible
        {
            get { return (bool)GetValue(ToolbarVisibleProperty); }
            set { SetValue(ToolbarVisibleProperty, value); }
        }

        private static void OnToolbarVisibleChanged(DependencyObject d,
            DependencyPropertyChangedEventArgs e)
        {
            ImageViewHost host = (ImageViewHost)d;
            if (host.m_viewer != null)
            {
                host.m_viewer.SetToolbarVisible((bool)e.NewValue);
            }
        }

        public static readonly DependencyProperty StatusBarVisibleProperty =
            DependencyProperty.Register("StatusBarVisible", typeof(bool), typeof(ImageViewHost),
                new PropertyMetadata(true, OnStatusBarVisibleChanged));

        /// <summary>뷰어 내장 상태바 표시. 기본 true.</summary>
        public bool StatusBarVisible
        {
            get { return (bool)GetValue(StatusBarVisibleProperty); }
            set { SetValue(StatusBarVisibleProperty, value); }
        }

        private static void OnStatusBarVisibleChanged(DependencyObject d,
            DependencyPropertyChangedEventArgs e)
        {
            ImageViewHost host = (ImageViewHost)d;
            if (host.m_viewer != null)
            {
                host.m_viewer.SetStatusBarVisible((bool)e.NewValue);
            }
        }

        #endregion

        #region DPI

        /// <summary>
        /// DIP -> 장치 픽셀 배율. 96dpi 에서 1.0, 150% 에서 1.5.
        ///
        /// 뷰어가 주는 좌표는 장치 픽셀이므로 WPF 좌표와 섞을 때 쓴다.
        /// </summary>
        public double DpiScale
        {
            get
            {
                PresentationSource source = PresentationSource.FromVisual(this);
                if (source == null || source.CompositionTarget == null)
                {
                    return 1.0;
                }

                return source.CompositionTarget.TransformToDevice.M11;
            }
        }

        /// <summary>뷰어 콜백 좌표(장치 픽셀) -> WPF DIP.</summary>
        public Point DeviceToDip(int deviceX, int deviceY)
        {
            double scale = DpiScale;
            if (scale <= 0.0)
            {
                scale = 1.0;
            }

            return new Point(deviceX / scale, deviceY / scale);
        }

        #endregion

        #region HwndHost

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            m_viewer = new D3D11ImageView();
            m_viewer.MouseEvent += OnViewerMouseEvent;
            m_viewer.RoiEvent += OnViewerRoiEvent;

            // 초기 크기는 의미가 없다. HwndHost 가 레이아웃 직후
            // 자식 창을 실제 크기로 다시 배치한다. 0 크기로 만들면
            // 스왑체인 생성이 실패할 수 있어 최소 크기를 준다.
            double scale = DpiScale;
            int width = (int)Math.Max(1.0, ActualWidth * scale);
            int height = (int)Math.Max(1.0, ActualHeight * scale);

            Rect2i rect = new Rect2i(0, 0, width, height);

            try
            {
                m_viewer.Initialize(hwndParent.Handle, rect);
                m_hwnd = m_viewer.Hwnd;
            }
            catch
            {
                m_viewer.Dispose();
                m_viewer = null;

                throw;
            }

            m_viewer.SetToolbarVisible(ToolbarVisible);
            m_viewer.SetStatusBarVisible(StatusBarVisible);

            EventHandler created = ViewerCreated;
            if (created != null)
            {
                created(this, EventArgs.Empty);
            }

            return new HandleRef(this, m_hwnd);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            if (m_viewer != null)
            {
                m_viewer.MouseEvent -= OnViewerMouseEvent;
                m_viewer.RoiEvent -= OnViewerRoiEvent;

                // D3IV_Destroy 가 창까지 파괴한다. DestroyWindow 를 따로
                // 부르면 이중 파괴가 되므로 여기서는 Dispose 만 한다.
                m_viewer.Dispose();
                m_viewer = null;
            }
            else if (hwnd.Handle != IntPtr.Zero)
            {
                DestroyWindow(hwnd.Handle);
            }

            m_hwnd = IntPtr.Zero;
        }

        // HwndHost 기본 MeasureOverride 는 남는 공간을 다 쓰지 않는다.
        // 뷰어는 컨테이너를 채우는 쪽이 자연스러우므로 제약을 그대로 받는다.
        protected override Size MeasureOverride(Size constraint)
        {
            double width = double.IsInfinity(constraint.Width) ? 320.0 : constraint.Width;
            double height = double.IsInfinity(constraint.Height) ? 240.0 : constraint.Height;

            return new Size(width, height);
        }

        #endregion

        #region 이벤트 중계

        private void OnViewerMouseEvent(object sender, ViewerMouseEventArgs e)
        {
            EventHandler<ViewerMouseEventArgs> handler = ViewerMouseEvent;
            if (handler != null)
            {
                handler(this, e);
            }
        }

        private void OnViewerRoiEvent(object sender, RoiEventArgs e)
        {
            EventHandler<RoiEventArgs> handler = ViewerRoiEvent;
            if (handler != null)
            {
                handler(this, e);
            }
        }

        #endregion
    }
}
