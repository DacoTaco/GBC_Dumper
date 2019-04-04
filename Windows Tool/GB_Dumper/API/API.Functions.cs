using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace GB_Dumper.API
{
    public partial class APIHandler
    {
        private void API_ResetVariables()
        {
            Info.CartName = String.Empty;
            Info.current_addr = 0;
            Info.FileSize = 0;
            Info.CartType = 0;
            fileHandler.CloseFile();
            API_Mode = APIMode.Open;
            _throwStatus(GB_API_Protocol.API_RESET);
            StartTime = null;
        }
        private int API_AttemptAPIHandshake()
        {
            serialInterface.Write(new byte[] { GB_API_Protocol.API_HANDSHAKE_REQUEST }, 0, 1);
            byte cnt = 0;

            while (cnt <= 10)
            {
                if (serialInterface.BytesToRead > 0)
                {
                    int data = serialInterface.ReadByte();
                    if (data == GB_API_Protocol.API_HANDSHAKE_ACCEPT)
                    {
                        return 1;
                    }
                    else if (data == GB_API_Protocol.API_HANDSHAKE_DENY)
                    {
                        return -1;
                    }
                    else
                    {
                        return 0;
                    }
                }
                else
                {
                    System.Threading.Thread.Sleep(20);
                    cnt++;
                }
            }

            //time out
            return 0;
        }
        private bool API_HandleReadRomRam(byte[] data)
        {
            //process header
            if(!fileHandler.IsOpened || string.IsNullOrWhiteSpace(Info.CartName))
            {
                //this is the header we are processing.
                if (!API_ProcessHeader(data))
                {
                    _throwStatus(GB_API_Protocol.API_ABORT_CMD);
                    API_ResetVariables();              
                    return false;
                }


                var filename = $"{Info.CartName}";
                if (API_Mode == APIMode.ReadRom)
                {
                    switch (Info.CartType)
                    {
                        case GB_CART_TYPE.API_GBA_ONLY:
                            filename += ".gba";
                            break;
                        case GB_CART_TYPE.API_GB_ONLY:
                            filename += ".gb";
                            break;
                        case GB_CART_TYPE.API_GBC_HYBRID:
                        case GB_CART_TYPE.API_GBC_ONLY:
                            filename += ".gbc";
                            break;
                    }
                }
                else
                    filename += ".sav";

                fileHandler.OpenFile(filename, FileMode.Create);
                _throwStatus(GB_API_Protocol.API_TASK_START);
                //send ok, we are ready for data
                serialInterface.Write(new byte[] { GB_API_Protocol.API_OK },0,1);
                return true;
            }

            //data
            if (StartTime == null)
                StartTime = DateTime.Now;

            //check if we retrieved all data
            if (Info.current_addr + data.Length < Info.FileSize)
            {
                fileHandler.Write(data);
                Info.current_addr += data.Length;
                _throwStatus(GB_API_Protocol.API_OK);
            }
            else
            {
                //end of file
                byte[] dataToWrite = new byte[Info.FileSize - Info.current_addr];
                Array.Copy(data, dataToWrite, Info.FileSize - Info.current_addr);
                fileHandler.Write(dataToWrite);
                Info.current_addr = Info.FileSize;
                _throwStatus(GB_API_Protocol.API_TASK_FINISHED);
                API_ResetVariables();
            }
            return true;
        }
        private bool API_HandleWriteRam(byte[] data)
        {
            try
            {
                //process header if needed
                if (Info.FileSize == 0)
                {
                    if (!API_ProcessHeader(data))
                    {
                        _throwStatus(GB_API_Protocol.API_ABORT_CMD);
                        API_ResetVariables();                       
                        return false;
                    }

                    if (fileHandler.FileSize != Info.FileSize)
                        throw new InvalidDataException($"Incorrect selected save size ({fileHandler.FileSize}). The Game's save is {Info.FileSize}");


                    _throwStatus(GB_API_Protocol.API_TASK_START);
                    //send ok, we are ready for data
                    serialInterface.Write(new byte[] { GB_API_Protocol.API_OK }, 0, 1);
                    return true;
                }

                if (data.Length < 2)
                {
                    //somehow we only got 1 byte. wait to see if we get more data for a while.
                    //if we dont, let the code handle it. it could be legit (API_TASK_START)
                    System.Threading.Thread.Sleep(10);
                    if (serialInterface.BytesToRead > 0)
                    {
                        //we have data!
                        var buf = new List<byte>(data);
                        foreach (var _byte in serialInterface.Read(serialInterface.BytesToRead))
                        {
                            buf.Add(_byte);
                        }
                        data = buf.ToArray();
                    }

                }

                //invalid data 
                if (data.Length > 2 || (data.Length == 1 && (data[0] != GB_API_Protocol.API_TASK_START && data[0] != GB_API_Protocol.API_TASK_FINISHED )))
                    throw new InvalidDataException($"Unexpected data retrieved from controller : 0x{data[0].ToString("X2")}({data.Length})");

                if (StartTime == null)
                    StartTime = DateTime.Now;

                //the handshake is done and the controller has send an OK!
                //this function will look as following : 

                //look at what is in the first byte we received. 
                //  -> API_TASK_START -> send OK + Byte
                //  -> API_VERIFY -> verify the last byte. if this is ok send OK + next byte. if not ok, send NOK + last byte again
                //in case we get into problems -> Send ABORT (exception catch)

                //proccess data we got from controller and send data
                switch (data[0])
                {
                    case GB_API_Protocol.API_TASK_START:
                        if (Info.current_addr != 0)
                            throw new InvalidDataException("Received API_TASK_START at an unexpected moment.");
                        serialInterface.Write(new byte[] { GB_API_Protocol.API_OK, fileHandler[0] },0,2);
                        break;
                    case GB_API_Protocol.API_VERIFY:
                        //if the data isn't correct, resend the byte
                        if(data[1] != fileHandler[Info.current_addr])
                        {
                            serialInterface.Write(new byte[] { GB_API_Protocol.API_NOK, fileHandler[Info.current_addr] }, 0, 2);
                            _throwStatus(GB_API_Protocol.API_OK);
                            break;
                        }

                        if (Info.current_addr > Info.FileSize)
                            throw new InvalidOperationException($"Current position in ram is greater then ram size");

                        _throwStatus(GB_API_Protocol.API_OK);
                        Info.current_addr++;
                        serialInterface.Write(new byte[] { GB_API_Protocol.API_OK, fileHandler[Info.current_addr] }, 0, 2);
                        
                        break;
                    case GB_API_Protocol.API_TASK_FINISHED:
                        _throwStatus(GB_API_Protocol.API_TASK_FINISHED);
                        API_ResetVariables();
                        break;
                    default:
                        throw new InvalidDataException($"Unexpected data retrieved from controller : {data[0].ToString("X2")}({data.Length})");
                }

                return true;
            }
            catch (Exception e)
            {
                _throwStatus(GB_API_Protocol.API_ABORT_CMD);
                API_ResetVariables();
                serialInterface.Write(new byte[] { GB_API_Protocol.API_ABORT }, 0, 1);
                throw e;
            }
        }
        private bool API_ProcessHeader(byte[] data)
        {
            if (data == null)
                throw new ArgumentException("Failed to process header : Invalid arguments");

            short sleep_cnt = 0;
            //we sometimes get uncomplete data, so wait a bit to see if more data is coming.
            while (serialInterface.BytesToRead == 0)
            {
                System.Threading.Thread.Sleep(1);
                sleep_cnt++;
                if (sleep_cnt > 500)
                    break;
            }
            if (serialInterface.BytesToRead > 0)
            {
                //we have data!
                data = serialInterface.Read(serialInterface.BytesToRead);
                data = data.Concat(serialInterface.Read(serialInterface.BytesToRead)).ToArray();
            }

            //did we get send an error?
            if (data[0] >= GB_API_Protocol.API_ABORT && data[1] >= GB_API_Protocol.API_ABORT)
            {
                //we retrieved an error. throw the error msg as info and stop
                _throwWarning(this, $"Cart Error : {Encoding.ASCII.GetString(data, 2, data.Length - 4)}{Environment.NewLine}");
                return false;
            }

            if (
                (API_Mode != APIMode.WriteRam && data.Length < 0xC) ||
                (API_Mode == APIMode.WriteRam && data.Length < 0x7)
                )
                throw new TimeoutException("Time out : system didn't respond with the correct amout of data");

            //time to process the actual header data :)
            for (int i = 0; i < data.Length; i++)
            {
                if(data[i] == GB_API_Protocol.API_GB_CART_TYPE_START && data[i + 2] == GB_API_Protocol.API_GB_CART_TYPE_END)
                {
                    //retrieve data if the game is a GB,GBC hybrid,GBC only or a GBA game
                    switch (data[i + 1])
                    {
                        case GB_CART_TYPE.API_GBC_ONLY:
                        case GB_CART_TYPE.API_GB_ONLY:
                        case GB_CART_TYPE.API_GBA_ONLY:
                        case GB_CART_TYPE.API_GBC_HYBRID:
                            Info.CartType = data[i + 1];
                            break;

                        default:
                            throw new ArgumentException($"Error parsing header (cart type) : unknown cart type {data[i+1].ToString()} received");
                    }
                    i += 3;
                }
                if (data[i] == GB_API_Protocol.API_GAMENAME_START && data[i + 2] == GB_API_Protocol.API_GAMENAME_END)
                {
                    //we need to retrieve the name. this is a packet between 3 - 20 bytes. 0x86 [name size] 0x87 [name]
                    int strSize = data[i + 1];
                    if (i + 3 + strSize > data.Length || strSize == 0)
                        throw new ArgumentException("Error parsing header (Game Name) : ERROR_INVALID_PARAM");


                    Info.CartName = Encoding.ASCII.GetString(data, i + 3, strSize);

                    //sadly this is possible. some unofficial games *cough wizardtree cough* have empty names...
                    if(string.IsNullOrWhiteSpace(Info.CartName))
                    {
                        Info.CartName = $"GAME_{DateTime.Now.ToString("ddMMyy")}";
                    }

                    i += (3 + strSize);
                }
                if (data[i] == GB_API_Protocol.API_FILESIZE_START && data[i + 5] == GB_API_Protocol.API_FILESIZE_END)
                {
                    //retrieve rom size which is in a 8 byte packet : header a b c d header_end
                    Info.FileSize = (data[i + 1] << 24) + (data[i + 2] << 16) + (data[i + 3] << 8) + data[i + 4];

                    //MBC2 is 0x200(minimum) and 0x8000 max(32KB)
                    if (Info.FileSize == 0 || 
                        (Info.CartType != GB_CART_TYPE.API_GBA_ONLY && (Info.FileSize < 0x0200 || Info.FileSize > 0x400000))
                        ) //we have an invalid valid rom or ram
                            throw new ArgumentException("Error parsing header (file size) : ERROR_INVALID_PARAM");

                    //skip to the bytes we need
                    i += 6;
                }
            }
            return true;
        }
    }
}
