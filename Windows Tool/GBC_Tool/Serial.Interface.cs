using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialCommunication
{
    public class SerialEventArgs : EventArgs
    {
        public SerialEventArgs()
        {
        }
    }
    public delegate void DataReadHandler(object source, SerialEventArgs e);

    public interface ISerialDevice
    {
        event DataReadHandler OnDataToRead;   

        bool IsOpen();
        int BytesToRead();
        IList<SerialDevice> ReloadDevices();

        void Open(string device, int BaudRate);
        void Close();
        void Write(string data);
        void Write(byte[] data, int offset, int count);
        byte[] Read(int count);
        int ReadByte();
    }
}
