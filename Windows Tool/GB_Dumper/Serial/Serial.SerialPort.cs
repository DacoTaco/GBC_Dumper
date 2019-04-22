using System.Collections.Generic;
using System.IO.Ports;

namespace GB_Dumper.Serial
{
    public class WindowsSerialInterface : ISerialInterface
    {
        public event DataReadHandler OnDataToRead;
        private SerialPort _device = null;
        private void _device_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            OnDataToRead?.Invoke(this, new SerialEventArgs());
        }

        public bool IsOpen() => _device?.IsOpen == true;

        public int BytesToRead() => _device.BytesToRead;
  

        public IList<SerialDevice> ReloadDevices()
        {
            var devList = new List<SerialDevice>();

            foreach (var port in SerialPort.GetPortNames())
            {
                devList.Add(new SerialDevice(port, port));
            }

            return devList;
        }

        public void Open(SerialDevice device, int BaudRate)
        {
            if (IsOpen())
                return;

            _device = new SerialPort(device.Device, BaudRate, Parity.None, 8, StopBits.One)
            {
                Handshake = Handshake.None,
                ReadTimeout = 50,
                WriteTimeout = 50,
                ReceivedBytesThreshold = 0x1,
                ReadBufferSize = 0x2000
            };
            _device.DataReceived += _device_DataReceived;

            _device.Open();
        }

        public void Close()
        {
            if (_device == null || !IsOpen())
                return;

            _device.Close();
            _device.DataReceived -= _device_DataReceived;
            _device = null;
        }

        public void Write(string data)
        {
            if (_device == null || !IsOpen())
                return;

            _device.Write(data);
        }
        public void Write(byte[] data, int offset, int count)
        {
            if (_device == null || !IsOpen())
                return;

            _device.Write(data, offset, count);
        }

        public byte[] Read(int count)
        {
            if (_device == null || !IsOpen())
                return null;

            byte[] ret = new byte[count];
            _device.Read(ret,0,count);

            return ret;
        }
        public int ReadByte()
        {
            if (_device == null || !IsOpen())
                return 0;

            return _device.ReadByte();
        }
    }
}
