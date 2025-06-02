using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EOIR_Simulator.Model;

namespace EOIR_Simulator.Service
{
    /// <summary>ICD(6 B) 패킷을 TCP 로 전송 + 연결 상태 감시</summary>
    public class CommandSender : IDisposable
    {
        private readonly string _ip;
        private readonly int _port;
        private TcpClient _client;
        private NetworkStream _stream;

        private int _connecting;                 // 0/1 플래그 (중복 Connect 방지)
        private CancellationTokenSource _watchCts;

        public TcpState State { get; private set; } = TcpState.Disconnected;
        public bool IsConnected => State == TcpState.Connected;

        public event Action<TcpState> StateChanged;

        public CommandSender(string ip, int port)
        {
            _ip = ip;
            _port = port;
        }

        /* ─────────────────────────────────  연결  ────────────────────────────── */
        public Task<bool> ConnectAsync() => EnsureConnectedAsync();

        private async Task<bool> EnsureConnectedAsync()
        {
            if (State == TcpState.Connected && _client?.Connected == true)
                return true;

            if (Interlocked.Exchange(ref _connecting, 1) == 1)
                return false;                    // 이미 진행 중

            try
            {
                State = TcpState.Connecting; StateChanged?.Invoke(State);

                _client = new TcpClient();
                await _client.ConnectAsync(_ip, _port);
                _stream = _client.GetStream();

                EnableTcpKeepAlive(_client.Client, 5_000, 1_000);   // 5s idle, 1s probe
                StartWatch();

                State = TcpState.Connected; StateChanged?.Invoke(State);
                return true;
            }
            catch
            {
                HandleDisconnect();              // 연결 실패 → 즉시 Disconnected
                return false;
            }
            finally
            {
                Interlocked.Exchange(ref _connecting, 0);
            }
        }

        /* ─────────────────────────────  패킷 전송  ───────────────────────────── */
        public async Task SendAsync(ModeNum mode, sbyte dx, sbyte dy)
        {
            if (!await EnsureConnectedAsync().ConfigureAwait(false))
                return;

            try
            {
                byte[] buf = new byte[6];
                buf[0] = 0xA5; buf[1] = 0xA5;              // magic word
                ushort m = (ushort)mode;
                buf[2] = (byte)m; buf[3] = (byte)(m >> 8); // mode
                buf[4] = unchecked((byte)dx);
                buf[5] = unchecked((byte)dy);

                await _stream.WriteAsync(buf, 0, buf.Length).ConfigureAwait(false);
                await _stream.FlushAsync().ConfigureAwait(false);
            }
            catch { HandleDisconnect(); }
        }

        /* ───────────────────────────  연결 상태 감시  ─────────────────────────── */
        private void StartWatch()
        {
            _watchCts?.Cancel();
            _watchCts = new CancellationTokenSource();

            _ = Task.Run(async () =>
            {
                var sock = _client.Client;
                while (!_watchCts.IsCancellationRequested)
                {
                    try
                    {
                        // 상대가 FIN 을 보내면 Poll(Read) == true && Available == 0
                        if (sock.Poll(0, SelectMode.SelectRead) && sock.Available == 0)
                        {
                            HandleDisconnect();
                            return;
                        }
                    }
                    catch { /* 소켓 오류 → 이후 HandleDisconnect() 로 처리 */ }

                    await Task.Delay(1_000, _watchCts.Token).ConfigureAwait(false);
                }
            }, _watchCts.Token);
        }

        /* ─────────────────────────  Keep-Alive 세팅  ─────────────────────────── */
        private static void EnableTcpKeepAlive(Socket s, uint idleMs, uint intervalMs)
        {
            // [Enable=1][Idle][Interval]   (각 4바이트 little-endian)
            byte[] opt = new byte[12];
            BitConverter.GetBytes(1u).CopyTo(opt, 0);
            BitConverter.GetBytes(idleMs).CopyTo(opt, 4);
            BitConverter.GetBytes(intervalMs).CopyTo(opt, 8);
            s.IOControl(IOControlCode.KeepAliveValues, opt, null);
        }

        /* ─────────────────────────────  종료/정리  ───────────────────────────── */
        public void Disconnect() => HandleDisconnect();

        private void HandleDisconnect()
        {
            if (State == TcpState.Disconnected) return;

            State = TcpState.Disconnected; StateChanged?.Invoke(State);

            _watchCts?.Cancel();
            try { _stream?.Close(); _client?.Close(); } catch { /* ignore */ }
        }

        public void Dispose() => HandleDisconnect();
    }
}
