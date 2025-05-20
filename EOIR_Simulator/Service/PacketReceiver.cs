using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EOIR_Simulator.Model;

namespace EOIR_Simulator.Service
{
    public class PacketReceiver : IDisposable
    {
        private readonly UdpClient _client;
        private readonly object _lock = new object();
        private CancellationTokenSource _cts;

        private readonly Dictionary<uint, Dictionary<ushort, byte[]>> _framePackets = new Dictionary<uint, Dictionary<ushort, byte[]>>();
        private readonly Dictionary<uint, int> _expectedCounts = new Dictionary<uint, int>();

        public event Action<FramePacket> FrameArrived;

        public PacketReceiver(int port)
        {
            _client = new UdpClient(port);
            System.Diagnostics.Debug.WriteLine($"[UDP] bind {port}"); //Debug
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

        private async Task ReceiveLoop(CancellationToken token)
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    UdpReceiveResult res = await _client.ReceiveAsync();
                    byte[] pkt = res.Buffer;

                    if (pkt.Length < 8) continue;

                    uint frameId = (uint)(pkt[0] << 24 | pkt[1] << 16 | pkt[2] << 8 | pkt[3]);
                    ushort packetId = (ushort)((pkt[4] << 8) | pkt[5]);
                    ushort totalPackets = (ushort)((pkt[6] << 8) | pkt[7]);

                    byte[] payload = new byte[pkt.Length - 8];
                    Buffer.BlockCopy(pkt, 8, payload, 0, payload.Length);

                    lock (_lock)
                    {
                        if (!_framePackets.ContainsKey(frameId))
                        {
                            _framePackets[frameId] = new Dictionary<ushort, byte[]>();
                            _expectedCounts[frameId] = totalPackets;
                            //Debug.WriteLine($"[INIT] frame {frameId} 시작 (총 {totalPackets}개)");
                        }

                        var dict = _framePackets[frameId];
                        dict[packetId] = payload;

                        //Debug.WriteLine($"[RECV] frame {frameId} - packet {packetId}/{totalPackets}");

                        // 수신률 70% 이상이면 바로 보여주기
                        if (dict.Count >= totalPackets * 1)
                        {
                            //Debug.WriteLine($"[PARTIAL] frame {frameId} 조립 시도 ({dict.Count}/{totalPackets})");

                            using (var ms = new MemoryStream())
                            {
                                for (ushort i = 0; i < totalPackets; i++)
                                {
                                    if (dict.ContainsKey(i))
                                        ms.Write(dict[i], 0, dict[i].Length);
                                }

                                byte[] jpeg = ms.ToArray();
                                FrameArrived?.Invoke(new FramePacket
                                {
                                    FrameId = frameId,
                                    JpegBytes = jpeg,
                                    Objects = new List<ObjectInfo>()
                                });
                            }

                            _framePackets.Remove(frameId);
                            _expectedCounts.Remove(frameId);
                        }
                    }
                }
                catch (ObjectDisposedException) { break; }
                catch (Exception ex)
                {
                    Debug.WriteLine($"[ERR] 수신 오류: {ex.Message}");
                }
            }
        }
    }
}