using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media.Imaging;
using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public sealed class VideoVM : ObservableObject
    {
        private readonly PacketReceiver _rx;
        private readonly System.Windows.Threading.Dispatcher _ui =
            Application.Current.Dispatcher;

        public bool AcceptFrames { get; set; } = true;

        private BitmapSource _currentFrame;
        public BitmapSource CurrentFrame
        {
            get => _currentFrame;
            private set { _currentFrame = value; RaisePropertyChanged(); }
        }

        public ObservableCollection<ObjectInfo> Objects { get; }
            = new ObservableCollection<ObjectInfo>();

        public VideoVM(PacketReceiver rx)
        {
            _rx = rx;
            _rx.FrameArrived += OnFrame;
        }
        private void OnFrame(FramePacket fp)
        {
            if (!AcceptFrames)
            {
                Debug.WriteLine("[VideoVM] Frame ignored due to AcceptFrames = false");
                return;
            }

            Debug.WriteLine("[VideoVM] Frame received. Size: " + (fp.JpegBytes != null ? fp.JpegBytes.Length.ToString() : "null") + " bytes");

            // JPEG 디코딩을 백그라운드 스레드에서 실행
            Task.Run(() =>
            {
                Stopwatch swDecode = Stopwatch.StartNew();

                BitmapImage bmp = null;
                try
                {
                    using (var ms = new MemoryStream(fp.JpegBytes))
                    {
                        bmp = new BitmapImage();
                        bmp.BeginInit();
                        bmp.CacheOption = BitmapCacheOption.OnLoad;
                        bmp.StreamSource = ms;
                        bmp.EndInit();
                        bmp.Freeze(); // UI 쓰레드에서 안전하게 사용 가능
                    }
                }
                catch (Exception ex)
                {
                    Debug.WriteLine("[VideoVM] JPEG decode error: " + ex.Message);
                    return;
                }

                swDecode.Stop();
                Debug.WriteLine("[VideoVM] 🧠 JPEG 디코딩 시간: " + swDecode.Elapsed.TotalMilliseconds + " ms");

                // UI 쓰레드에서 이미지 갱신
                _ui.BeginInvoke(new Action(() =>
                {
                    Stopwatch swUi = Stopwatch.StartNew();

                    CurrentFrame = bmp;

                    Objects.Clear();
                    foreach (var o in fp.Objects)
                        Objects.Add(o);

                    swUi.Stop();
                    Debug.WriteLine("[VideoVM] 🎨 UI 렌더링 시간: " + swUi.Elapsed.TotalMilliseconds + " ms");
                }));
            });
        }



        public void Clear()
        {
            CurrentFrame = null;
            Objects.Clear();
        }
    }
}
