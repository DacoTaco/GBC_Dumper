﻿using SerialCommunication;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gameboy
{
    /// <summary>
    /// All API related bytes and values
    /// </summary>
    public static class GB_API_Protocol
    {
        //header functions
        public const byte API_GBC_SUPPORT_START = 0x76;
        public const byte API_GBC_ONLY = 0x77;
        public const byte API_GBC_HYBRID = 0x78;
        public const byte API_GBC_GB = 0x79;
        public const byte API_GBC_SUPPORT_END = 0x7A;
        public const byte API_GAMENAME_START = 0x86;
        public const byte API_GAMENAME_END = 0x87;
        public const byte API_FILESIZE_START = 0x96;
        public const byte API_FILESIZE_END = 0x97;

        //command functions
        public const byte API_OK = 0x10;
        public const byte API_NOK = 0x11;
        public const byte API_VERIFY = 0x12;
        public const byte API_RESEND_CMD = 0x13;
        public const byte API_HANDSHAKE_REQUEST = 0x14;
        public const byte API_HANDSHAKE_ACCEPT = 0x15;
        public const byte API_HANDSHAKE_DENY = 0x16;

        public const byte API_TASK_START = 0x20;
        public const byte API_TASK_FINISHED = 0x21;

        public const byte API_MODE_READ_ROM = 0xD0;
        public const byte API_MODE_READ_RAM = 0xD1;
        public const byte API_MODE_WRITE_RAM = 0xD2;

        public const byte API_ABORT = 0xF0;
        public const byte API_ABORT_ERROR = 0xF1;
        public const byte API_ABORT_CMD = 0xF2;
        public const byte API_ABORT_PACKET = 0xF3;

        public const byte TYPE_ROM = 0;
        public const byte TYPE_RAM = 1;
    }

    public class GB_API_Handler
    {
        /// <summary>
        /// structure used to hold all current game info
        /// </summary>
        public struct GameInfo
        {
            public string CartName;
            public Int32 FileSize;
            public Int32 current_addr;
            public bool? IsGBC;
        }

        //all variables used by the API handler
        public GameInfo Info = new GameInfo();
        private FileStream Filefs = null;
        Serial serial = Serial.Instance;
        private byte PrevByte = 0x00;
        private byte[] PrevPacket = {0x00,0x00};
        private byte APIMode = 0x00;
        private string RamFilepath = String.Empty;
        private bool IsWriting = false;      


        public GB_API_Handler()
        {
            ResetVariables();
        }

        //reset all variables and settings
        public void ResetVariables()
        {
            if (Filefs != null)
            {
                Filefs.Close();
                Filefs = null;
            }
            APIMode = 0x00;
            Info.CartName = string.Empty;
            Info.FileSize = 0;
            Info.current_addr = 0;
            Info.IsGBC = null;
            IsWriting = false;
            RamFilepath = string.Empty;
            return;
        }

        //send data over serial connection
        private void SendData(byte[] data)
        {
            if (data == null || data.Length <= 0)
                return;

            PrevPacket = data;
            serial.Write(data, 0, data.Length);
        }

        //functions about opening file for ROM/RAM reading or RAM writing
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
            {
                if (Info.IsGBC == true)
                    filename += ".gbc";
                else
                    filename += ".gb";
            }
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
            //Open File with the selected mode, and allow others to still read the file!
            Filefs = new FileStream(Filename, mode, FileAccess.ReadWrite, FileShare.Read);
        }

        private int ProcessHeaderPacket(byte[] buf, bool IsRom, bool IsWriting)
        {
            if (IsRom && IsWriting)
                return -1;

            //we sometimes get uncomplete data, so wait a bit to see if more data is coming.
            System.Threading.Thread.Sleep(10);
            if (serial.BytesToRead > 0)
            {
                //we have data!
                int lenght = serial.BytesToRead;
                byte[] data = new byte[lenght];
                serial.Read(data,0,lenght);
                buf = buf.Concat(data).ToArray();
            }

            int bufSize = buf.Length > 0x25?0x25:buf.Length;
            int offset = 0;
            for (int i = 0; i < bufSize; i++)
            {
                if (Info.IsGBC == null && i + 2 <= bufSize && buf[i] == GB_API_Protocol.API_GBC_SUPPORT_START && buf[i + 2] == GB_API_Protocol.API_GBC_SUPPORT_END)
                {
                    //retrieve data if the game is a GB,GBC hybrid or GBC only game
                    switch (buf[i + 1])
                    {
                        default:
                        case GB_API_Protocol.API_GBC_GB:
                            Info.IsGBC = false;
                            break;
                        case GB_API_Protocol.API_GBC_HYBRID:
                        case GB_API_Protocol.API_GBC_ONLY:
                            Info.IsGBC = true;
                            break;
                    }
                    i += 2;
                    offset = i;
                }
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
                        i += 3;
                        offset = i;
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

                        i += (2 + strSize);
                        offset = i;                       
                    }
                }
                else if (i + 2 <= bufSize && buf[i] == GB_API_Protocol.API_ABORT)
                {
                    if (buf[i + 1] == GB_API_Protocol.API_ABORT_ERROR || buf[i + 1] == GB_API_Protocol.API_ABORT_CMD)
                    {
                        //there was an error. see field for more info. meanwhile, close all commands
                        ResetVariables();
                        return -3;
                    }
                }
                else if (i == bufSize - 1 && buf[i] == GB_API_Protocol.API_OK && (Info.FileSize == 0))//( String.IsNullOrWhiteSpace(Info.CartName) || Info.FileSize == 0 || Info.IsGBC == null))
                {
                    //we got the OK from the information but we are lacking some information. bail out! send ABORT!
                    byte[] data = new byte[] { GB_API_Protocol.API_ABORT, GB_API_Protocol.API_ABORT_CMD };
                    serial.Write(data, 0, data.Length);
                    ResetVariables();
                    return -4;
                }
            }
            if (offset > 0)
                offset++;

            return offset;
        }
        private int ProcessHeaderPacket(byte[] buf, bool IsRom)
        {
            return ProcessHeaderPacket(buf, IsRom,false);
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

        public byte GetAPIMode()
        {
            return APIMode;
        }
        public void SetAPIMode(byte mode)
        {
            switch (mode)
            {
                case GB_API_Protocol.API_MODE_READ_ROM:
                    APIMode = mode;
                    serial.Write("API_READ_ROM\n");
                    break;
                case GB_API_Protocol.API_MODE_READ_RAM:
                    APIMode = mode;
                    serial.Write("API_READ_RAM\n");
                    break;
                case GB_API_Protocol.API_MODE_WRITE_RAM:
                    APIMode = mode;
                    serial.Write("API_WRITE_RAM\n");
                    break;
                default:
                    APIMode = 0x00;
                    break;
            }
        }
        public int AttemptAPIHandshake()
        {
            int ret = 0;

            serial.Write(new byte[] {GB_API_Protocol.API_HANDSHAKE_REQUEST},0,1);
            byte cnt = 0;

            while (true)
            {
                if (cnt >= 10)
                {
                    ret = 0;
                    break;
                }

                if (serial.BytesToRead > 0)
                {
                    int data = serial.ReadByte();
                    if (data == GB_API_Protocol.API_HANDSHAKE_ACCEPT)
                    {
                        ret = 1;
                    }
                    else if (data == GB_API_Protocol.API_HANDSHAKE_DENY)
                    {
                        ret = -1;
                    }
                    else
                    {
                        ret = 0;
                    }
                    break;
                }
                else
                {
                    System.Threading.Thread.Sleep(20);
                    cnt++;
                }
            }    
            return ret;
        }

        public int HandleData(byte[] buf)
        {
            if (buf == null || buf.Length <= 0)
                return 0;

            int ret = 0;

            switch (APIMode)
            {
                case GB_API_Protocol.API_MODE_WRITE_RAM:
                    ret = HandleWriteRam(buf);
                    break;
                case GB_API_Protocol.API_MODE_READ_ROM:
                    ret = HandleReadRomRam(buf,true);
                    break;
                case GB_API_Protocol.API_MODE_READ_RAM:
                    ret = HandleReadRomRam(buf, false);
                    break;
                default:
                    ret = -15;
                    break;
            }
            return ret;
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
                offset = ProcessHeaderPacket(buf, false, true);
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
                        ret = GB_API_Protocol.API_OK;
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
                    System.Threading.Thread.Sleep(10);
                    if (serial.BytesToRead == 1)
                    {
                        //we have data!
                        byte new_data = (byte)(serial.ReadByte() & 0xFF);
                        buf = new byte[] { buf[0], new_data };
                        bufSize = buf.Length;
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
                            ret = GB_API_Protocol.API_OK;
                        }
                    }
                    else
                    {
                        data = new byte[] { GB_API_Protocol.API_NOK, PrevByte };
                        ret = GB_API_Protocol.API_NOK;
                    }
                }
                else if (bufSize == 1 && buf[0] == GB_API_Protocol.API_RESEND_CMD)
                {
                    data = new byte[] { PrevPacket[0], PrevPacket[1] };
                    ret = GB_API_Protocol.API_RESEND_CMD;
                }
                else if (bufSize == 1 && buf[0] == GB_API_Protocol.API_TASK_START && Info.current_addr == 0)
                {
                    Filefs.Position = 0;
                    int readByte = Filefs.ReadByte();
                    byte_to_send = Convert.ToByte(readByte);
                    PrevByte = byte_to_send;
                    Info.current_addr++;
                    data = new byte[] { GB_API_Protocol.API_OK, byte_to_send };
                    ret = GB_API_Protocol.API_TASK_START;
                }
                else if ((bufSize == 1 && buf[0] == GB_API_Protocol.API_TASK_FINISHED) || (bufSize == 3 && buf[2] == GB_API_Protocol.API_TASK_FINISHED))
                {
                    //oh, we are done!
                    ret = GB_API_Protocol.API_TASK_FINISHED;
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
        private int HandleReadRomRam(byte[] buf, bool isRom)
        {
            int offset = 0;
            int bufSize = buf.Length;
            int ret = 0;
            if (Info.FileSize <= 0 || String.IsNullOrWhiteSpace(Info.CartName))
            {
                offset = ProcessHeaderPacket(buf, isRom);
                if (offset < 0 || (Info.FileSize <= 0 || String.IsNullOrWhiteSpace(Info.CartName)))
                {
                    byte[] data = new byte[] { GB_API_Protocol.API_NOK };
                    serial.Write(data, 0, data.Length);
                    return -11;
                }
                else if (offset > 0)
                {
                    //send ok to say we are ready!
                    byte[] data = new byte[] { GB_API_Protocol.API_OK };
                    serial.Write(data, 0, data.Length);
                    //since last byte is the OK from the controller, we need to see if there is more data beyond it. so +2 (byte 00 and the last OK after the offset)
                    if (offset + 1 >= bufSize)
                        return 0;
                    else
                        offset++;
                }
            }
            if (Info.FileSize > 0 && String.IsNullOrWhiteSpace(Info.CartName) == false)
            {
                if (Filefs == null)
                {
                    OpenFile(isRom);
                    if (Filefs == null)
                    {
                        ret = -10;
                    }
                }

                if(Filefs != null)
                {
                    //check if we retrieved all data
                    if ((Info.current_addr + (bufSize - offset)) < Info.FileSize)
                    {
                        Filefs.Write(buf, offset, bufSize - offset);
                        Info.current_addr += bufSize - offset;
                        //ret = Info.current_addr;
                        ret = GB_API_Protocol.API_OK;
                    }
                    else
                    {
                        //end of file
                        Filefs.Write(buf, offset, Info.FileSize - Info.current_addr);
                        Info.current_addr = Info.FileSize;
                        //ret = Info.current_addr;
                        ret = GB_API_Protocol.API_TASK_FINISHED;
                    }
                }
            }
            return ret;
        }

    }
}
