using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Essy.FTDIWrapper;
using System.Timers;

namespace SerialCommunication
{
    //passthrough class that relays everything to either the FTDI driver or the .NET Serial driver
    public sealed class Serial
    {
        private static readonly Serial instance = new Serial();
        public static Serial Instance { get { return instance; } }
        public bool FtdiMode = false;
        private Serial() { }
        private ISerialDevice _ftdiDevice = new FTDIDevice();
        private ISerialDevice _serialDevice = new SerialPortDevice();
        private ISerialDevice _device;

        private IList<SerialDevice> _devices = null;
        public IList<SerialDevice> Devices
        {
            get
            {
                if (_devices == null)
                    ReloadDevices();
                return _devices;
            }
        }
        public int[] BaudRates = { 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 250000, 460800, 500000, 1000000};

        public bool IsOpen
        {
            get
            {
                if (_device == null)
                    return false;

                return _device.IsOpen();
            }
        }
        public int BytesToRead
        {
            get
            {
                if (_device == null)
                    return 0;

                return _device.BytesToRead();
            }
        }

        private object _eventLock = new object();
        public event DataReadHandler OnDataToRead
        {
            add
            {
                if (_device == null)
                    return;

                lock(_eventLock)
                {
                    _device.OnDataToRead += value;
                }
                
            }
            remove
            {
                if (_device == null)
                    return;

                lock (_eventLock)
                {
                    _device.OnDataToRead -= value;
                }
            }
        }


        public int ReloadDevices()
        {
            if (IsOpen)
            {
                Close();
            }

            if (FtdiMode)
                _device = _ftdiDevice;
            else
                _device = _serialDevice;

            _devices = _device.ReloadDevices();
            return _devices.Count;
        }

        public void Write(string data)
        {
            if (_device == null || IsOpen == false)
                return;

            _device.Write(data);
        }
        public void Write(byte[] data, int offset, int count)
        {
            if (_device == null || IsOpen == false)
                return;

            _device.Write(data, offset, count);
        }

        public byte[] Read(int count)
        {
            if (_device == null || IsOpen == false)
                return null;

            return _device.Read(count);
        }
        public int ReadByte()
        {
            if (_device == null || IsOpen == false)
                return 0;

            return _device.ReadByte();
        }

        public void Open(SerialDevice device, int BaudRate)
        {
            if (_device == null)
                return;

            _device.Open(device.Device, BaudRate);
        }
        public void Close()
        {
            if (_device == null || IsOpen == false)
                return;

            _device.Close();
            _devices = null;
        }

    }
}
