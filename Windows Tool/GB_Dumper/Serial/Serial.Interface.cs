using System.Collections.Generic;

namespace GB_Dumper.Serial
{
    //passthrough class that relays everything to either the FTDI driver or the .NET Serial driver
    public sealed class SerialInterface
    {
        public static SerialInterface Instance { get; } = new SerialInterface();

        private SerialInterface()   { FTDIMode = false; }

        private ISerialInterface _ftdiInterface = new FTDIInterface();
        private ISerialInterface _windowsInterface = new WindowsSerialInterface();
        private ISerialInterface _serialInterface;

        public IList<SerialDevice> Devices => _serialInterface == null ? new List<SerialDevice>() : _serialInterface.ReloadDevices();
        public int[] BaudRates = { 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 250000, 460800, 500000, 1000000};

        private bool _FtdiMode = false;
        public bool FTDIMode
        {
            get => _FtdiMode;
            set
            {
                _FtdiMode = value;
                _serialInterface = _FtdiMode ? _ftdiInterface : _windowsInterface;
            }
        }

        private object _eventLock = new object();
        public event DataReadHandler OnDataToRead
        {
            add
            {
                if (_serialInterface == null)
                    return;

                lock (_eventLock)
                {
                    _serialInterface.OnDataToRead += value;
                }

            }
            remove
            {
                if (_serialInterface == null)
                    return;

                lock (_eventLock)
                {
                    _serialInterface.OnDataToRead -= value;
                }
            }
        }
        public bool IsOpen => _serialInterface == null ? false:_serialInterface.IsOpen();
        public int BytesToRead => _serialInterface == null ? 0 : _serialInterface.BytesToRead();

        public void OpenDevice(SerialDevice device,int BaudRate)
        {
            if (_serialInterface == null)
                return;

            _serialInterface.Open(device,BaudRate);
        }
        public void Close()
        {
            if (_serialInterface == null || IsOpen == false)
                return;

            _serialInterface.Close();
        }

        public void Write(string data)
        {
            if (_serialInterface == null || IsOpen == false)
                return;

            _serialInterface.Write(data);
        }
        public void Write(byte[] data, int offset, int count)
        {
            if (_serialInterface == null || IsOpen == false)
                return;

            _serialInterface.Write(data, offset, count);
        }

        public byte[] Read(int count)
        {
            if (_serialInterface == null || IsOpen == false)
                return null;

            return _serialInterface.Read(count);
        }
        public int ReadByte()
        {
            if (_serialInterface == null || IsOpen == false)
                return 0;

            return _serialInterface.ReadByte();
        }


    }
}
