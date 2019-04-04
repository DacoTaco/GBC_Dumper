
namespace GB_Dumper.API
{
    /// <summary>
    /// All API related bytes and values. these SHOULD ALWAYS BE INSYNC WITH THE CONTROLLER'S VALUES
    /// </summary>
    public static class GB_API_Protocol
    {
        //header functions
        public const byte API_GB_CART_TYPE_START = 0x76;
        public const byte API_GB_CART_TYPE_END = 0x77;

        public const byte API_GAMENAME_START = 0x86;
        public const byte API_GAMENAME_END = 0x87;
        public const byte API_FILESIZE_START = 0x96;
        public const byte API_FILESIZE_END = 0x97;

        //commands
        public const string API_READ_ROM = "API_READ_ROM";
        public const string API_READ_RAM = "API_READ_RAM";
        public const string API_WRITE_RAM = "API_WRITE_RAM";

        //command functions
        public const byte API_OK = 0x10;
        public const byte API_NOK = 0x11;
        public const byte API_VERIFY = 0x12;
        public const byte API_RESET = 0x13;
        public const byte API_HANDSHAKE_REQUEST = 0x17;
        public const byte API_HANDSHAKE_ACCEPT = 0x06;
        public const byte API_HANDSHAKE_DENY = 0x15;

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
    public static class GB_CART_TYPE
    {
        public const byte API_GBC_ONLY = 0x78;
        public const byte API_GBC_HYBRID = 0x79;
        public const byte API_GB_ONLY = 0x7A;
        public const byte API_GBA_ONLY = 0x7B;
    }
}
