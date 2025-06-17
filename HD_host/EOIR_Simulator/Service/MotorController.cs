using System;
using System.Net.Sockets;
using System.Threading.Tasks;
using EOIR_Simulator.Model;

namespace EOIR_Simulator.Service
{
    public class MotorController
    {
        private readonly TcpCmdChannel _tcp;

        public event Action<int, int> AngleChanged;

        public MotorController(TcpCmdChannel tcp)
        {
            _tcp = tcp;
        }

        // 방향 전송: "Yaw" 또는 "Pitch" / dir: 0 or 1
        public async Task SendDirectionAsync(string axis, byte dir)
        {
            byte cmd = 0xFF; // 초기화

            if (axis == "Yaw")
            {
                cmd = (dir == 0) ? (byte)0x00 : (byte)0x01;
            }
            else if (axis == "Pitch")
            {
                cmd = (dir == 0) ? (byte)0x03 : (byte)0x02;

            }

            if (cmd != 0xFF)
                await _tcp.SendCommandAsync((byte)CmdFlag.MoveMotor, cmd);
        }
    }
}
