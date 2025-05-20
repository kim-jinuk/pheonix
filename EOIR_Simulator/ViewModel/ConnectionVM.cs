using System;
using System.Windows.Input;
using System.Timers;
using EOIR_Simulator.Service;
using EOIR_Simulator.Utils;

namespace EOIR_Simulator.ViewModel
{
    public sealed class ConnectionVM : ObservableObject
    {
        private readonly CommandSender _tcp;
        private readonly PacketReceiver _vrx;

        /* ── 상태 문자열 ─────────────────────────────── */
        private string _tcpStatus = "TCP : Disconnected";
        public string TcpStatus { get => _tcpStatus; private set { _tcpStatus = value; RaisePropertyChanged(); } }

        private string _udpStatus = "UDP : Disconnected";
        public string UdpStatus { get => _udpStatus; private set { _udpStatus = value; RaisePropertyChanged(); } }

        public string ConnectButtonText => _tcp.IsConnected ? "Disconnect" : "Connect";

        public ICommand ConnectCommand { get; }

        /* ── UDP 타임아웃용 ──────────────────────────── */
        private DateTime _lastUdp = DateTime.MinValue;
        private readonly Timer _timer = new Timer(500);   // 0.5 s

        public ConnectionVM(CommandSender tcp, PacketReceiver vrx)
        {
            _tcp = tcp;   
            _vrx = vrx;

            _tcp.StateChanged += s =>
            {
                TcpStatus = "TCP : " + s;
                RaisePropertyChanged(nameof(ConnectButtonText)); // 텍스트 갱신
            };

            _vrx.FrameArrived += _ =>
            {
                _lastUdp = DateTime.UtcNow;
                UdpStatus = "UDP : Connected";
            };

            /* 0.5 s 마다 마지막 수신 시각 검사 */
            _timer.Elapsed += (s, e) =>
            {
                var gap = DateTime.UtcNow - _lastUdp;
                if (gap.TotalSeconds > 1.5 && UdpStatus != "UDP : Disconnected")
                    UdpStatus = "UDP : Disconnected";
            };
            _timer.Start();

            ConnectCommand = new RelayCommand(async _ =>
            {
                if (_tcp.IsConnected)
                {
                    _tcp.Disconnect();
                    _lastUdp = DateTime.MinValue;          // 즉시 타임아웃 유도
                    UdpStatus = "UDP : Disconnected";
                }
                else
                    await _tcp.ConnectAsync();

                RaisePropertyChanged(nameof(ConnectButtonText));
            });
        }
    }
}