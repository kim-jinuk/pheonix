using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Data;
using System.IO;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;
using System.Windows.Media.Media3D;

namespace EOIR_Simulator.ViewModel
{
    public class MainViewModel : INotifyPropertyChanged
    {
        /*──────── 예외처리(Application.Current.Dispatcher) ────────*/
        private readonly System.Windows.Threading.Dispatcher _ui;

        /*──────── 네트워크 서비스 ────────*/
        private readonly TcpSender _tcp = new TcpSender("192.168.1.3", 9999);  // 필요 시 IP/PORT 수정
        private readonly UDPReceiver _rx;
        private readonly TcpAngleReceiver _angleReceiver;

        /*──────── 소켓 연결 속성 및 Command 선언 ────────*/
        public bool IsTcpConnected => _tcp.IsConnected;
        public bool IsTcpDisconnected => !IsTcpConnected;
        public string ConnectButtonText => IsTcpConnected ? "Disconnect" : "Connect";
        public ICommand ConnectCommand { get; }

        /* ★ MotorController 주입 */
        private readonly MotorController _motor;

        /* ───── Direction 벡터 (X, Y, Z) ───── */
        private double _dx, _dy, _dz;
        public double DirX { get => _dx; private set { _dx = value; OnPropertyChanged(); } }
        public double DirY { get => _dy; private set { _dy = value; OnPropertyChanged(); } }
        public double DirZ { get => _dz; private set { _dz = value; OnPropertyChanged(); } }

        /*──────── 모드 · 방향키 ───────────*/
        private ModeNum _mode = ModeNum.Manual;
        public ModeNum Mode
        {
            get => _mode;
            set
            {
                if (_mode == value) return;
                /* 1) 이전 스캔 멈춤 */
                //_motor.StopScan();

                _mode = value;
                OnPropertyChanged(nameof(Mode));
                OnPropertyChanged(nameof(IsManualMode));

                /* 2) 새 모드가 Scan 이면 즉시 StartScan */
                //if (_mode == ModeNum.Scan)
                //    _motor.StartScan();

                /* 3) 마지막에 모드 변경 패킷 전송 */
                _tcp.SendAsync(_mode, 0, 0).ConfigureAwait(false);
            }
        }
        public bool IsManualMode => _mode == ModeNum.Manual;

        public ICommand MoveCommand { get; }

        /*──────── ③ 수신 영상 · 객체 ─────────*/
        public ObservableCollection<ObjectInfo> Objects { get; }
            = new ObservableCollection<ObjectInfo>();

        private BitmapSource _currentFrame;


        /* 최근 프레임 시간 기록 */
        private DateTime _lastUdp = DateTime.MinValue;

        /* GUI 바인딩용 텍스트 */
        private string _udpStatus = "UDP : Disconnected";
        public string UdpStatus
        {
            get => _udpStatus;
            private set { _udpStatus = value; OnPropertyChanged(nameof(UdpStatus)); }
        }

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

        private string _connText = "TCP : Disconnected";
        public string ConnectionStatus
        {
            get => _connText;
            private set { _connText = value; OnPropertyChanged(nameof(ConnectionStatus)); }
        }

        /*──────── 각도 수신용 속성 (바인딩 가능) ────────*/
        private byte _angleX;
        private byte _angleY;
        //방향 벡터
        private Point3D _dirPoint = new Point3D(0, 0, 1);
        public Point3D DirPoint
        {
            get => _dirPoint;
            private set { _dirPoint = value; OnPropertyChanged(); }
        }
        //방향 벡터와 수직인 면
        private Vector3D _dirVector = new Vector3D(0, 0, 1);
        public Vector3D DirVector            // ★ Plane 의 Normal 바인딩용
        {
            get => _dirVector;
            private set { _dirVector = value; OnPropertyChanged(); }
        }

        //방향 벡터와 수직인 정육면체
        private Transform3D _cubeTransform = Transform3D.Identity;
        public Transform3D CubeTransform
        {
            get => _cubeTransform;
            private set { _cubeTransform = value; OnPropertyChanged(); }
        }

        /* 방향 업데이트 메서드 */
        private void UpdateDirection()
        {
            const double RAD = Math.PI / 180.0;

            /* ①  보드 값 → 라디안 */
            double yawRad = (AngleX - 90) * RAD;   // Yaw = AngleX
            double pitchRad = (AngleY - 90) * RAD;   // Pitch = AngleY

            /* ②  단위 방향벡터  (수평=+X, 위=+Z) */
            double cosP = Math.Cos(pitchRad);
            double dx = cosP * Math.Cos(yawRad);    // X
            double dy = cosP * Math.Sin(yawRad);    // Y
            double dz = Math.Sin(pitchRad);         // Z

            DirPoint = new Point3D(dx, dy, dz);
            DirVector = new Vector3D(dx, dy, dz);

            /* ③  회전(Z축 → DirVector) */
            Vector3D zAxis = new Vector3D(0, 0, 1);
            Vector3D axis = Vector3D.CrossProduct(zAxis, DirVector);
            double angle = Vector3D.AngleBetween(zAxis, DirVector);   // deg

            var rot = axis.Length < 1e-6
                      ? Transform3D.Identity
                      : new RotateTransform3D(new AxisAngleRotation3D(axis, angle));

            /* ④  화살표 방향으로 0.35 전진 */
            var trans = new TranslateTransform3D(dx * 0.35, dy * 0.35, dz * 0.35);

            /* ⑤  복합 변환 */
            var grp = new Transform3DGroup();
            grp.Children.Add(rot);
            grp.Children.Add(trans);
            CubeTransform = grp;
        }

        public byte AngleX
        {
            get => _angleX;
            set { _angleX = value; OnPropertyChanged(); UpdateDirection(); }
        }
        public byte AngleY
        {
            get => _angleY;
            set { _angleY = value; OnPropertyChanged(); UpdateDirection(); }
        }


        /* ───────── FrameArrived 핸들러 ───────── */
        private void OnFrameArrived(FramePacket fp)
        {
            // JPEG → BitmapImage (간단 버전)
            BitmapImage bmp;
            using (var ms = new MemoryStream(fp.JpegBytes))
            {
                bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.StreamSource = ms;
                bmp.EndInit();
                bmp.Freeze();                     // 크로스스레드 안전
            }

            _lastUdp = DateTime.UtcNow;

            if (Application.Current == null || _ui.HasShutdownStarted)
                return;                         // 앱이 닫히는 중이면 무시

            // UI 스레드로 배포
            _ui.BeginInvoke(new Action(() =>
            {
                CurrentFrame = bmp;
                ReplaceObjects(fp.Objects);
            }));
        }

        /*──────── ④ 생성자 ─────────────────*/
        public MainViewModel()
        {
            /*──────── 예외처리(BeginInvoke[AngleX,AngleY]) ────────*/
            _ui = Application.Current?.Dispatcher ??            // 정상 실행
                  System.Windows.Threading.Dispatcher.CurrentDispatcher; // 디자인/테스트

            _motor = new MotorController(_tcp);

            MoveCommand = new RelayCommand(async dirObj =>
            {
                if (!IsManualMode) return;
                var dir = dirObj as string;
                sbyte dx = 0, dy = 0;
                int speed = 10;

                switch (dir)
                {
                    case "Up": dy = (sbyte)speed; break;
                    case "Down": dy = (sbyte)-speed; break;
                    case "Left": dx = (sbyte)speed; break;
                    case "Right": dx = (sbyte)-speed; break;
                }
                await _tcp.SendAsync(ModeNum.Manual, dx, dy);
            });
            /* ── UdpFrameReceiver 구독 ── */
            _rx = new UDPReceiver(IcdConstants.UDP_PORT);
            _rx.FrameArrived += OnFrameArrived;
            _rx.Start();

            // TCPAngleReceiver
            _angleReceiver = new TcpAngleReceiver(9998);
            _angleReceiver.AngleReceived += (x, y) =>
            {
                if (_ui.HasShutdownStarted) return;     // 종료 중엔 무시

                _ui.BeginInvoke(new Action(() =>
                {
                    AngleX = x;   // setter → UpdateDirection()
                    AngleY = y;
                }));
            };
            _angleReceiver.Start();

            /* ── 500 ms 주기 타이머로 상태 검사 ── */
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
            }, _ => true);          // 버튼 항상 활성 (토글이므로)
        }

        /*──────── ⑤ INotifyPropertyChanged ──*/
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
