using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EOIR_Simulator.Model
{
    public class ObjectInfo   // ← 모델은 단순 POCO
    {
        public byte Class { get; set; }
        public byte X { get; set; }
        public byte Y { get; set; }
        public byte W { get; set; }
        public byte H { get; set; }
        public float Confidence { get; set; }
    }
}
