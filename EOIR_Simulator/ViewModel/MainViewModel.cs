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
        private readonly AngleReceiver _aRx;

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

        public MainViewModel()
        {
            _tcp = new CommandSender("192.168.1.3", 9999);
            _vRx = new PacketReceiver(IcdConstants.UDP_PORT);
            _aRx = new AngleReceiver(9998);

            _vRx.Start();
            _aRx.Start();

            Video = new VideoVM(_vRx);
            Angle = new AngleVM(_aRx);
            Connection = new ConnectionVM(_tcp, _vRx);

            MoveCommand = new RelayCommand(dirObj =>
            {
                if (!IsManualMode) return;
                var step = DirToStep(dirObj as string);
                _tcp.SendAsync(ModeNum.Manual, step.dx, step.dy).ConfigureAwait(false);
            });
        }

        private static (sbyte dx, sbyte dy) DirToStep(string dir)
        {
            switch (dir)
            {
                case "Up": return (0, +10);
                case "Down": return (0, -10);
                case "Left": return (+10, 0);
                case "Right": return (-10, 0);
                default: return (0, 0);
            }
        }
    }
}
