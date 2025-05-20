using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EOIR_Simulator.Model;

namespace EOIR_Simulator.Service
{

    /// <summary>ICD(6 B) 패킷을 TCP 로 전송 + 연결 상태 알림</summary>
    public class CommandSender : IDisposable
    {
        private readonly string _ip;
        private readonly int _port;
        private TcpClient _client;
        private NetworkStream _stream;

        private int _connecting;                 // 0/1 플래그 (중복 Connect 방지)
        public TcpState State { get; private set; } = TcpState.Disconnected;

        public bool IsConnected => State == TcpState.Connected;

        /// <summary>연결 상태가 바뀌면 호출됩니다.</summary>
        public event Action<TcpState> StateChanged;

        public CommandSender(string ip, int port)
        {
            _ip = ip;
            _port = port;
        }

        /// <summary>필요할 때만 비동기 Connect 시도</summary>
        private async Task<bool> EnsureConnectedAsync()
        {
            if (State == TcpState.Connected && _client?.Connected == true)
                return true;

            if (Interlocked.Exchange(ref _connecting, 1) == 1)
                return false;                    // 이미 연결 시도 중

            try
            {
                State = TcpState.Connecting; StateChanged?.Invoke(State);

                _client = new TcpClient();
                await _client.ConnectAsync(_ip, _port);
                _stream = _client.GetStream();

                State = TcpState.Connected; StateChanged?.Invoke(State);
                return true;
            }
            catch
            {
                State = TcpState.Disconnected; StateChanged?.Invoke(State);
                return false;                    // 실패해도 예외 외부로 throw 안 함
            }
            finally
            {
                Interlocked.Exchange(ref _connecting, 0);
            }
        }

        public Task<bool> ConnectAsync() => EnsureConnectedAsync();

        /// <summary>ICD‑패킷 전송 (magic + mode + dx + dy = 6 byte)</summary>
        public async Task SendAsync(ModeNum mode, sbyte dx, sbyte dy)
        {
            if (!await EnsureConnectedAsync().ConfigureAwait(false))
                return;                           // 연결 안 됐으면 조용히 리턴

            try
            {
                byte[] buf = new byte[6];
                // magic_word 0xA5A5 (little‑endian)
                buf[0] = 0xA5; buf[1] = 0xA5;
                // mode_num
                ushort m = (ushort)mode;
                buf[2] = (byte)m; buf[3] = (byte)(m >> 8);
                // dx, dy
                buf[4] = unchecked((byte)dx);
                buf[5] = unchecked((byte)dy);

                await _stream.WriteAsync(buf, 0, buf.Length).ConfigureAwait(false);
                await _stream.FlushAsync().ConfigureAwait(false);
            }
            catch
            {
                State = TcpState.Disconnected; StateChanged?.Invoke(State);
            }
        }

        public void Disconnect()
        {
            if (State == TcpState.Connected)
            {
                try
                {
                    _stream?.Close(); _client?.Close();
                }
                finally
                {
                    State = TcpState.Disconnected; StateChanged?.Invoke(State);
                }
            }
        }

        public void Dispose()
        {
            _stream?.Dispose();
            _client?.Close();
        }
    }
}