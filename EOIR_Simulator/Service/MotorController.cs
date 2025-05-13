using System;
using System.Threading;
using System.Threading.Tasks;

namespace EOIR_Simulator.Service
{
    public class MotorController : IDisposable
    {
        private readonly TcpSender _tcp;
        private CancellationTokenSource _scanCts;

        /* ── 두 축 각도 ───────────────────── */
        public int Yaw { get; private set; } = 90;   // 좌/우
        public int Pitch { get; private set; } = 90;   // 상/하
        public event Action<int, int> AngleChanged;    // (yaw,pitch)

        public MotorController(TcpSender tcp) { _tcp = tcp; }

        /* ── Clamp 함수 ───────────────────── */
        private static int Clip(int deg) => deg < 0 ? 0 : deg > 180 ? 180 : deg;

        /* ── 수동 제어 (±5° 단위) ─────────── */
        public async Task ManualMoveAsync(sbyte dx, sbyte dy)
        {
            Yaw = Clip(Yaw + dx);
            Pitch = Clip(Pitch + dy);
            AngleChanged?.Invoke(Yaw, Pitch);
            await _tcp.SendAsync(ModeNum.Manual, dx, dy);
        }

        /* ── 스캔 모드 : 좌/우 ±20°, 1° step ─ */
        public void StartScan()
        {
            if (_scanCts != null) return;
            _scanCts = new CancellationTokenSource();

            Task.Run(async () =>
            {
                int centerYaw = Yaw;
                int dir = 4;
                while (!_scanCts.IsCancellationRequested)
                {
                    int nextYaw = Clip(Yaw + dir);
                    sbyte dx = (sbyte)(nextYaw - Yaw);

                    Yaw = nextYaw;
                    AngleChanged?.Invoke(Yaw, Pitch);
                    await _tcp.SendAsync(ModeNum.Scan, dx, 0);

                    if (Yaw >= centerYaw + 20) dir = -4;
                    if (Yaw <= centerYaw - 20) dir = 4;

                    await Task.Delay(100, _scanCts.Token);
                }
            }, _scanCts.Token);
        }
        public void StopScan() { _scanCts?.Cancel(); _scanCts = null; }

        public void Dispose() => StopScan();
    }
}
