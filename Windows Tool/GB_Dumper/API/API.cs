using GB_Dumper.FileHelper;
using GB_Dumper.Serial;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace GB_Dumper.API
{
    public enum APIMode
    {
        Open,
        ReadRom,
        ReadRam,
        WriteRam
    }

    public partial class APIHandler
    {
        //error/exception/warning/info events!
        //-----------------------------------------
        public Action<string, Exception> OnExceptionThrown;
        public Action<string, string> OnWarningThrown;
        public Action<string, string> OnInfoThrown;
        public Action<ApiInfo> OnStatusThrown;

        private void _throwException(Exception e) => OnExceptionThrown(e.Source, e);
        private void _throwWarning(object source,string msg) => OnWarningThrown(source?.GetType().ToString(),msg);
        private void _throwInfo(object source, string msg) => OnInfoThrown(source?.GetType().ToString(), msg);
        private void _throwStatus(byte status) => _throwStatus(status, null);
        private void _throwStatus(byte status,string Msg)
        {
            ApiInfo msg = new ApiInfo
            {
                Status = status,
                Msg = Msg,
                gameInfo = Info,
                API_Mode = API_Mode,
                StartTime = StartTime
            };
            OnStatusThrown(msg);
        }


        //variables
        //---------------------
        private FileHandler fileHandler = new FileHandler();
        private APIMode API_Mode;
        public bool AutoDetect { get; private set; }
        private GameInfo Info = new GameInfo();
        private DateTime? StartTime;

        private SerialInterface serialInterface = SerialInterface.Instance;
        public bool FTDIMode
        {
            get
            {
                return serialInterface.FTDIMode;
            }
            set
            {
                serialInterface.FTDIMode = value;
            }
        }
        public IList<SerialDevice> SerialDevices => serialInterface.Devices;
        public int[] BaudRates => serialInterface.BaudRates;
        public bool IsConnected => serialInterface.IsOpen;
        public bool IsApiBusy => API_Mode != APIMode.Open;

        //functions
        //--------------------------------
        public bool Connect(SerialDevice device,int BaudRate)
        {
            try
            {
                if (IsApiBusy)
                    throw new InvalidOperationException("Failed to connect : API not ready.");

                serialInterface.OpenDevice(device, BaudRate);

                switch(API_AttemptAPIHandshake())
                {
                    case -1:
                    default:
                        serialInterface.Close();
                        return false;
                    case 0:
                        _throwWarning(this, $"{Environment.NewLine}Controller did not respond correctly to the handshake. Keeping connection open...{Environment.NewLine}");
                        goto case 1;
                    case 1:
                        //handshake was successful
                        serialInterface.OnDataToRead += Serial_DataToRead;
                        serialInterface.OnErrorRaised += Serial_ErrorRaised;
                        break;
                }
            }
            catch(Exception e)
            {
                _throwException(e);
                return false;
            }
            return true;
        }

        private void Serial_ErrorRaised(string ErrorType, string message)
        {
            _throwWarning(this, message);
        }

        public void Disconnect()
        {
            try
            {
                /*if (IsApiBusy)
                    throw new InvalidOperationException("Failed to disconnect : API not ready.");*/

                serialInterface.OnDataToRead -= Serial_DataToRead;
                serialInterface.OnErrorRaised -= Serial_ErrorRaised;
                serialInterface.Close();
                API_ResetVariables();
            }
            catch (Exception e)
            {
                _throwException(e);
            }
        }
        public void ReadRom()
        {
            try
            {
                if (!IsConnected)
                    throw new InvalidOperationException("Failed to read rom : Serial is not connected");
                if (IsApiBusy)
                    throw new InvalidOperationException("Failed to read rom : API is not ready");

                API_Mode = APIMode.ReadRom;
                //send command!
                serialInterface.Write($"{GB_API_Protocol.API_READ_ROM}\n");
            }
            catch (Exception e)
            {
                _throwException(e);
                return ;
            }  
        }
        public void ReadRam()
        {
            try
            {
                if (!IsConnected)
                    throw new InvalidOperationException("Failed to read ram : Serial is not connected");
                if (IsApiBusy)
                    throw new InvalidOperationException("Failed to read ram : API is not ready");

                API_Mode = APIMode.ReadRam;
                //send command!
                serialInterface.Write($"{GB_API_Protocol.API_READ_RAM}\n");
            }
            catch (Exception e)
            {
                _throwException(e);
                return;
            }
        }
        public void WriteRam(string filename)
        {
            try
            {
                if (!IsConnected)
                    throw new InvalidOperationException("Failed to write ram : Serial is not connected.");
                if (IsApiBusy)
                    throw new InvalidOperationException("Failed to write ram : API is not ready.");

                if (!File.Exists(filename))
                    throw new FileNotFoundException($"Failed to write ram : {filename} does not exist.");


                fileHandler.OpenFile(filename, FileMode.Open);
                API_Mode = APIMode.WriteRam;
                //send command!
                serialInterface.Write($"{GB_API_Protocol.API_WRITE_RAM}\n");
            }
            catch (Exception e)
            {
                _throwException(e);
                return;
            }
        }

        private void Serial_DataToRead(object source, SerialEventArgs e)
        {
            try
            {
                if (serialInterface.BytesToRead <= 0)
                    return;

                var data = serialInterface.Read(serialInterface.BytesToRead);

                switch(API_Mode)
                {
                    case APIMode.WriteRam:
                        API_HandleWriteRam(data);
                        break;
                    case APIMode.ReadRam:
                    case APIMode.ReadRom:
                        if (!API_HandleReadRomRam(data))
                            API_ResetVariables();
                        break;
                    case APIMode.Open:
                    default:
                        _throwInfo(this, Encoding.ASCII.GetString(data, 0, data.Length));
                        break;
                }
                return;
            }
            catch (Exception ex)
            {
                _throwException(ex);
                API_ResetVariables();
                try
                {
                    serialInterface.Write(new byte[] { GB_API_Protocol.API_ABORT_CMD }, 0, 1);
                }
                catch(Exception exc)
                {
                    _throwException(exc);
                }
                return;
            }
        }
    }
}
