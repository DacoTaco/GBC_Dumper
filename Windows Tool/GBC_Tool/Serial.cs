using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialCommunication
{
    public sealed class Serial
    {
        private static readonly Serial instance = new Serial();
        public static Serial Instance { get { return instance; } }
        private Serial () {}

        public SerialPort connection = new SerialPort();
        private string[] comPorts;
        public string[] ComPorts 
        {
            get
            {
                return comPorts;
            }
            set
            {
                comPorts = value;
            }
        }
        public int[] BaudRates = { 300, 600, 1200, 2400, 4800, 9600, 14400,19200,38400,57600,115200,460800 };
    }
}
