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
        }

        //수동 모터 제어
        private static (sbyte dx, sbyte dy) DirToStep(string dir)
        {
            switch (dir)
            {
                case "Up": return (0, -10);
                case "Down": return (0, +10);
                case "Left": return (+10, 0);
                case "Right": return (-10, 0);
                default: return (0, 0);
            }
        }
    }
}
