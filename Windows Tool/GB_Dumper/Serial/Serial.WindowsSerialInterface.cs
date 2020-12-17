using System.Collections.Generic;
using System.IO.Ports;

namespace GB_Dumper.Serial
{
    public class WindowsSerialInterface : ISerialInterface
    {
        public event DataReadHandler OnDataToRead;
        public event DataErrorHandler OnErrorRaised;

        private SerialPort _device = null;
        private void _device_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            OnDataToRead?.Invoke(this, new SerialEventArgs());
        }
        private void _device_RaiseError(string type,string msg)
        {
            OnErrorRaised?.Invoke(type,msg);
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
                ReadTimeout = 100,
                WriteTimeout = 100,
                ReceivedBytesThreshold = 0x1,
                ReadBufferSize = 0x100000
            };
            _device.DataReceived += _device_DataReceived;
            _device.ErrorReceived += _device_ErrorReceived;

            _device.Open();
        }

        private void _device_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
        {
            if (e.EventType == SerialError.Overrun)
                _device_RaiseError(e.EventType.ToString(),"Received data while buffer is full(overrun)");
            else
                _device_RaiseError(e.EventType.ToString(),$"Unknown Serial error '{e.EventType.ToString()}' while receiving data");
        }

        public void Close()
        {
            if (_device == null || !IsOpen())
                return;

            _device.Close();
            _device.DataReceived -= _device_DataReceived;
            _device.ErrorReceived -= _device_ErrorReceived;
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
