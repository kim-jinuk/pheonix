using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Media.Imaging;
using EOIR_Simulator.Model;

namespace EOIR_Simulator.Service
{

    internal class UDPReceiver : IDisposable
    {
        private readonly UdpClient _client;
        private readonly Dictionary<uint, MemoryStream> _frames =
            new Dictionary<uint, MemoryStream>();

        public event Action<FramePacket> FrameArrived;

        private readonly object _lock = new object();
        private CancellationTokenSource _cts;

        public UDPReceiver(int port)
        {
            _client = new UdpClient(port);
        }

        public void Start()
        {
            _cts = new CancellationTokenSource();
            Task.Run(() => ReceiveLoop(_cts.Token));
        }

        public void Stop()
        {
            _cts?.Cancel();
        }

        public void Dispose()
        {
            Stop();
            _client?.Dispose();
        }

        // ─────────────────────────────────────────────────────────────

        private async Task ReceiveLoop(CancellationToken token)
        {
            var ep = new IPEndPoint(IPAddress.Any, 0);

            while (!token.IsCancellationRequested)
            {
                UdpReceiveResult res;
                try { res = await _client.ReceiveAsync(); }
                catch (ObjectDisposedException) { break; }

                var pkt = res.Buffer;

                // ── DEBUG #1: 패킷 기본 정보 ──────────────────────────
                Console.WriteLine($"[PKT] size={pkt.Length}  first4=0x{BitConverter.ToUInt32(pkt, 0):X8}");

                if (pkt.Length < 8) continue;                 // 최소 magic+fid

                uint magic = BitConverter.ToUInt32(pkt, 0);
                if (magic == IcdConstants.MAGIC_WORD)
                {
                    // ┌─ 새 프레임 시작
                    uint fid = BitConverter.ToUInt32(pkt, 4);
                    lock (_lock)
                    {
                        MemoryStream ms;
                        if (!_frames.TryGetValue(fid, out ms))
                        {
                            ms = new MemoryStream();
                            _frames.Add(fid, ms);
                        }
                        ms.SetLength(0);          // 덮어쓰기
                        ms.Write(pkt, 0, pkt.Length);
                        TryFinalize(fid);
                    }
                }
                else
                {
                    // ┌─ 이어지는 JPEG 조각
                    lock (_lock)
                    {
                        if (_frames.Count == 0) continue;
                        uint lastFid = _frames.Keys.Max();
                        MemoryStream ms = _frames[lastFid];
                        ms.Write(pkt, 0, pkt.Length);
                        TryFinalize(lastFid);
                    }
                }
            }
        }

        /// <summary>
        /// JPEG EOI(0xFFD9)까지 도착했으면 파싱 후 이벤트 발생
        /// </summary>
        private void TryFinalize(uint fid)
        {
            MemoryStream ms;
            if (!_frames.TryGetValue(fid, out ms)) return;

            byte[] buf = ms.GetBuffer();
            int len = (int)ms.Length;

            // ── 추가 로그 ✦✦✦
            Console.WriteLine($"[CHECK] fid={fid} len={len} last2={buf[Math.Max(0, len - 2)]:X2}{buf[Math.Max(1, len - 1)]:X2}");

            if (len < IcdConstants.HEADER_SIZE + 2)
            {
                Console.WriteLine($"[RET] len<{IcdConstants.HEADER_SIZE + 2}");
                return;
            }
            if (buf[len - 2] != 0xFF || buf[len - 1] != 0xD9)
            {
                Console.WriteLine("[RET] no EOI FF D9");
                return;
            } // JPEG 종료 확인

            // ── 헤더 파싱 ────────────────────────────────────────────
            var objList = new List<ObjectInfo>(IcdConstants.OBJ_COUNT_MAX);
            int idx = 8; // magic(4)+fid(4)

            for (int i = 0; i < IcdConstants.OBJ_COUNT_MAX; i++)
            {
                if (idx + IcdConstants.OBJ_SIZE > len) break;

                var info = new ObjectInfo
                {
                    Class = buf[idx++],
                    X = buf[idx++],
                    Y = buf[idx++],
                    W = buf[idx++],
                    H = buf[idx++],
                    Confidence = BitConverter.ToSingle(buf, idx)
                };
                idx += 4; // conf

                objList.Add(info);
            }

            int jpegLen = len - idx;
            var jpeg = new byte[jpegLen];
            Array.Copy(buf, idx, jpeg, 0, jpegLen);

            var packet = new FramePacket
            {
                FrameId = fid,
                Objects = objList,
                JpegBytes = jpeg
            };
            FrameArrived?.Invoke(packet);

            // ── DEBUG #2: 헤더까지 누적된 바이트 ──────────────
            Console.WriteLine($"[HDR] streamLen={ms.Length}");

            _frames.Remove(fid);  // 메모리 해제
        }
    }
}
