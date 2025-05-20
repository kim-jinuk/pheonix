using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Data;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public class MainViewModel : INotifyPropertyChanged, IDisposable
    {
        /*──────── 네트워크 서비스 ────────*/
        private readonly TcpSender _tcp = new TcpSender("192.168.1.3", 9999);  // 필요 시 수정
        private readonly UDPReceiver _rx;
        private readonly TcpAngleReceiver _angleReceiver;

        /*──────── 소켓 연결 속성 및 Command 선언 ────────*/
        public bool IsTcpConnected => _tcp.IsConnected;
        public bool IsTcpDisconnected => !IsTcpConnected;
        public string ConnectButtonText => IsTcpConnected ? "Disconnect" : "Connect";
        public ICommand ConnectCommand { get; }

        /* ★ MotorController 주입 */
        private readonly MotorController _motor;

        /*──────── 모드 · 방향키 ───────────*/
        private ModeNum _mode = ModeNum.Manual;
        public ModeNum Mode
        {
            get => _mode;
            set
            {
                if (_mode == value) return;
                _mode = value;
                OnPropertyChanged(nameof(Mode));
                OnPropertyChanged(nameof(IsManualMode));
                _tcp.SendAsync(_mode, 0, 0).ConfigureAwait(false);
            }
        }

        public bool IsManualMode => _mode == ModeNum.Manual;

        public ICommand MoveCommand { get; }

        /*──────── 수신 영상 · 객체 ─────────*/
        public ObservableCollection<ObjectInfo> Objects { get; }
            = new ObservableCollection<ObjectInfo>();

        private BitmapSource _currentFrame;
        public BitmapSource CurrentFrame
        {
            get => _currentFrame;
            set { _currentFrame = value; OnPropertyChanged(); }
        }

        public void ReplaceObjects(IList<ObjectInfo> list)
        {
            Objects.Clear();
            foreach (var o in list) Objects.Add(o);
        }

        /*──────── 상태 표시 ─────────*/
        private DateTime _lastUdp = DateTime.MinValue;
        private string _udpStatus = "UDP : Disconnected";
        public string UdpStatus
        {
            get => _udpStatus;
            private set { _udpStatus = value; OnPropertyChanged(); }
        }

        private string _connText = "TCP : Disconnected";
        public string ConnectionStatus
        {
            get => _connText;
            private set { _connText = value; OnPropertyChanged(nameof(ConnectionStatus)); }
        }

        /*──────── 각도 수신용 속성 (바인딩 가능) ────────*/
        private byte _angleX;
        private byte _angleY;

        public byte AngleX
        {
            get => _angleX;
            set { _angleX = value; OnPropertyChanged(); }
        }

        public byte AngleY
        {
            get => _angleY;
            set { _angleY = value; OnPropertyChanged(); }
        }

        /*──────── FrameArrived 핸들러 ─────────*/
        private void OnFrameArrived(FramePacket fp)
        {
            BitmapImage bmp;
            using (var ms = new MemoryStream(fp.JpegBytes))
            {
                bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.StreamSource = ms;
                bmp.EndInit();
                bmp.Freeze();
            }

            _lastUdp = DateTime.UtcNow;

            if (Application.Current == null || Application.Current.Dispatcher.HasShutdownStarted)
                return;

            Application.Current.Dispatcher.BeginInvoke(new Action(() =>
            {
                CurrentFrame = bmp;
                ReplaceObjects(fp.Objects);
            }));
        }

        /*──────── 생성자 ─────────*/
        public MainViewModel()
        {
            _motor = new MotorController(_tcp);

            MoveCommand = new RelayCommand(async dirObj =>
            {
                if (!IsManualMode) return;
                var dir = dirObj as string;
                sbyte dx = 0, dy = 0;

                switch (dir)
                {
                    case "Up": dy = 5; break;
                    case "Down": dy = -5; break;
                    case "Left": dx = -5; break;
                    case "Right": dx = 5; break;
                }
                await _tcp.SendAsync(ModeNum.Manual, dx, dy);
            });

            _rx = new UDPReceiver(IcdConstants.UDP_PORT);
            _rx.FrameArrived += OnFrameArrived;
            _rx.Start();

            _angleReceiver = new TcpAngleReceiver(9998);
            _angleReceiver.AngleReceived += (x, y) =>
            {
                AngleX = x;
                AngleY = y;
            };
            _angleReceiver.Start();

            var timer = new System.Timers.Timer(500);
            timer.Elapsed += (s, e) =>
            {
                var delta = DateTime.UtcNow - _lastUdp;
                var newState = (delta.TotalSeconds < 1.5) ? UdpState.Connected : UdpState.Disconnected;
                string txt = "UDP : " + newState;

                if (txt != _udpStatus)
                    App.Current.Dispatcher.BeginInvoke(new Action(() => UdpStatus = txt));
            };
            timer.Start();

            _tcp.StateChanged += st =>
            {
                ConnectionStatus = "TCP : " + st;
                OnPropertyChanged(nameof(IsTcpConnected));
                OnPropertyChanged(nameof(IsTcpDisconnected));
                OnPropertyChanged(nameof(ConnectButtonText));
            };

            ConnectCommand = new RelayCommand(async _ =>
            {
                if (_tcp.IsConnected)
                    _tcp.Disconnect();
                else
                    await _tcp.ConnectAsync();
            }, _ => true);
        }

        /*──────── INotifyPropertyChanged ─────────*/
        public event PropertyChangedEventHandler PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string p = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(p));

        /*──────── 종료 처리 ─────────*/
        public void Dispose()
        {
            _rx?.Dispose();
            _tcp?.Dispose();
            _angleReceiver?.Dispose();
        }
    }
}
