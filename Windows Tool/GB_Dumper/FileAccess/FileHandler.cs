using System;
using System.IO;

namespace GB_Dumper.FileAccess
{
    public class FileHandler
    {
        private FileStream _file;
        public bool IsOpened => _file != null;
        public void CloseFile()
        {
            if (!IsOpened)
                return;

            _file.Close();
            _file = null;
        }
        public bool OpenFile(string filename,FileMode mode)
        {
            try
            {
                if (string.IsNullOrWhiteSpace(filename) || string.IsNullOrWhiteSpace(Path.GetFileNameWithoutExtension(filename)))
                    throw new ArgumentNullException("Filename");

                if (IsOpened)
                    CloseFile();

                _file = File.Open(filename, mode);
                _file.Position = 0;
            }
            catch (Exception ex)
            {
                throw ex;
            }
            return true;
        }
        public byte this[int index]
        {
            get
            {
                if(Read(out var array,index,1) > 0)
                    return array[0];
                throw new IOException($"Failed to read {_file.Name}(@{index}).");                
            }
            set => throw new NotImplementedException();
        }

        public int FileSize => Convert.ToInt32(_file.Length);

        public int Write(object value)
        {
            if (!IsOpened)
                throw new InvalidOperationException("Failed to write data : File Not Open");

            if (!(value is byte[]))
                throw new ArgumentException("Failed to write data : can only write byte[]");

            var data = value as byte[];
            _file.Write(data, 0, data.Length);
            _file.Flush();

            return FileSize;
        }

        public int Read(out byte[] buf, int index,int count)
        {
            buf = new byte[count];
            if (index >= FileSize)
                index = FileSize - 1;
            _file.Position = index;
            return _file.Read(buf, 0, count);
        }
    }
}
