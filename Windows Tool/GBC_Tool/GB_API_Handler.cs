using SerialCommunication;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gameboy
{
    public static class GB_API_Protocol
    {
        public static byte API_GAMENAME_START = 0x86;
        public static byte API_GAMENAME_END = 0x87;
        public static byte API_FILESIZE_START = 0x96;
        public static byte API_FILESIZE_END = 0x97;

        public static byte API_OK = 0x10;
        public static byte API_NOK = 0x11;
        public static byte API_VERIFY = 0x12;
        public static byte API_RESEND_CMD = 0x13;

        public static byte API_TASK_START = 0x20;
        public static byte API_TASK_FINISHED = 0x21;

        public static byte API_ABORT = 0xF0;
        public static byte API_ABORT_ERROR = 0xF1;
        public static byte API_ABORT_CMD = 0xF2;
        public static byte API_ABORT_PACKET = 0xF3;

        public static byte TYPE_ROM = 0;
        public static byte TYPE_RAM = 1;
    }
    public class GB_API_Handler
    {
        public struct GameInfo
	    {
		    public string CartName;
            public Int32 FileSize;
            public Int32 current_addr;
	    }

        public GameInfo Info = new GameInfo();
        private FileStream Filefs = null;
        Serial serial = Serial.Instance;
        private byte PrevByte = 0x00;
        private byte[] PrevPacket = {0x00,0x00};

        private string RamFilepath = String.Empty;

        private bool isWriting = false;
        public bool IsWriting
        {
            get { return isWriting; }
            private set { isWriting = value; }
        }

        private bool isReading = false;
        public bool IsReading
        {
            get 
            {
                return isReading; 
            }
            private set
            {
                isReading = value;
            }
        }
        

        public GB_API_Handler()
        {
            ResetVariables();
        }
        public void ResetVariables()
        {
            if (Filefs != null)
            {
                Filefs.Close();
                Filefs = null;
            }
            Info.CartName = string.Empty;
            Info.FileSize = 0;
            Info.current_addr = 0;
            IsWriting = false;
            IsReading = false;
            RamFilepath = string.Empty;
            return;
        }
        private void SendData(byte command)
        {
            byte[] data = { command };
            SendData(data);
        }
        private void SendData(byte[] data)
        {
            if (data == null || data.Length <= 0)
                return;

            PrevPacket = data;
            serial.connection.Write(data, 0, data.Length);
        }
        private void OpenSaveFile()
        {
            string filename = string.Empty;
            if (String.IsNullOrWhiteSpace(RamFilepath))
            {
                filename = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName) + @"\save.sav";
            }
            else
                filename = RamFilepath;

            OpenFile(filename, true);
        }
        private void OpenFile(bool OpenRom)
        {
            if (string.IsNullOrWhiteSpace(Info.CartName))
                return;

            string filename = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName) + @"\" + Info.CartName;
            if (OpenRom)
                filename += ".gb";
            else
                filename += ".sav";

            OpenFile(filename, false);
        }
        private void OpenFile(string Filename,bool OpenOnly)
        {
            if (string.IsNullOrWhiteSpace(Filename))
                return;    

            FileMode mode = FileMode.Create;
            if (OpenOnly)
                mode = FileMode.Open;
            Filefs = System.IO.File.Open(Filename, mode);
        }
        private int GetHeaderInfo(byte[] buf, bool IsRom, bool IsWriting)
        {
            if (IsRom && IsWriting)
                return -1;

            int bufSize = buf.Length;
            int offset = 0;
            for (int i = 0; i < bufSize; i++)
            {
                if (Info.FileSize <= 0 && i + 3 <= bufSize && buf[i] == GB_API_Protocol.API_FILESIZE_START && buf[i + 3] == GB_API_Protocol.API_FILESIZE_END)
                {
                    //retrieve rom size which is in a 6 byte packet. 0x66 0xRombank1 0xRombank2 0x67
                    Info.FileSize = (buf[i + 1] << 8) + (buf[i + 2] & 0xFF);

                    if (IsRom)
                    {
                        //when reading rom we are getting the bank amount instead. so lets calculate ourselfs
                        Info.FileSize = Info.FileSize * 0x4000;
                    }

                    if (Info.FileSize < 0x0800 || Info.FileSize > 0x400000) //we have an invalid valid rom or ram
                    {
                        ResetVariables();
                        return -1;
                    }
                    else
                    {
                        //skip to the bytes we need
                        offset += 4;
                    }
                }
                else if (String.IsNullOrWhiteSpace(Info.CartName) && i + 2 <= bufSize && buf[i] == GB_API_Protocol.API_GAMENAME_START && buf[i + 2] == GB_API_Protocol.API_GAMENAME_END)
                {
                    //we need to retrieve the name. this is a packet between 3 - 20 bytes. 0x86 [name size] 0x87 [name]
                    int strSize = buf[i + 1];
                    if (i + strSize > bufSize)
                    {
                        ResetVariables();
                        return -2;
                    }
                    else
                    {
                        int cnt = (strSize + i + 3 >= bufSize) ? (bufSize - i - 3) : strSize;
                        Info.CartName = Encoding.ASCII.GetString(buf, i + 3, cnt);
                        offset += (3 + strSize);
                    }
                }
                else if (i + 2 <= bufSize && buf[i] == GB_API_Protocol.API_ABORT)
                {
                    if (buf[i + 1] == GB_API_Protocol.API_ABORT_ERROR || buf[i + 1] == GB_API_Protocol.API_ABORT_CMD)
                    {
                        //TextField += (Environment.NewLine + "An Error was Given by the Controller!" + Environment.NewLine);
                        //there was an error. see field for more info. meanwhile, close all commands
                        ResetVariables();
                        return -3;
                    }
                }
            }
            return offset;
        }
        private int GetHeaderInfo(byte[] buf, bool IsRom)
        {
            return GetHeaderInfo(buf, IsRom,false);
        }
        public void SetRamFilename(string filepath)
        {
            if (String.IsNullOrWhiteSpace(filepath))
            {
                RamFilepath = string.Empty;
            }
            else
                RamFilepath = filepath;
        }
        public int HandleWriteRam(byte[] buf)
        {
            if (buf == null || buf.Length <= 0)
                return 0;

            int offset = 0;
            int bufSize = buf.Length;
            int ret = 1;
            byte[] data = null;
            if (IsWriting == false && Info.current_addr == 0)
            {
                offset = GetHeaderInfo(buf, false, true);
                if (offset < 0)
                    return offset;
                else if (offset > 0)
                {
                    //we got the header, time to open file and do the handshake
                    if (Filefs != null)
                    {
                        Filefs.Close();
                        Filefs = null;
                    }
                    OpenSaveFile();


                    if (buf[offset] == GB_API_Protocol.API_OK && Filefs != null)
                    {
                        Filefs.Position = 0;
                        data = new byte[] { GB_API_Protocol.API_OK };
                        ret = 1;
                        IsWriting = true;
                    }
                    else
                    {
                        data = new byte[] { GB_API_Protocol.API_NOK };
                        ResetVariables();
                        ret = -2;
                    }
                    SendData(data);
                }
            }
            else if (IsWriting == true)
            {
                //the handshake is done and the controller has send a OK!
                //this function will look as following : 

                //read byte from file
                //check if we are WaitingForData. if we are, and the packet is 2 bytes and its an VERIFY: 
                //      -> is it the 2nd byte the byte we send?
                //          -> if it is, send OK + new byte and increase the cur_addr & read new byte
                //          -> if it isn't, send NOK + byte again
                //if we are WaitingForData but packet isn't right -> ABBANDON SHIP!
                //check if we are not WaitingForData, send OK + byte & set writing

                if (bufSize == 1)
                {
                    //somehow we only got 1 byte. wait to see if we get more data for a while.
                    //if we dont, let the code handle it. it could be legit
                    System.Threading.Thread.Sleep(5);
                    if (serial.connection.BytesToRead == 1)
                    {
                        //we have data!
                        byte new_data = (byte)(serial.connection.ReadByte() & 0xFF);
                        buf = new byte[] { buf[0], new_data };
                    }

                }


                byte byte_to_send;

                if (Info.current_addr > 0 && bufSize == 2 && buf[0] == GB_API_Protocol.API_VERIFY)
                {
                    if (buf[1] == PrevByte)
                    {
                        byte_to_send = PrevByte;
                        if (Info.current_addr < Info.FileSize)
                        {
                            Info.current_addr++;
                            Filefs.Position = Info.current_addr - 1;
                            int readByte = Filefs.ReadByte();
                            byte_to_send = Convert.ToByte(readByte & 0xFF);
                            PrevByte = byte_to_send;
                        }
                        else if (Info.current_addr > Info.FileSize + 1)
                        {
                            //something went wrong! ABORT!
                            data = new byte[] { GB_API_Protocol.API_ABORT, GB_API_Protocol.API_ABORT_CMD };
                            ResetVariables();
                            ret = -6;
                        }

                        if (data == null)
                        {
                            data = new byte[] { GB_API_Protocol.API_OK, byte_to_send };
                            ret = 3;
                        }
                    }
                    else
                    {
                        data = new byte[] { GB_API_Protocol.API_NOK, PrevByte };
                        ret = 4;
                    }
                }
                else if (bufSize == 1 && buf[0] == GB_API_Protocol.API_RESEND_CMD)
                {
                    data = new byte[] { PrevPacket[0], PrevPacket[1] };
                    ret = 6;
                }
                else if (bufSize == 1 && buf[0] == GB_API_Protocol.API_TASK_START && Info.current_addr == 0)
                {
                    Filefs.Position = 0;
                    int readByte = Filefs.ReadByte();
                    byte_to_send = Convert.ToByte(readByte);
                    PrevByte = byte_to_send;
                    Info.current_addr++;
                    data = new byte[] { GB_API_Protocol.API_OK, byte_to_send };
                    ret = 2;
                }
                else if ((bufSize == 1 && buf[0] == GB_API_Protocol.API_TASK_FINISHED) || (bufSize == 3 && buf[2] == GB_API_Protocol.API_TASK_FINISHED))
                {
                    //oh, we are done!
                    ResetVariables();
                    ret = 5;
                }
                else
                {
                    //we got some BS data. ABBANDON SHIP
                    data = new byte[] { GB_API_Protocol.API_ABORT, GB_API_Protocol.API_ABORT_PACKET };
                    ResetVariables();
                    ret = -3;
                }

                if (data != null)
                    SendData(data);            
            }
            return ret;
        }
        public int HandleRom(byte[] buf)
        {
            return HandleReadRomRam(buf,true);
        }
        public int HandleRam(byte[] buf)
        {
            return HandleReadRomRam(buf,false);
        }
        private int HandleReadRomRam(byte[] buf, bool isRom)
        {
            int offset = 0;
            int bufSize = buf.Length;
            int ret = 0;
            if (Info.FileSize <= 0 || String.IsNullOrWhiteSpace(Info.CartName))
            {
                offset = GetHeaderInfo(buf, isRom);
                if (offset < 0)
                    return offset;
            }
            if (Info.FileSize > 0 && String.IsNullOrWhiteSpace(Info.CartName) == false)
            {
                if (Filefs == null)
                {
                    OpenFile(isRom);
                    IsReading = true;
                    if (Filefs == null)
                    {
                        ret = -10;
                    }
                }
                else
                {
                    //check if we retrieved all data
                    if ((Info.current_addr + (bufSize - offset)) < Info.FileSize)
                    {
                        Filefs.Write(buf, offset, bufSize - offset);
                        Info.current_addr += bufSize - offset;
                        ret = Info.current_addr;
                    }
                    else
                    {
                        //end of file
                        Filefs.Write(buf, offset, Info.FileSize - Info.current_addr);
                        Info.current_addr = Info.FileSize;
                        ret = Info.current_addr;
                        ResetVariables();
                    }
                }
            }
            return ret;
        }

    }
}
