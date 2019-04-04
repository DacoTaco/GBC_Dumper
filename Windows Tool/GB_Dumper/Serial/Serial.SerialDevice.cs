
namespace GB_Dumper.Serial
{
    public class SerialDevice
    {
        public string Device { get; private set; }
        public string Name { get; private set; }

        public SerialDevice(string name, string device)
        {
            Device = device;
            Name = name;
        }

        public override string ToString()
        {
            return Name;
        }
    }
}