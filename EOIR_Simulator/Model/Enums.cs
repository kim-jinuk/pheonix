using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EOIR_Simulator.Model
{
    public enum ModeNum : ushort { Manual = 0, Scan = 1, Track = 2 }

    public enum TcpState { Disconnected, Connecting, Connected }

    public enum UdpState { Disconnected, Connected }

    public enum SimState { Idle, Operating }
    public class Enums
    {
    }
}
