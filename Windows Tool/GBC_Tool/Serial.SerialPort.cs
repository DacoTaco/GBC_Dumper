using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialCommunication
{
    public class SerialPortDevice : ISerialDevice
    {
        public event DataReadHandler OnDataToRead;
        private SerialPort _device = null;
        private void _device_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            if (OnDataToRead != null)
                OnDataToRead(this, new SerialEventArgs());
        }

        public bool IsOpen()
        {
            if (_device == null)
                return false;

            return _device.IsOpen;
        }

        public int BytesToRead()
        {
            return _device.BytesToRead;
        }

        public IList<SerialDevice> ReloadDevices()
        {
            var devList = new List<SerialDevice>();

            foreach (var port in SerialPort.GetPortNames())
            {
                devList.Add(new SerialDevice(port, port));
            }

            return devList;
        }

        public void Open(string device, int BaudRate)
        {
            if (IsOpen())
                return;

            _device = new SerialPort(device,BaudRate,Parity.None,8,StopBits.One);
            _device.Handshake = Handshake.None;
            _device.ReadTimeout = 200;
            _device.WriteTimeout = 50;
            _device.ReceivedBytesThreshold = 0x1;
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
