using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;

using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public class MainViewModel : INotifyPropertyChanged
    {
        private readonly TcpCmdChannel _cmdTcp;
        private readonly TcpStateChannel _stateTcp;
        private readonly PacketReceiver _vRx;
        private readonly MotorController _motor;

        public VideoVM Video { get; }
        public AngleVM Angle { get; }
        public ConnectionVM Connection { get; }

        public ObservableCollection<LoggerEntry> TcpLogs { get; } = new ObservableCollection<LoggerEntry>();

        public ICommand SetManualModeCommand { get; private set; }
        public ICommand DirectionCommand { get; private set; }
        public ICommand SetEOCamCommand { get; private set; }
        public ICommand SetIRCamCommand { get; private set; }
        public ICommand SendPrepCommand { get; private set; }
        public ICommand SendTrackCommand { get; private set; }
        public ICommand RunCommand { get; private set; }
        public bool PrepContrast { get; set; }
        public bool PrepLaplacian { get; set; }
        public bool PrepDenoise { get; set; }
        public bool PrepDeblur { get; set; }
        public bool PrepClutter { get; set; }

        public bool IsManualMode => Mode == ModeNum.Manual;
        public string ModeText => Mode.ToEng();

        public event PropertyChangedEventHandler PropertyChanged;
        private void Raise([CallerMemberName] string p = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(p));

        public string ConnectionStateText => Connection.ConnectButtonText;
        public ICommand ConnectionCommand => Connection.ConnectCommand;
        //public ICommand RunCommand => Connection.RunCommand;

        private ModeNum _mode = ModeNum.Manual;
        public ModeNum Mode
        {
            get => _mode;
            set
            {
                if (_mode == value) return;
                _mode = value;
                Raise();
                Raise(nameof(IsManualMode));
                _ = LogAndSendCommandAsync((byte)CmdFlag.ModeNum, (byte)Mode);
            }
        }

        private string _selectedPreprocessMode = "일반환경";
        public string SelectedPreprocessMode
        {
            get => _selectedPreprocessMode;
            set
            {
                if (_selectedPreprocessMode != value)
                {
                    _selectedPreprocessMode = value;
                    Raise();
                    Debug.WriteLine($"[UI] 영상 전처리 모드 선택: {value}");
                }
            }
        }

        public MainViewModel()
        {
            Debug.WriteLine("MainViewModel Created: " + GetHashCode());

            _cmdTcp = new TcpCmdChannel("192.168.1.3", 9999);
            _stateTcp = new TcpStateChannel("192.168.1.3", 9000);
            _vRx = new PacketReceiver(IcdConstants.UDP_PORT);
            _motor = new MotorController(_cmdTcp);

            Video = new VideoVM(_vRx);
            Angle = new AngleVM(_vRx, _stateTcp);
            Connection = new ConnectionVM(_stateTcp, _cmdTcp, _vRx, _ => { });

            Connection.ConnectTextChanged += () =>
            {
                Raise(nameof(ConnectionStateText));
            };

            InitializeCommands();
            SubscribeEvents();
        }

        private void InitializeCommands()
        {
            SetManualModeCommand = new RelayCommand(_ => SetManualMode());
            DirectionCommand = new RelayCommand(param => HandleArrowKey(param));
            SetEOCamCommand = new RelayCommand(_ => SendCamCommand(CamType.EO));
            SetIRCamCommand = new RelayCommand(_ => SendCamCommand(CamType.IR));
            SendPrepCommand = new RelayCommand(_ => SendPrepOptions());
            SendTrackCommand = new RelayCommand(_ => LogAndSendCommandAsync((byte)CmdFlag.Track, 0));
            RunCommand = new RelayCommand(_ => StartServices());
        }

        private void SetManualMode()
        {
            Mode = ModeNum.Manual;
            _ = LogAndSendCommandAsync((byte)CmdFlag.ModeNum, (byte)ModeNum.Manual);
        }

        private void HandleArrowKey(object param)
        {
            var dir = param as string;
            if (!IsManualMode || dir == null) return;

            byte flag = (byte)CmdFlag.MoveMotor;
            byte cmd = 0;

            switch (dir)
            {
                case "Down": cmd = 0b10; break; // Pitch CW
                case "Up": cmd = 0b11; break; // Pitch CCW
                case "Left": cmd = 0b01; break; // Yaw CCW
                case "Right": cmd = 0b00; break; // Yaw CW
                default: return;
            }

            _ = LogAndSendCommandAsync(flag, cmd); // 여기서만 1회 전송 (로그 포함)
        }


        private void SendCamCommand(CamType cam)
        {
            _ = LogAndSendCommandAsync((byte)CmdFlag.CamNum, (byte)cam);
        }

        private void SendPrepOptions()
        {
            byte mask = 0;
            if (PrepContrast) mask |= 0b00000001;
            if (PrepLaplacian) mask |= 0b00000010;
            if (PrepDenoise) mask |= 0b00000100;
            if (PrepDeblur) mask |= 0b00001000;
            if (PrepClutter) mask |= 0b00010000;

            _ = LogAndSendCommandAsync((byte)CmdFlag.PrepOpt, mask);
        }

        private async Task LogAndSendCommandAsync(byte flag, byte cmd)
        {
            string text = ConvertFlagToText(flag, cmd);
            Application.Current.Dispatcher.Invoke(() =>
            {
                TcpLogs.Insert(0, new LoggerEntry(DateTime.Now, flag, text, isAck: false));
            });

            try
            {
                await _cmdTcp.SendCommandAsync(flag, cmd);
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[ERR] SendCommandAsync 실패: {ex.Message}");
            }
        }

        private string ConvertFlagToText(byte flag, byte cmd)
        {
            switch ((CmdFlag)flag)
            {
                case CmdFlag.ModeNum:
                    return $"{(ModeNum)cmd} mode change";
                case CmdFlag.CamNum:
                    return cmd == 0 ? "EO cam selection " : "IR cam selection ";
                case CmdFlag.PrepOpt:
                    return $"전처리 마스크: 0x{cmd:X2}";
                case CmdFlag.MoveMotor:
                    {
                        byte directionBits = (byte)(cmd & 0b00000011);
                        string axis = (directionBits & 0b10) == 0 ? "Yaw" : "Pitch";
                        string dir = (axis == "Yaw")
                            ? ((directionBits & 0b01) == 0 ? "right" : "left")
                            : ((directionBits & 0b01) == 0 ? "down" : "up");

                        return $"{axis} motor {dir}";
                    }
                case CmdFlag.Track:
                    return "tracking start";
                default:
                    return $"알 수 없는 명령 (flag: 0x{flag:X2}, cmd: 0x{cmd:X2})";
            }
        }

        private void SubscribeEvents()
        {
            _cmdTcp.CommandAckReceived += (flag, cmd) =>
            {
                Application.Current.Dispatcher.Invoke(() =>
                {
                    var logText = ConvertFlagToText(flag, cmd);
                    TcpLogs.Insert(0, new LoggerEntry(DateTime.Now, flag, logText, isAck: true));
                });
            };

            _stateTcp.StateReceived += OnStateReceived;
        }

        private void OnStateReceived(StatePacket pkt)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                Connection?.UpdateState(pkt);
                Mode = (ModeNum)pkt.Mode;
            });
        }

        public void StartServices()
        {
            _vRx?.Start();
        }
    }
}
