using System;

namespace GB_Dumper.API
{
    /// <summary>
    /// structure used to hold all current game info
    /// </summary>
    public struct GameInfo
    {
        public string CartName;
        public Int32 FileSize;
        public Int32 current_addr;
        public Int32 CartType;
    }

    public class ApiInfo
    {
        public GameInfo gameInfo = new GameInfo();
        public string Msg;
        public APIMode API_Mode;
        public byte Status;
        public DateTime? StartTime;
    }
}
