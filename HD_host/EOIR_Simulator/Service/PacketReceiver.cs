/*
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;


using System.Runtime.InteropServices;


using System.Threading;
using System.Threading.Tasks;
using EOIR_Simulator.Model;
using EOIR_Simulator.ViewModel;

namespace EOIR_Simulator.Service
{
    public class PacketReceiver : IDisposable
    {
        //C++ 연동
        //[DllImport("FrameSender.dll", CallingConvention = CallingConvention.Cdecl)]
        //public static extern void InitSender(string pipeline);

        //[DllImport("FrameSender.dll", CallingConvention = CallingConvention.Cdecl)]
        //public static extern void SendFrame(byte[] jpegData, int length);

        //[DllImport("FrameSender.dll", CallingConvention = CallingConvention.Cdecl)]
        //public static extern void CloseSender();


        private readonly UdpClient _client;
        private readonly object _lock = new object();
        private CancellationTokenSource _cts;

        private readonly Dictionary<uint, Dictionary<ushort, byte[]>> _framePackets = new Dictionary<uint, Dictionary<ushort, byte[]>>();
        private readonly Dictionary<uint, int> _expectedCounts = new Dictionary<uint, int>();

        public event Action<FramePacket> FrameArrived;
        //public event Action<byte, byte> AngleReceived;

        public PacketReceiver(int port)
        {
            //_client = new UdpClient(port);
            _client = new UdpClient(new IPEndPoint(IPAddress.Any, port));
           // _client.Client.ReceiveBufferSize = 2 * 1024 * 1024; // DAN 추가
            Debug.WriteLine($"[UDP] bind {port}");
        }

        public void Start()
        {
            _cts = new CancellationTokenSource();
            //InitSender("appsrc name=mysrc is-live=true block=false format=GST_FORMAT_TIME ! " +
            //           "videoconvert ! video/x-raw,format=I420 ! openh264enc bitrate=500000 gop-size=30 ! rtph264pay config-interval=1 pt=96 ! " +
            //           "udpsink host=127.0.0.1 port=5005 sync=false async=false");
            ////Debug.WriteLine("[PacketReceiver] 수신 루프 시작됨");
            Task.Run(() => ReceiveLoop(_cts.Token));
        }

        public void Stop()
        {
            _cts?.Cancel();
        }

        public void Dispose()
        {
            Stop();
            //CloseSender(); // GStreamer 파이프라인 종료
            _client?.Dispose();
        }

        private const int HEADER_SIZE = 12;
        private const int OBJECTINFO_SIZE = 14;
        private const int META_SIZE = OBJECTINFO_SIZE * 5; // ✅ 고정 5개 객체
        private const int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;


        private async Task ReceiveLoop(CancellationToken token)
        {
            Debug.WriteLine("[ReceiveLoop] Waiting for UDP...");
            while (!token.IsCancellationRequested)
            {
                try
                {
                    DateTime time1 = DateTime.Now;
                    UdpReceiveResult res = await _client.ReceiveAsync();
                    DateTime time2 = DateTime.Now;
                    //Debug.WriteLine($"패킷 수신 시간 : {time1 - time2}");
                    byte[] pkt = res.Buffer;

                 //   Debug.WriteLine($"[RECV] From {res.RemoteEndPoint.Address}:{res.RemoteEndPoint.Port}, Size={res.Buffer.Length}, Time = {time2-time1}");

                    if (pkt.Length < PAYLOAD_OFFSET) continue;

                    // [0-3] Magic Word
                    uint magic = (uint)(pkt[0] << 24 | pkt[1] << 16 | pkt[2] << 8 | pkt[3]);
                    if (magic != 0xDEADBEEF) continue; // 식별자 불일치

                    //Debug.WriteLine($"[UDP] magic: {magic}");

                    // [4-7] Frame ID
                    uint frameId = (uint)(pkt[4] << 24 | pkt[5] << 16 | pkt[6] << 8 | pkt[7]);
                    // [8-9] Packet ID
                    ushort packetId = (ushort)((pkt[8] << 8) | pkt[9]);
                    // [10-11] Total Packets
                    ushort totalPackets = (ushort)((pkt[10] << 8) | pkt[11]);


                 //   Debug.WriteLine($"[UDP] FrameID: {frameId}, PacketID: {packetId}, TotalPackets: {totalPackets}");

                    //Console.WriteLine($"[TCP] State Received - Mode: {mode}, State: {state}, TPU: {tpu}, CAM: {cam}");

                    // [12-13] Motor angle
                    //byte nx = pkt[12];
                    //byte ny = pkt[13];

                    //Console.WriteLine($"[DEBUG] nx = {nx}, ny = {ny}");
                    //AngleReceived?.Invoke(nx, ny);



                    // [14 - ...] Object Info
                    List<ObjectInfo> objects = new List<ObjectInfo>();
                    for (int i = 0; i < 5; i++)
                    {
                        objects.Add(ObjectInfo.FromBytes(pkt, 14 + i * OBJECTINFO_SIZE));
                    }

                    string time = DateTime.Now.ToString("HH:mm:ss");

                    foreach (var obj in objects)
                    {
                        LoggerService.LogUdpDetection(
                            time,
                            (int)frameId,
                            obj.GetHashCode(), // 객체 ID 대체값, 또는 0
                            obj.Class,
                            obj.X,
                            obj.Y,
                            obj.W,
                            obj.H,
                            (int)obj.Confidence
                        );
                    }


                    // [offset 이후] Payload
                    byte[] payload = new byte[pkt.Length - PAYLOAD_OFFSET];
                    Buffer.BlockCopy(pkt, PAYLOAD_OFFSET, payload, 0, payload.Length);

                    lock (_lock)
                    {
                        if (!_framePackets.ContainsKey(frameId))
                        {
                            _framePackets[frameId] = new Dictionary<ushort, byte[]>();
                            _expectedCounts[frameId] = totalPackets;
                        } 

                        _framePackets[frameId][packetId] = payload;

                        if (_framePackets[frameId].Count == totalPackets)
                        {
                            DateTime assembleStart = DateTime.Now;
                            using (var ms = new MemoryStream())
                            {
                                bool allPacketsPresent = true;
                                for (ushort i = 0; i < totalPackets; i++)
                                {
                                    if (!_framePackets[frameId].TryGetValue(i, out var part))
                                    {
                                        allPacketsPresent = false;
                                        break;
                                    }
                                    ms.Write(part, 0, part.Length);
                                }

                                if (!allPacketsPresent)
                                {
                                    Debug.WriteLine($"[WARN] Frame {frameId} dropped due to missing packets");
                                    _framePackets.Remove(frameId);
                                    _expectedCounts.Remove(frameId);
                                    continue;
                                }

                                byte[] jpeg = ms.ToArray();
                                DateTime assembleEnd = DateTime.Now;
                                Debug.WriteLine($"[DEBUG]  프레임 {frameId} 조립 시간: {(assembleEnd - assembleStart).TotalMilliseconds} ms");
                                //Task.Run(() =>
                                //{
                                //    try
                                //    {
                                //        Debug.WriteLine($"[SendFrame] 호출 - {jpeg.Length} bytes");
                                //        SendFrame(jpeg, jpeg.Length);
                                //    }
                                //    catch (Exception ex)
                                //    {
                                //        Debug.WriteLine($"[SendFrame ERROR] {ex.Message}");
                                //    }
                                //});

                                FrameArrived?.Invoke(new FramePacket
                                {
                                    FrameId = frameId,
                                    JpegBytes = jpeg,
                                    Objects = objects
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

*/
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EOIR_Simulator.Model;
using EOIR_Simulator.ViewModel;

namespace EOIR_Simulator.Service
{
    public class PacketReceiver : IDisposable
    {
        private readonly UdpClient _client;
        private readonly object _lock = new object();
        private CancellationTokenSource _cts;

        private readonly Dictionary<uint, Dictionary<ushort, byte[]>> _framePackets = new Dictionary<uint, Dictionary<ushort, byte[]>>();
        private readonly Dictionary<uint, int> _expectedCounts = new Dictionary<uint, int>();
        private readonly Dictionary<uint, DateTime> _frameStartTimes = new Dictionary<uint, DateTime>();

        private readonly Queue<double> _frameIntervals = new Queue<double>();
        private readonly int _fpsWindowSize = 20;
        private DateTime _lastFrameTime = DateTime.MinValue;

        public event Action<FramePacket> FrameArrived;

        public PacketReceiver(int port)
        {
            _client = new UdpClient(new IPEndPoint(IPAddress.Any, port));
            Debug.WriteLine("[UDP] bind " + port);
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
            if (_client != null) _client.Dispose();
        }

        private const int HEADER_SIZE = 12;
        private const int OBJECTINFO_SIZE = 14;
        private const int META_SIZE = OBJECTINFO_SIZE * 5;
        private const int PAYLOAD_OFFSET = HEADER_SIZE + META_SIZE;

        private async Task ReceiveLoop(CancellationToken token)
        {
            Debug.WriteLine("[ReceiveLoop] Waiting for UDP...");
            while (!token.IsCancellationRequested)
            {
                DateTime t0 = DateTime.Now;
                try
                {
                    UdpReceiveResult res = await _client.ReceiveAsync();
                    DateTime t1 = DateTime.Now;

                    byte[] pkt = res.Buffer;
                    if (pkt.Length < PAYLOAD_OFFSET) continue;

                    uint magic = (uint)(pkt[0] << 24 | pkt[1] << 16 | pkt[2] << 8 | pkt[3]);
                    if (magic != 0xDEADBEEF) continue;

                    uint frameId = (uint)(pkt[4] << 24 | pkt[5] << 16 | pkt[6] << 8 | pkt[7]);
                    ushort packetId = (ushort)((pkt[8] << 8) | pkt[9]);
                    ushort totalPackets = (ushort)((pkt[10] << 8) | pkt[11]);

                    List<ObjectInfo> objects = new List<ObjectInfo>();
                    for (int i = 0; i < 5; i++)
                    {
                        objects.Add(ObjectInfo.FromBytes(pkt, 14 + i * OBJECTINFO_SIZE));
                    }

                    string time = DateTime.Now.ToString("HH:mm:ss");
                    foreach (ObjectInfo obj in objects)
                    {
                        LoggerService.LogUdpDetection(
                            time,
                            (int)frameId,
                            obj.GetHashCode(),
                            obj.Class,
                            obj.X,
                            obj.Y,
                            obj.W,
                            obj.H,
                            (int)obj.Confidence
                        );
                    }

                    byte[] payload = new byte[pkt.Length - PAYLOAD_OFFSET];
                    Buffer.BlockCopy(pkt, PAYLOAD_OFFSET, payload, 0, payload.Length);

                    lock (_lock)
                    {
                        if (!_framePackets.ContainsKey(frameId))
                        {
                            _framePackets[frameId] = new Dictionary<ushort, byte[]>();
                            _expectedCounts[frameId] = totalPackets;
                            _frameStartTimes[frameId] = t0;
                        }

                        _framePackets[frameId][packetId] = payload;

                        if (_framePackets[frameId].Count == totalPackets)
                        {
                            DateTime t2 = DateTime.Now;

                            MemoryStream ms = new MemoryStream();
                            bool allPacketsPresent = true;
                            for (ushort i = 0; i < totalPackets; i++)
                            {
                                byte[] part;
                                if (!_framePackets[frameId].TryGetValue(i, out part))
                                {
                                    allPacketsPresent = false;
                                    break;
                                }
                                ms.Write(part, 0, part.Length);
                            }

                            DateTime t3 = DateTime.Now;

                            if (!allPacketsPresent)
                            {
                                Debug.WriteLine("[WARN] Frame " + frameId + " dropped due to missing packets");
                                _framePackets.Remove(frameId);
                                _expectedCounts.Remove(frameId);
                                _frameStartTimes.Remove(frameId);
                                continue;
                            }

                            byte[] jpeg = ms.ToArray();
                            ms.Close();

                            double recvTime = (t1 - t0).TotalMilliseconds;
                            double waitTime = (t2 - t0).TotalMilliseconds;
                            double assembleTime = (t3 - t2).TotalMilliseconds;
                            double totalTime = (t3 - t0).TotalMilliseconds;

                            //if (_lastFrameTime != DateTime.MinValue)
                            //{
                            //    double interval = (t3 - _lastFrameTime).TotalSeconds;
                            //    _frameIntervals.Enqueue(interval);

                            //    if (_frameIntervals.Count > _fpsWindowSize)
                            //        _frameIntervals.Dequeue();

                            //    double averageInterval = 0;
                            //    foreach (var i in _frameIntervals)
                            //        averageInterval += i;
                            //    averageInterval /= _frameIntervals.Count;

                            //    double fps = 1.0 / averageInterval;
                            //    Debug.WriteLine($"[FRAME {frameId}] FPS(avg over {_fpsWindowSize}): {fps:F2}");
                            //}
                            //_lastFrameTime = t3;

                            if (_lastFrameTime != DateTime.MinValue)
                            {
                                double interval = (t3 - _lastFrameTime).TotalSeconds;
                                double fps = 1.0 / interval;
                                Debug.WriteLine($"[FRAME {frameId}] FPS: {fps:F2}");
                            }
                            _lastFrameTime = t3;

                            Debug.WriteLine("[FRAME " + frameId + "] ⏱ 수신: " + recvTime + "ms, 누적대기: " + waitTime + "ms, 조립: " + assembleTime + "ms, 총: " + totalTime + "ms");


                            FramePacket packet = new FramePacket();
                            packet.FrameId = frameId;
                            packet.JpegBytes = jpeg;
                            packet.Objects = objects;
                            FrameArrived?.Invoke(packet);

                            _framePackets.Remove(frameId);
                            _expectedCounts.Remove(frameId);
                            _frameStartTimes.Remove(frameId);
                        }
                    }
                }
                catch (ObjectDisposedException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Debug.WriteLine("[ERR] 수신 오류: " + ex.Message);
                }
            }
        }
    }
}
