using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO.Ports;
using System.ComponentModel;
using System.IO;
using System.Diagnostics;
using Gameboy;
using SerialCommunication;
using Microsoft.Win32;

namespace GBC_Tool
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged(string propertyName)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
            }
        }

        
        FileStream fs = null;
        bool ReadRam = false;
        bool WriteRam = false;
        bool ReadRom = false;
        GB_API_Handler Handler = new GB_API_Handler();
        Serial serial = Serial.Instance;

        public bool EnableControls 
        {
            get
            {
                return !Connected;
            }
        }

        private bool connected;
        public bool Connected
        {
            get { return connected; }
            set 
            { 
                connected = value;
                OnPropertyChanged("Connected");
                OnPropertyChanged("EnableControls");
            }
        }
        

        private string textField;
        public string TextField
        {
            get
            {
                return textField;
            }
            set
            {
                textField = value;
                OnPropertyChanged("TextField");
            }

        }
        private void RefreshSerial()
        {
            
            if (serial.connection.IsOpen)
            {
                serial.connection.Close();
            }
            Connected = false;

            serial.ComPorts = SerialPort.GetPortNames();
            if (serial.ComPorts.Length > 0)
            {
                Array.Sort(serial.ComPorts);
                Array.Sort(serial.BaudRates);

                cbComPorts.ItemsSource = serial.ComPorts;
                cbComPorts.SelectedIndex = 0;
                cbBaudRate.ItemsSource = serial.BaudRates;
                cbBaudRate.SelectedItem = 9600;
            }
            else
            {
                Connected = true;
            }
        }
        private void Init()
        {
            this.DataContext = this;
            RefreshSerial();
            string filename = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName) + @"\output.txt";
            fs = System.IO.File.Create(filename);

            return;
        }
        private void ConnectPort()
        {
            serial.connection.PortName = cbComPorts.SelectedItem.ToString();
            serial.connection.BaudRate = Convert.ToInt32(cbBaudRate.SelectedItem);
            serial.connection.Handshake = System.IO.Ports.Handshake.None;
            serial.connection.Parity = Parity.None;
            serial.connection.DataBits = 8;
            serial.connection.StopBits = StopBits.One;
            //connection.DtrEnable = false;
            //connection.RtsEnable = false;
            serial.connection.ReadTimeout = 200;
            serial.connection.WriteTimeout = 50;
            serial.connection.ReceivedBytesThreshold = 1;
            Connected = true;

            serial.connection.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(HandleReceive);
            serial.connection.Open();

        }
        private void DisconnectPort()
        {
            ResetVariables();
            serial.connection.Close();
            Connected = false;
        }
        private void AddTextToField(string text)
        {
            if (String.IsNullOrWhiteSpace(text))
                return;

            byte[] data = Encoding.ASCII.GetBytes(text);
            fs.Write(data, 0, data.Length);
            TextField += text;
        }
        private void HandleReceive(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            // Collecting the characters received to our 'buffer' (string).
            int bufSize = serial.connection.BytesToRead;
            byte[] buf = new byte[bufSize];
            serial.connection.Read(buf, 0, bufSize);

            //Process the display
            //Process the reading of the rom/ram
            int ret = 0;
            if (ReadRom || ReadRam)
            {
                if(ReadRom)
                    ret = Handler.HandleRom(buf);
                else
                    ret = Handler.HandleRam(buf);

                if (ret < 0)
                {
                    byte[] data = new byte[bufSize - 2];
                    Array.Copy(buf, 2, data, 0, buf.Length - 2);
                    AddTextToField(String.Format("{1}{0}An Error was Given by the Controller!{1}", System.Text.Encoding.Default.GetString(data), Environment.NewLine));
                    ResetVariables();
                }
                else if (ret > 0)
                {
                    if (Handler.IsReading == false && (ReadRom == true || ReadRam == true))
                    {
                        //we are done reading!
                        AddTextToField((Environment.NewLine + "Done!" + Environment.NewLine));
                        ResetVariables();
                    }
                    else
                    {
                        if (TextField.Contains("Downloading"))
                        {
                            //replace the string in TextField!
                            //aka, update the read bytes
                            string display;
                            int place = TextField.LastIndexOf("Downloading...");
                            if (place > 0)
                            {
                                place += String.Format("Downloading...{0}", Environment.NewLine).Length;
                                display = TextField.Remove(place, 24);
                                display += String.Format("0x{0:X8}/0x{1:X8}...", Handler.Info.current_addr, Handler.Info.FileSize);
                                TextField = display;
                            }

                        }
                        else
                        {
                            AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, Handler.Info.current_addr, Handler.Info.FileSize));
                        }
                    }
                }
            }           
            else if(WriteRam)
            {
                ret = Handler.HandleWriteRam(buf);
                if (ret < 0)
                {
                    AddTextToField(buf + (Environment.NewLine + "An Error was Given by the Controller!" + Environment.NewLine));
                    ResetVariables();
                }
                else 
                {
                    if (ret == 5 )//&& Handler.IsWriting == false)
                    {
                        //error or we are done!
                        ResetVariables();
                        AddTextToField((Environment.NewLine + "Done!" + Environment.NewLine));
                    }
                    else
                    {
                        //still ongoing!
                        if (TextField.Contains("Uploading"))
                        {
                            //replace the string in TextField!
                            //aka, update the read bytes
                            string display;
                            int place = TextField.LastIndexOf("Uploading...");
                            if (place > 0)
                            {
                                place += String.Format("Uploading...{0}", Environment.NewLine).Length;
                                display = TextField.Remove(place, 24);
                                display += String.Format("0x{0:X8}/0x{1:X8}...", Handler.Info.current_addr, Handler.Info.FileSize);
                                TextField = display;
                            }

                        }
                        else
                        {
                            AddTextToField(String.Format("Uploading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, Handler.Info.current_addr, Handler.Info.FileSize));
                        }
                    }
                }
            }
            else
            {
                string data = Encoding.ASCII.GetString(buf, 0, bufSize);
                AddTextToField(data);
            }
        }
        private void ResetVariables()
        {
            Handler.ResetVariables();
            ReadRom = false;
            ReadRam = false;
            WriteRam = false;
            return;
        }
        private void GetRom()
        {
            if (serial.connection.IsOpen == true && ReadRom == false && WriteRam == false && ReadRam == false)
            {
                serial.connection.Write("API_READ_ROM\n");
                ReadRom = true;
                OpenFileDialog dialog = new OpenFileDialog();
                
                AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, Handler.Info.current_addr, Handler.Info.FileSize));
            }

        }
        private void GetRam()
        {
            if (serial.connection.IsOpen == true && ReadRom == false && WriteRam == false && ReadRam == false)
            {
                serial.connection.Write("API_READ_RAM\n");
                ReadRam = true;
                AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, Handler.Info.current_addr, Handler.Info.FileSize));
            }
        }
        private void SendRam()
        {
            if (serial.connection.IsOpen == true && ReadRom == false && WriteRam == false && ReadRam == false)
            {
                OpenFileDialog dialog = new OpenFileDialog();
                dialog.Filter = "gameboy save (*.sav/)|*.sav|All files (*.*)|*.*";
                dialog.FilterIndex = 1;
                dialog.InitialDirectory = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName);


                if (dialog.ShowDialog() == true)
                {
                    Handler.SetRamFilename(dialog.FileName);//Debug.WriteLine("we got a file! {0}{1}", dialog.FileName, Environment.NewLine);
                    serial.connection.Write("API_WRITE_RAM\n");
                    WriteRam = true;
                    AddTextToField(String.Format("Uploading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, Handler.Info.current_addr, Handler.Info.FileSize));
                }
                else
                {
                    Debug.WriteLine("nope");
                }
            }
        }
        public MainWindow()
        {
            InitializeComponent();
            Init();
        }

        private void Connect_Click(object sender, RoutedEventArgs e)
        {
            if (btConnect.IsChecked == true)
            {
                AddTextToField(String.Format("connecting... "));
                ConnectPort();
                AddTextToField(String.Format("Connected! {0}", Environment.NewLine));
            }
            else
            {
                DisconnectPort();
            }
        }

        private void txtOutput_TextChanged(object sender, TextChangedEventArgs e)
        {
            txtOutput.ScrollToEnd();
        }

        private void btnGetRom_Click(object sender, RoutedEventArgs e)
        {
            GetRom();
        }

        private void btnGetRam_Click(object sender, RoutedEventArgs e)
        {
            GetRam();
        }
        private void btnSendRam_Click(object sender, RoutedEventArgs e)
        {
            SendRam();
        }

        private void DetectSerial_Click(object sender, RoutedEventArgs e)
        {
            RefreshSerial();
        }

    }
}
