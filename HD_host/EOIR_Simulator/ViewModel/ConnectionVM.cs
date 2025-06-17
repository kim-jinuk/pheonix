
using System;
using System.Windows.Input;
using System.Timers;
using System.Windows.Media;
using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public enum AppConnectionState
    {
        Initial,
        Connected,
        Running,
        StoppedAfterRun
    }

    public sealed class ConnectionVM : ObservableObject
    {
        private readonly TcpStateChannel _tcpState;
        private readonly TcpCmdChannel _tcpCmd;
        private readonly PacketReceiver _vrx;
        private readonly Action<bool> _setDeviceState;

        private AppConnectionState _appState = AppConnectionState.Initial;

        private string _tcpStatus = "TCP : Disconnected";
        public string TcpStatus
        {
            get => _tcpStatus;
            private set
            {
                _tcpStatus = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(TcpStateColor));
                RaisePropertyChanged(nameof(ConnectButtonText));
                RaisePropertyChanged(nameof(TpuStateColor));
                RaisePropertyChanged(nameof(CamStateColor));
            }
        }

        private string _udpStatus = "UDP : Disconnected";
        public string UdpStatus
        {
            get => _udpStatus;
            private set
            {
                _udpStatus = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(UdpStateColor));
            }
        }

        private string _cmdStatus = "CMD : Disconnected";
        public string CmdStatus
        {
            get => _cmdStatus;
            private set
            {
                _cmdStatus = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(CmdStateColor));
            }
        }

        private bool _tpuConnected;
        public bool TpuConnected
        {
            get => _tpuConnected;
            private set
            {
                _tpuConnected = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(TpuStateColor));
            }
        }

        private bool _camConnected;
        public bool CamConnected
        {
            get => _camConnected;
            private set
            {
                _camConnected = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(CamStateColor));
            }
        }

        private int _stateCode;
        public int StateCode
        {
            get => _stateCode;
            private set
            {
                _stateCode = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(StateString));
                RaisePropertyChanged(nameof(StateColor));
            }
        }

        public string StateString => _stateCode.ToStateString();
        public Brush StateColor
        {
            get
            {
                switch (_stateCode)
                {
                    case 0: return Brushes.Red; // CHECKING
                    case 1: return Brushes.Gray;      // IDLE
                    case 2: return Brushes.LimeGreen; // RUNNING
                    default: return Brushes.Red;
                }
            }
        }


        private byte _modeNum;
        public byte ModeNum
        {
            get => _modeNum;
            private set
            {
                _modeNum = value;
                RaisePropertyChanged();
                RaisePropertyChanged(nameof(ModeString));
            }
        }

        public string ModeString => ((ModeNum)_modeNum).ToEng();

        public Brush TcpStateColor => TcpStatus.Contains("Connected") ? Brushes.LimeGreen : Brushes.Red;
        public Brush CmdStateColor => CmdStatus.Contains("Connected") ? Brushes.LimeGreen : Brushes.Red;
        public Brush TpuStateColor => TcpStatus.Contains("Connected") ? (TpuConnected ? Brushes.LimeGreen : Brushes.Red) : Brushes.Gray;
        public Brush CamStateColor => TcpStatus.Contains("Connected") ? (CamConnected ? Brushes.LimeGreen : Brushes.Red) : Brushes.Gray;
        public Brush UdpStateColor => UdpStatus.Contains("Connected") ? Brushes.LimeGreen : Brushes.Red;

        public string RunButtonText => _tcpCmd.IsConnected ? "Stop" : "Run";
        public string ConnectButtonText => _tcpState.IsConnected ? "Disconnect" : "Connect";

        public bool IsConnectEnabled => _appState == AppConnectionState.Initial || _appState == AppConnectionState.StoppedAfterRun;
        public bool IsRunEnabled => _appState == AppConnectionState.Connected ||  _appState == AppConnectionState.Running ||_appState == AppConnectionState.StoppedAfterRun;

        public bool AreOtherButtonsEnabled => _appState == AppConnectionState.Running;

        public ICommand ConnectCommand { get; }
        public ICommand RunCommand { get; }

        public event Action ConnectTextChanged;

        private DateTime _lastUdp = DateTime.MinValue;
        private readonly Timer _timer = new Timer(500);

        public ConnectionVM(TcpStateChannel tcpState, TcpCmdChannel tcpCmd, PacketReceiver vrx, Action<bool> setDeviceState)
        {
            _tcpState = tcpState;
            _tcpCmd = tcpCmd;
            _vrx = vrx;
            _setDeviceState = setDeviceState;

            _vrx.FrameArrived += _ =>
            {
                _lastUdp = DateTime.UtcNow;
                UdpStatus = "UDP : Connected";
            };

            _timer.Elapsed += (s, e) =>
            {
                if ((DateTime.UtcNow - _lastUdp).TotalSeconds > 1.5 && UdpStatus != "UDP : Disconnected")
                    UdpStatus = "UDP : Disconnected";
            };
            _timer.Start();

            _tcpCmd.StateChanged += connected =>
            {
                CmdStatus = connected ? "CMD : Connected" : "CMD : Disconnected";
                RaisePropertyChanged(nameof(RunButtonText));
            };

            ConnectCommand = new RelayCommand(async _ =>
            {
                if (_tcpState.IsConnected)
                {
                    _tcpState.Disconnect();
                    _tcpCmd.Disconnect();
                    _vrx.Stop();
                    TcpStatus = "TCP : Disconnected";
                    _lastUdp = DateTime.MinValue;
                    UdpStatus = "UDP : Disconnected";
                    CmdStatus = "CMD : Disconnected";
                    TpuConnected = false;
                    CamConnected = false;
                    _setDeviceState(false);

                    _appState = AppConnectionState.Initial;
                }
                else
                {
                    _vrx.Start();
                    bool success = await _tcpState.ConnectAsync();
                    TcpStatus = success ? "TCP : Connected" : "TCP : Failed";
                    _setDeviceState(success);
                    _appState = success ? AppConnectionState.Connected : AppConnectionState.Initial;
                }
                RaiseAllStateProperties();
                ConnectTextChanged?.Invoke();
            });

            RunCommand = new RelayCommand(async _ =>
            {
                if (_tcpCmd.IsConnected)
                {
                    _tcpCmd.Disconnect();
                    _appState = AppConnectionState.StoppedAfterRun;
                }
                else
                {
                    await _tcpCmd.ConnectAsync();
                    _appState = AppConnectionState.Running;
                }
                RaiseAllStateProperties();
            });
        }

        private void RaiseAllStateProperties()
        {
            RaisePropertyChanged(nameof(IsConnectEnabled));
            RaisePropertyChanged(nameof(IsRunEnabled));
            RaisePropertyChanged(nameof(AreOtherButtonsEnabled));
            RaisePropertyChanged(nameof(RunButtonText));
            RaisePropertyChanged(nameof(ConnectButtonText));
        }

        public void UpdateState(StatePacket packet)
        {
            StateCode = packet.State;
            TpuConnected = packet.Tpu;
            CamConnected = packet.Cam;
            ModeNum = (byte)packet.Mode;
        }
    }
}
