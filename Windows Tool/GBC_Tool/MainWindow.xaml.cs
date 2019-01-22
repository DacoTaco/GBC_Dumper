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
using System.Timers;


namespace GBC_Tool
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        //Property changed event raiser
        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged(string propertyName)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
            }
        }

        
        //variables used by the interface
        static FileStream Debugfs = null;
        GB_API_Handler APIHandler = new GB_API_Handler();
        Serial serial = Serial.Instance;

        //variable to control the enabled property of the controls in the UI
        public bool EnableControls 
        {
            get
            {
                return !Connected;
            }
        }

        //serial or FTDI mode?
        private bool ftdiMode = true;
        public bool FTDIMode
        {
            get
            {
                return ftdiMode;
            }
            set
            {
                ftdiMode = value;
                RefreshSerial();
            }
        }

        //if we are busy or not
        private bool notBusy;
        public bool NotBusy
        {
            get 
            {
                if (Connected == false)
                    return false;
                return notBusy; 
            }
            set 
            { 
                notBusy = value;
                OnPropertyChanged("NotBusy");
                OnPropertyChanged("EnableControls");
            }
        }
        

        //see EnabledControls
        private bool connected;
        public bool Connected
        {
            get { return connected; }
            set 
            { 
                connected = value;
                OnPropertyChanged("Connected");
                OnPropertyChanged("NotBusy");
                OnPropertyChanged("EnableControls");
            }
        }
        

        //variable containing all text of the textfield
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

        //redetect all serial ports
        private void RefreshSerial()
        {   
            Connected = false;

            serial.ReloadDevices(FTDIMode);

            if (serial.Devices.Count > 0)
            {
                var list = serial.Devices.ToArray();
                Array.Sort(list,(x,y) => x.Name.CompareTo(y.Name));
                Array.Sort(serial.BaudRates);

                cbComPorts.ItemsSource = list;
                if (cbComPorts.Items.Count > 0)
                    cbComPorts.SelectedIndex = 0;
                else
                    cbComPorts.SelectedIndex = -1;

                cbBaudRate.ItemsSource = serial.BaudRates;
                if (cbBaudRate.Items.Count > 0)
                    cbBaudRate.SelectedItem = 250000;
                else
                    cbBaudRate.SelectedIndex = -1;
            }
            else
            {
                cbComPorts.ItemsSource = null;
                cbComPorts.SelectedIndex = -1;

                cbBaudRate.ItemsSource = null;
                cbBaudRate.SelectedIndex = -1;

                Connected = true;
            }
        }

        //Init everything of the UI
        private void Init()
        {
            ResetVariables();
            this.DataContext = this;
            RefreshSerial();
            string filename = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName) + @"\output.txt";
            Debugfs = new FileStream(filename, FileMode.Create, FileAccess.ReadWrite, FileShare.Read);//System.IO.File.Create(filename);

            return;
        }
        private void ConnectPort()
        {
            try
            {
                //connect to FTDI chip! 
                serial.Open((SerialDevice)cbComPorts.SelectedItem,Convert.ToInt32(cbBaudRate.SelectedItem));

                int ret = APIHandler.AttemptAPIHandshake();
                if (ret >= 0)
                {
                    //handshake was succesful
                    serial.OnDataToRead += new DataReadHandler(HandleReceive);
                    Connected = true;
                    if (ret == 0)
                    {
                        AddTextToField("Controller did not give a correct response, still keeping connection open..." + Environment.NewLine);
                    }
                }
                else
                {
                    //fail
                    serial.Close();
                }
            }
            catch (Exception ex)
            {

                throw ex;
            }   
        }
        //disconnect port
        private void DisconnectPort()
        {
            ResetVariables();
            serial.Close();
            serial.OnDataToRead -= new DataReadHandler(HandleReceive);
            Connected = false;
        }

        //add a string to the textfield and write it to file
        private void AddTextToField(string text)
        {
            if (String.IsNullOrWhiteSpace(text))
                return;

            byte[] data = Encoding.ASCII.GetBytes(text);
            Debugfs.Write(data, 0, data.Length);
            Debugfs.Flush();
            TextField += text;
        }

        //event handler for the receiving of data!
        private void HandleReceive(object sender, SerialEventArgs e)
        {
            // Collecting the characters received to our 'buffer' (string).
            int bufSize = serial.BytesToRead;

            byte[] buf = new byte[bufSize];
            buf = serial.Read(bufSize);
            int ret = 0;

            //Process the reading of the rom/ram
            byte API_Mode = APIHandler.GetAPIMode();
            if (API_Mode > 0)
            {
                ret = APIHandler.HandleData(buf);
                if (ret < 0)
                {
                    int index = Array.IndexOf(buf, GB_API_Protocol.API_ABORT);
                    if (index+2 > bufSize)
                    {
                        index = 0;
                    }
                    else
                    {
                        index += 2;
                    }
                    byte[] data = new byte[bufSize - index];
                    Array.Copy(buf, index, data, 0, buf.Length - index);
                    AddTextToField(Environment.NewLine + "An Error (" + ret + ") was Given by the Controller : " + Encoding.ASCII.GetString(data) + Environment.NewLine);
                    ResetVariables();
                }
                else if( ret > 0)
                {
                    //we are reading/downloading data
                    if(API_Mode == GB_API_Protocol.API_MODE_READ_RAM || API_Mode == GB_API_Protocol.API_MODE_READ_ROM)
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
                                display += String.Format("0x{0:X8}/0x{1:X8}...", APIHandler.Info.current_addr, APIHandler.Info.FileSize);
                                TextField = display;
                            }

                        }
                        else
                        {
                            AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, APIHandler.Info.current_addr, APIHandler.Info.FileSize));
                        }
                    }
                    //we are writing/uploading data!
                    else if(API_Mode == GB_API_Protocol.API_MODE_WRITE_RAM)
                    {
                        //still ongoing!
                        //update the text!
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
                                display += String.Format("0x{0:X8}/0x{1:X8}...", APIHandler.Info.current_addr, APIHandler.Info.FileSize);
                                TextField = display;
                            }

                        }
                        else
                        {
                            //didn't find the text for some odd reason. so lets add it!
                            AddTextToField(String.Format("Uploading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, APIHandler.Info.current_addr, APIHandler.Info.FileSize));
                        }
                    }
                    else //unknown WTF we are doing, so lets just output data
                    {
                        string data = Encoding.ASCII.GetString(buf, 0, bufSize);
                        AddTextToField(data);
                    }

                    //are we done?
                    if (ret == GB_API_Protocol.API_TASK_FINISHED)
                    {
                        //we are done reading!
                        AddTextToField((Environment.NewLine + "Done!" + Environment.NewLine));
                        ResetVariables();
                    }
                }
            }
            //default : display data
            else
            {
                string data = Encoding.ASCII.GetString(buf, 0, bufSize);
                AddTextToField(data);
            }
        }

        //reset all variables and handlers
        private void ResetVariables()
        {
            APIHandler.ResetVariables();
            NotBusy = true;
            return;
        }


        //function called by the buttons
        private void GetRom()
        {
            if (serial.IsOpen == true && APIHandler.GetAPIMode() == 0)
            {
                NotBusy = false;
                APIHandler.SetAPIMode(GB_API_Protocol.API_MODE_READ_ROM);             
                AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, APIHandler.Info.current_addr, APIHandler.Info.FileSize));
                serial.OnDataToRead += HandleReceive;
            }

        }

        //function called by the buttons
        private void GetRam()
        {
            if (serial.IsOpen == true && APIHandler.GetAPIMode() == 0)
            {
                NotBusy = false;
                APIHandler.SetAPIMode(GB_API_Protocol.API_MODE_READ_RAM);
                AddTextToField(String.Format("Downloading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, APIHandler.Info.current_addr, APIHandler.Info.FileSize));
            }
        }
        //function called by the buttons
        private void SendRam()
        {
            if (serial.IsOpen == true && APIHandler.GetAPIMode() == 0)
            {
                OpenFileDialog dialog = new OpenFileDialog();
                dialog.Filter = "gameboy save (*.sav/)|*.sav|All files (*.*)|*.*";
                dialog.FilterIndex = 1;
                dialog.InitialDirectory = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName);


                if (dialog.ShowDialog() == true)
                {
                    NotBusy = false;
                    APIHandler.SetRamFilename(dialog.FileName);//Debug.WriteLine("we got a file! {0}{1}", dialog.FileName, Environment.NewLine);
                    APIHandler.SetAPIMode(GB_API_Protocol.API_MODE_WRITE_RAM);
                    AddTextToField(String.Format("Uploading{0}{1}{2}{3}0x{4:X8}/0x{5:X8}...", ".", ".", ".", Environment.NewLine, APIHandler.Info.current_addr, APIHandler.Info.FileSize));
                }
            }
        }

        //class init
        public MainWindow()
        {
            InitializeComponent();
            Init();
        }

        //event handler for connecting
        private void Connect_Click(object sender, RoutedEventArgs e)
        {
            try
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
            catch (Exception ex)
            {
                AddTextToField(String.Format("Failed to connect to COM port : {0}{1}", Environment.NewLine, ex.Message));
                btConnect.IsChecked = false;
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
