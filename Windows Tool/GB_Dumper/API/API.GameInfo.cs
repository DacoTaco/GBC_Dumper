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
}
