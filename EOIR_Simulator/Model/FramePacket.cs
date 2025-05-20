using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EOIR_Simulator.Model
{
    public class FramePacket
    {
        public uint FrameId;
        public List<ObjectInfo> Objects;
        public byte[] JpegBytes;
    }
}
