using System;
using System.Collections.Generic;

namespace GB_Dumper.Serial
{
    public class SerialEventArgs : EventArgs
    {
        public SerialEventArgs()
        {
        }
    }
    public delegate void DataReadHandler(object source, SerialEventArgs e);

    public interface ISerialInterface
    {
        event DataReadHandler OnDataToRead;

        bool IsOpen();
        int BytesToRead();
        IList<SerialDevice> ReloadDevices();
        

        void Open(SerialDevice device, int BaudRate);
        void Close();
        void Write(string data);
        void Write(byte[] data, int offset, int count);
        byte[] Read(int count);
        int ReadByte();
    }
}
