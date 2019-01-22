using Essy.FTDIWrapper;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management;
using System.Text;
using System.Timers;

namespace SerialCommunication
{
    public class FTDIDevice : ISerialDevice
    {
        private FTDI_Device _FtdiDevice = null;
        private FTDI_DeviceInfo _FtdiInfo;

        //because nor FTDI's driver nor the wrapper has an event...
        private static System.Timers.Timer aTimer = new System.Timers.Timer(0.5);
        public event DataReadHandler OnDataToRead;
        private void timerTest(Object source, ElapsedEventArgs e)
        {
            if (BytesToRead() > 0 && OnDataToRead != null)
            {
                OnDataToRead(this, new SerialEventArgs());
            }
        }

        public int BytesToRead()
        {
            if (_FtdiDevice == null || IsOpen() == false)
                return 0;
            return Convert.ToInt32(_FtdiDevice.NumberOfBytesInReceiveBuffer());
        }
        public bool IsOpen()
        {
            if (_FtdiInfo == null)
                return false;

            return _FtdiInfo.DeviceOpened;
        }

        public void Write(string data)
        {
            Write(Encoding.GetEncoding("ISO-8859-1").GetBytes(data), 0, data.Length);
        }
        public void Write(byte[] data, int offset, int count)
        {
            if (_FtdiDevice == null || IsOpen() == false)
                return;

            _FtdiDevice.WriteBytes(data);
        }

        public byte[] Read(int count)
        {
            if (_FtdiDevice == null || IsOpen() == false)
                return null;

            byte[] data = _FtdiDevice.ReadBytes(count);
            //_device.WaitForData();
            return data;
        }
        public int ReadByte()
        {
            if (_FtdiDevice == null || IsOpen() == false)
                return 0;

            byte[] data;
            data = _FtdiDevice.ReadBytes(1);
            return Convert.ToInt32(data[0]);
        }

        public void Open(string device, int BaudRate)
        {
            uint baud = (uint)BaudRate;
            _FtdiInfo = (FTDI_DeviceInfo)FTDI_DeviceInfo.EnumerateDevices().First(x => x.DeviceSerialNumber == device);
            if (_FtdiInfo == null)
                return;
            _FtdiDevice = new FTDI_Device(_FtdiInfo);
            _FtdiDevice.SetParameters(DataLength.EightBits, Parity.None, StopBits.OneStopBit);
            _FtdiDevice.SetBaudrate(baud);

            //this is to change the USB buffer before it passes data to applications. on high baudrates it allows for faster response times instead of getting data in dirty blocks
            _FtdiDevice.SetBufferSizes(64, 64);

            //set the timer for the event
            aTimer.Elapsed += timerTest;
            aTimer.AutoReset = true;
            aTimer.Enabled = true;

        }
        public void Close()
        {
            if (_FtdiDevice == null)
                return;
            _FtdiDevice.Close();
            aTimer.Enabled = false;
            aTimer.Elapsed -= timerTest;
        }

        public IList<SerialDevice> ReloadDevices()
        {
            var devList = new List<SerialDevice>();
            List<ManagementBaseObject> list = null;

            using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_PnPEntity WHERE Caption like '%(COM%'"))
            {
                list = searcher.Get().Cast<ManagementBaseObject>().ToList();
            }

            if (list == null || list.Count <= 0)
                return devList;

            foreach (var device in FTDI_DeviceInfo.EnumerateDevices())
            {
                var name = "Unknown Device";
                ManagementBaseObject ftdi = null;

                if (String.IsNullOrWhiteSpace(device.DeviceSerialNumber))
                {
                    var _tempList = list.Where(x => x["DeviceID"].ToString().Equals(device.DeviceSerialNumber)).ToList();
                    if (_tempList.Count > 0)
                        ftdi = null;
                    else
                        ftdi = list.First();
                }
                else
                    ftdi = list.Where(x => x["DeviceID"].ToString().Contains(device.DeviceSerialNumber)).SingleOrDefault();

                if (ftdi != null)
                {
                    name = (string)ftdi["Name"];
                }
                devList.Add(new SerialDevice(name, device.DeviceSerialNumber));
            }

            return devList;
        }
    }
}
