// D3D11ImageView — WPF 최소 예제
//
// 검사장비 뷰어가 실제로 쓰는 흐름만 담았다.
//   1) 8bit 이미지를 올린다 (카메라 프레임 자리)
//   2) 결함 좌표를 이미지 오버레이로 표시한다
//   3) 사용자가 ROI 를 그리거나 옮긴다 -> EditEnd 에서 결과를 읽는다
//   4) 마우스 좌표/픽셀값을 상태줄에 띄운다
//   5) 결함 좌표로 ZoomToRect 로 날아간다

using System;
using System.Text;
using System.Windows;

using D3D11ImageViewInterop;

namespace WpfViewerExample
{
    public partial class MainWindow : Window
    {
        // ★ 이미지 버퍼는 필드로 붙든다.
        //   뷰어는 이 배열을 복사하지 않고 빌려 쓴다. 지역 변수로 두면
        //   GC 가 수거해 렌더 스레드가 해제된 메모리를 읽는다.
        //   (래퍼가 pin 은 걸어 주지만 참조 자체는 호스트가 유지해야 한다.)
        private byte[] m_imageBuffer;
        private int m_imageWidth;
        private int m_imageHeight;

        public MainWindow()
        {
            InitializeComponent();

            Closed += OnClosed;
        }

        private D3D11ImageView View
        {
            get { return Viewer != null ? Viewer.Viewer : null; }
        }

        /// <summary>
        /// 뷰어 창이 만들어진 직후. 생성자에서는 아직 창이 없다.
        /// </summary>
        private void OnViewerCreated(object sender, EventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            // 이미지 바깥 배경. 검사 화면은 보통 어둡게 둔다.
            view.SetBackgroundColor(D3D11ImageView.Rgb(32, 32, 32));

            StatusText.Text = string.Format("뷰어 준비 완료 (native {0})",
                D3D11ImageView.GetNativeVersion());
        }

        private void OnClosed(object sender, EventArgs e)
        {
            // HwndHost 가 DestroyWindowCore 에서 뷰어를 Dispose 한다.
            // 여기서는 버퍼 참조만 놓는다.
            m_imageBuffer = null;
        }

        #region 이미지

        private void OnGenerateImage(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            m_imageWidth = 4096;
            m_imageHeight = 4096;

            // 8bit 그레이. 카메라 프레임이 들어올 자리다.
            m_imageBuffer = new byte[(long)m_imageWidth * m_imageHeight];

            for (int y = 0; y < m_imageHeight; y++)
            {
                int row = y * m_imageWidth;
                for (int x = 0; x < m_imageWidth; x++)
                {
                    // 격자 + 그라디언트. 샘플링 품질을 눈으로 확인하기 좋다.
                    bool grid = (x % 64 == 0) || (y % 64 == 0);
                    m_imageBuffer[row + x] = grid
                        ? (byte)255
                        : (byte)((x * 255 / m_imageWidth + y * 255 / m_imageHeight) / 2);
                }
            }

            view.UpdateImage(m_imageBuffer, m_imageWidth, m_imageHeight,
                m_imageWidth, PixelFormat.Gray8);

            view.ZoomFit(false);

            StatusText.Text = string.Format("이미지 {0}x{1} 8bit Gray 표시",
                m_imageWidth, m_imageHeight);
        }

        // 16bit Gray.
        //
        // 값을 40000~40400 의 좁은 구간에만 몰아넣는다. 실제 검사 이미지가
        // 흔히 이런 모양이다 — 센서 전체 범위 중 관심 구간은 일부뿐이다.
        //
        // 백버퍼가 8bit 라 이 구간은 LUT 없이 보면 화면에서 156~157,
        // 즉 두 계조로 뭉개진다. LUT(자동 대비)를 켜야 비로소 보인다.
        private void OnGenerateImage16(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            m_imageWidth = 2048;
            m_imageHeight = 2048;

            m_imageBuffer = new byte[(long)m_imageWidth * m_imageHeight * 2];

            const ushort baseValue = 40000;
            const ushort span = 400;

            for (int y = 0; y < m_imageHeight; y++)
            {
                int rowByte = y * m_imageWidth * 2;

                for (int x = 0; x < m_imageWidth; x++)
                {
                    // 대각 그라디언트 + 원형 무늬. 대비가 살아나야 보인다.
                    int dx = x - m_imageWidth / 2;
                    int dy = y - m_imageHeight / 2;
                    int radius = (int)Math.Sqrt(dx * dx + dy * dy);

                    int offset = (x + y) * span / (m_imageWidth + m_imageHeight);
                    if ((radius / 64) % 2 == 0)
                    {
                        offset += span / 8;
                    }

                    ushort value = (ushort)(baseValue + offset);

                    m_imageBuffer[rowByte + x * 2] = (byte)(value & 0xFF);
                    m_imageBuffer[rowByte + x * 2 + 1] = (byte)(value >> 8);
                }
            }

            view.UpdateImage(m_imageBuffer, m_imageWidth, m_imageHeight,
                m_imageWidth * 2, PixelFormat.Gray16);

            view.ZoomFit(false);

            StatusText.Text = string.Format(
                "이미지 {0}x{1} 16bit Gray 표시 — 값이 {2}~{3} 에만 있습니다. " +
                "LUT(▦)를 켜야 대비가 보입니다.",
                m_imageWidth, m_imageHeight, baseValue, baseValue + span + span / 8);
        }

        // 컬러(BGR 8bit). LUT 버튼이 비활성으로 바뀌는 것을 확인하는 용도다.
        private void OnGenerateImageColor(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            m_imageWidth = 2048;
            m_imageHeight = 2048;

            m_imageBuffer = new byte[(long)m_imageWidth * m_imageHeight * 3];

            for (int y = 0; y < m_imageHeight; y++)
            {
                int rowByte = y * m_imageWidth * 3;

                for (int x = 0; x < m_imageWidth; x++)
                {
                    int i = rowByte + x * 3;

                    // BGR 순서. 채널마다 다른 방향으로 변하게 둔다.
                    m_imageBuffer[i + 0] = (byte)(x * 255 / m_imageWidth);
                    m_imageBuffer[i + 1] = (byte)(y * 255 / m_imageHeight);
                    m_imageBuffer[i + 2] = (byte)(255 - (x * 255 / m_imageWidth));
                }
            }

            view.UpdateImage(m_imageBuffer, m_imageWidth, m_imageHeight,
                m_imageWidth * 3, PixelFormat.Bgr8);

            view.ZoomFit(false);

            StatusText.Text = string.Format(
                "이미지 {0}x{1} BGR 8bit 표시 — 컬러라 LUT 버튼(▦)이 비활성입니다.",
                m_imageWidth, m_imageHeight);
        }

        #endregion

        #region 뷰 제어

        private void OnZoomFit(object sender, RoutedEventArgs e)
        {
            if (View != null)
            {
                View.ZoomFit();
            }
        }

        private void OnZoom1To1(object sender, RoutedEventArgs e)
        {
            if (View != null)
            {
                View.Zoom1To1();
            }
        }

        private void OnBuiltInUiChanged(object sender, RoutedEventArgs e)
        {
            // XAML 이 IsChecked="True" 를 적용하는 시점에 Checked 가 먼저 터진다.
            // 그때는 아직 아래쪽의 ImageViewHost 가 만들어지지 않아 Viewer 가 null 이다.
            // XAML 에 연결한 핸들러는 로드 도중에도 불릴 수 있다고 보고 방어해야 한다.
            if (Viewer == null)
            {
                return;
            }

            bool visible = UseBuiltInUi.IsChecked == true;

            // 의존 속성으로 두었으므로 XAML 바인딩으로도 제어할 수 있다.
            Viewer.ToolbarVisible = visible;
            Viewer.StatusBarVisible = visible;
        }

        #endregion

        #region 오버레이 — 결함 마커

        private void OnAddMarkers(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null || m_imageBuffer == null)
            {
                StatusText.Text = "먼저 이미지를 생성하세요.";
                return;
            }

            view.ClearImageOverlay();

            // 검사 결과 좌표. 이미지 좌표계이므로 줌/팬을 따라 움직인다.
            Rect2f[] defects = new Rect2f[]
            {
                new Rect2f(500.0f, 400.0f, 620.0f, 520.0f),
                new Rect2f(1800.0f, 1200.0f, 1880.0f, 1260.0f),
                new Rect2f(3000.0f, 2600.0f, 3120.0f, 2700.0f),
            };

            view.AddImageOverlay(ShapeType.Rect2f, defects,
                OverlayStyle.Outline(255, 64, 64, 2.0f));

            // 첫 결함으로 날아간다. 여백 20%.
            view.ZoomToRect(defects[0], 0.2f);

            StatusText.Text = string.Format("결함 {0}개 표시, 1번으로 이동", defects.Length);
        }

        #endregion

        #region ROI

        private void OnAddRoi(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null || m_imageBuffer == null)
            {
                StatusText.Text = "먼저 이미지를 생성하세요.";
                return;
            }

            // 키는 호스트가 정한다. 같은 키로 다시 Set 하면 갱신이다.
            view.SetRoi("roi.rect", "검사영역",
                new Rect2f(300.0f, 300.0f, 900.0f, 800.0f),
                D3D11ImageView.Rgb(0, 220, 0));

            view.SetRoi("roi.circle", "원형 게이지",
                new Circle2f(2000.0f, 1500.0f, 350.0f),
                D3D11ImageView.Rgb(0, 180, 255));

            // 거리 측정과 같은 Line ROI. 길이 라벨이 자동으로 붙는다.
            view.SetRoi("roi.line", "", new Line2f(600.0f, 1800.0f, 1800.0f, 2400.0f),
                D3D11ImageView.Rgb(255, 200, 0));

            StatusText.Text = "ROI 2개 추가. 드래그로 옮기거나 크기를 바꿀 수 있습니다.";
        }

        private void OnReadRoi(object sender, RoutedEventArgs e)
        {
            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            StringBuilder log = new StringBuilder();

            foreach (string key in view.GetRoiKeys())
            {
                RoiShape shape;
                if (!view.TryGetRoiShape(key, out shape))
                {
                    continue;
                }

                log.Append(key);
                log.Append(" [");
                log.Append(view.GetRoiName(key));
                log.Append("] ");

                switch (shape.Type)
                {
                    case RoiShapeType.Rectangle:
                        // 해석적 값을 그대로 받으므로 면적이 정확하다.
                        log.AppendFormat("Rect {0:F1},{1:F1} {2:F1}x{3:F1} 면적={4:F0}px  ",
                            shape.Rect.Left, shape.Rect.Top,
                            shape.Rect.Width, shape.Rect.Height,
                            shape.Rect.Width * shape.Rect.Height);
                        break;

                    case RoiShapeType.Circle:
                        log.AppendFormat("Circle c=({0:F1},{1:F1}) r={2:F1} 면적={3:F0}px  ",
                            shape.Circle.CenterX, shape.Circle.CenterY,
                            shape.Circle.Radius,
                            Math.PI * shape.Circle.Radius * shape.Circle.Radius);
                        break;

                    case RoiShapeType.Ellipse:
                        log.AppendFormat("Ellipse c=({0:F1},{1:F1}) r=({2:F1},{3:F1}) 면적={4:F0}px  ",
                            shape.Ellipse.CenterX, shape.Ellipse.CenterY,
                            shape.Ellipse.RadiusX, shape.Ellipse.RadiusY,
                            Math.PI * shape.Ellipse.RadiusX * shape.Ellipse.RadiusY);
                        break;

                    case RoiShapeType.Polygon:
                        // 다각형은 정점으로 받는다.
                        Point2f[] vertices = view.GetRoiVertices(key);
                        log.AppendFormat("Polygon {0}점  ",
                            vertices != null ? vertices.Length : 0);
                        break;
                }
            }

            StatusText.Text = log.Length > 0 ? log.ToString() : "ROI 가 없습니다.";
        }

        private void OnViewerRoi(object sender, RoiEventArgs e)
        {
            // 편집 확정은 EditEnd 에서 읽는다. EditChanged 는 드래그 중 매 프레임 온다.
            if (e.Type != RoiEventType.EditEnd && e.Type != RoiEventType.Selected)
            {
                return;
            }

            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            Rect2f bounds;
            if (view.TryGetRoiBounds(e.Key, out bounds))
            {
                StatusText.Text = string.Format(
                    "ROI {0} / {1} -> ({2:F1}, {3:F1}) {4:F1}x{5:F1}",
                    e.Type, e.Key, bounds.Left, bounds.Top, bounds.Width, bounds.Height);
            }
        }

        #endregion

        #region 마우스

        private void OnViewerMouse(object sender, ViewerMouseEventArgs e)
        {
            if (e.Type != MouseEventType.Move)
            {
                return;
            }

            if (!e.IsInsideImage)
            {
                return;
            }

            D3D11ImageView view = View;
            if (view == null)
            {
                return;
            }

            double[] pixel = view.GetPixelValue((int)e.ImageX, (int)e.ImageY);

            StatusText.Text = string.Format("image ({0:F1}, {1:F1})  value={2}  zoom={3:F3}",
                e.ImageX, e.ImageY,
                pixel != null ? string.Join(",", Array.ConvertAll(pixel, v => v.ToString("F0"))) : "-",
                view.Zoom);

            // e.Handled 를 건드리지 않으므로 팬/줌/ROI 편집은 뷰어가 계속 처리한다.
            // 호스트가 선점하려면 e.Handled = true 로 둔다.
        }

        #endregion
    }
}
