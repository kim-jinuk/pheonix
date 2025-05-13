using System;
using System.IO;
using System.Windows;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Media;

using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.ViewModel;
using System.Collections.Generic;
using System.Windows.Controls;
using System.Windows.Threading;
using System.ComponentModel;


namespace EOIR_Simulator.View
{
    public partial class MainView : Window
    {
        private readonly UDPReceiver _rx;
        private readonly MainViewModel _vm;

        private WriteableBitmap _wb;

        public MainView()
        {
            InitializeComponent();

            if (!DesignerProperties.GetIsInDesignMode(this))
            {
                _vm = (MainViewModel)DataContext;   // XAML에서 생성된 VM 참조
                _rx = new UDPReceiver(IcdConstants.UDP_PORT);
                _rx.FrameArrived += OnFrameArrived;
                _rx.Start();
            }
        }

        private void OnFrameArrived(FramePacket fp)
        {
            // JPEG → BitmapImage
            var bmp = new BitmapImage();
            using (var ms = new MemoryStream(fp.JpegBytes))
            {
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.StreamSource = ms;
                bmp.EndInit();
            }
            bmp.Freeze(); // cross‑thread 접근 안전

            Dispatcher.BeginInvoke(DispatcherPriority.Render, new Action(() =>
            {
                if (_wb == null ||
                    _wb.PixelWidth != bmp.PixelWidth ||
                    _wb.PixelHeight != bmp.PixelHeight ||
                    _wb.Format != bmp.Format)
                {
                    _wb = new WriteableBitmap(
                        bmp.PixelWidth, bmp.PixelHeight,
                        bmp.DpiX, bmp.DpiY,
                        bmp.Format,    // ← bmp.Format 그대로 사용 (대개 Bgr32/Bgra32)
                        null);
                }

                _wb.Lock();

                //WriteableBitmap 백버퍼 접근 경합 대응
                if (bmp.PixelWidth == 0) return;

                bmp.CopyPixels(
                    new Int32Rect(0, 0, bmp.PixelWidth, bmp.PixelHeight),
                    _wb.BackBuffer,
                    _wb.BackBufferStride * _wb.PixelHeight,
                    _wb.BackBufferStride);          // stride 충분
                _wb.AddDirtyRect(new Int32Rect(0, 0, _wb.PixelWidth, _wb.PixelHeight));
                _wb.Unlock();

                /* 3) ViewModel ­– Image 바인딩 갱신 */
                _vm.CurrentFrame = _wb;                // Image.Source 는 바인딩으로 자동 교체

                /* 4) 메타데이터 */
                _vm.ReplaceObjects(fp.Objects);
            }));
        }

        protected override void OnClosed(EventArgs e)
        {
            _rx?.Dispose();
            base.OnClosed(e);
        }
        
    }
}
