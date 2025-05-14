using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Data;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Windows.Media.Imaging;
using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public class MainViewModel : INotifyPropertyChanged
    {
        /*──────── 네트워크 서비스 ────────*/
        private readonly TcpSender _tcp = new TcpSender("192.168.3.141", 9999);  // 필요 시 IP/PORT 수정

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

        private string _connText = "TCP: Disconnected";
        public string ConnectionStatus
        {
            get => _connText;
            private set { _connText = value; OnPropertyChanged(nameof(ConnectionStatus)); }
        }

        /*──────── ④ 생성자 ─────────────────*/
        public MainViewModel()
        {
            _motor = new MotorController(_tcp);

            MoveCommand = new RelayCommand(async dirObj =>
            {
                if (!IsManualMode) return;
                var dir = dirObj as string;
                sbyte dx = 0, dy = 0;

                switch (dir)                 // C# 7.3 switch 문
                {
                    case "Up": dy = 5; break;
                    case "Down": dy = -5; break;
                    case "Left": dx = -5; break;
                    case "Right": dx = 5; break;
                }
                await _tcp.SendAsync(ModeNum.Manual, dx, dy);
            });
            _tcp.StateChanged += st => ConnectionStatus = "TCP: " + st;
        }

        /*──────── ⑤ INotifyPropertyChanged ──*/
        public event PropertyChangedEventHandler PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string p = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(p));
    }
}
