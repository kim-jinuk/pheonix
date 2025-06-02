using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;

using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;   // RelayCommand / ObservableObject 등


namespace EOIR_Simulator.ViewModel
{
    public class MainViewModel : INotifyPropertyChanged
    {
        /* 서비스 한 번만 생성 */
        private readonly CommandSender _tcp;
        private readonly PacketReceiver _vRx;

        /* 하위 VM */
        public VideoVM Video { get; }
        public AngleVM Angle { get; }
        public ConnectionVM Connection { get; }

        /* 서보 한계각에 의한 버튼 활성화/비활성화 */
        public bool CanLeft => IsManualMode && Angle.AngleX < 180;
        public bool CanRight => IsManualMode && Angle.AngleX > 0;
        public bool CanUp => IsManualMode && Angle.AngleY > 0;
        public bool CanDown => IsManualMode && Angle.AngleY < 180;

        /* 이동 */
        public ICommand MoveCommand { get; }

        public bool IsManualMode => Mode == ModeNum.Manual;

        public event PropertyChangedEventHandler PropertyChanged;
        private void Raise([CallerMemberName] string p = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(p));

        private ModeNum _mode = ModeNum.Manual;
        public ModeNum Mode
        {
            get => _mode;
            set
            {
                if (_mode == value) return;
                _mode = value;
                Raise();               // Mode
                Raise(nameof(IsManualMode));
                
                _ = SendModeAsync();
            }
        }

        private async Task SendModeAsync()
        {
            try { await _tcp.SendAsync(Mode, 0, 0); }
            catch 
            {
                //로그 처리 부분
            }
        }

        /* 상태 표시 */
        public SimState State          // ← UI 에 바인딩할 속성
        {
            get => _state;
            private set { _state = value; Raise(nameof(State)); }
        }
        private SimState _state = SimState.Idle;

        /* 생성자 */
        public MainViewModel()
        {
            _tcp = new CommandSender(IcdConstants.TCP_IP, IcdConstants.TCP_PORT);
            _vRx = new PacketReceiver(IcdConstants.UDP_PORT);

            _vRx.Start();

            Video = new VideoVM(_vRx);
            Angle = new AngleVM(_vRx);
            Connection = new ConnectionVM(_tcp, _vRx);

            MoveCommand = new RelayCommand(dirObj =>
            {
                if (!IsManualMode) return;
                var step = DirToStep(dirObj as string);
                _tcp.SendAsync(ModeNum.Manual, step.dx, step.dy).ConfigureAwait(false);
            });

            // TCP 연결 상태 → Simulator State 로 변환
            _tcp.StateChanged += s =>
            {
                if (s == TcpState.Connected)
                {
                    Mode = ModeNum.Manual;        //TCP 연결시 모드 초기화
                    State = SimState.Operating;
                    Video.AcceptFrames = true;
                }
                else
                {
                    State = SimState.Idle;
                    Video.AcceptFrames = false;
                    Video.Clear();                 // ← 프레임·메타 지우기
                }
            };

            /* Angle 값 변할 때 → 버튼 갱신 */
            Angle.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(AngleVM.AngleX) ||
                    e.PropertyName == nameof(AngleVM.AngleY))
                {
                    Raise(nameof(CanLeft)); Raise(nameof(CanRight));
                    Raise(nameof(CanUp)); Raise(nameof(CanDown));
                }
            };

            /* Mode 바뀔 때도 함께 갱신 */
            PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(IsManualMode))
                {
                    Raise(nameof(CanLeft)); Raise(nameof(CanRight));
                    Raise(nameof(CanUp)); Raise(nameof(CanDown));
                }
            };
        }

        //수동 모터 제어
        private static (sbyte dx, sbyte dy) DirToStep(string dir)
        {
            switch (dir)
            {
                case "Up": return (0, -5);
                case "Down": return (0, +5);
                case "Left": return (+5, 0);
                case "Right": return (-5, 0);
                default: return (0, 0);
            }
        }

        public async Task ShutdownAsync()
        {
            try
            {
                /* ② 정상 종료 직전에 Manual 한번 더 */
                await _tcp.SendAsync(ModeNum.Manual, 0, 0);
                await Task.Delay(50);              // 1 RTT 여유
            }
            catch { /* 로그만 */ }

            _tcp.Disconnect();                     // 소켓 정리
        }
    }
}
