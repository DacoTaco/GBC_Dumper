using GB_Dumper.API;
using GB_Dumper.Serial;
using Microsoft.Win32;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace GB_Dumper
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
            PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
        }

        //variables
        //-------------
        APIHandler apiHandler = new APIHandler();

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
        //serial or FTDI mode?
        public bool FTDIMode
        {
            get => apiHandler.FTDIMode;
            set
            {
                apiHandler.FTDIMode = value;
                RefreshSerial();
                OnPropertyChanged("FTDIMode");
            }
        }

        //connected == busy -> false, connected == true && busy == false -> true , connected = false && busy == false -> false
        public bool EnableFunctions => Connected != apiHandler.IsApiBusy;
        public bool NotConnected => !Connected;
        public bool Connected => apiHandler.IsConnected;
        

        //functions
        //------------------
        public MainWindow()
        {
            InitializeComponent();
            Init();
        }

        //Init everything of the UI
        private void Init()
        {      
            DataContext = this;
            apiHandler.OnExceptionThrown += ApiHandler_OnExceptionThrown;
            apiHandler.OnWarningThrown += ApiHandler_OnWarningThrown;
            apiHandler.OnInfoThrown += ApiHandler_OnInfoThrown;
            apiHandler.OnStatusThrown += ApiHandler_OnStatusThrown;
            RefreshSerial();
            return;
        }

        //refill the UI controls that have the serial devices.
        public void RefreshSerial()
        {
            var list = apiHandler.SerialDevices.ToArray();

            if(list.Length <= 0)
            {
                cbComPorts.ItemsSource = null;
                cbComPorts.SelectedIndex = -1;

                cbBaudRate.ItemsSource = null;
                cbBaudRate.SelectedIndex = -1;
                return;
            }

            Array.Sort(list, (x, y) => x.Name.CompareTo(y.Name));
            Array.Sort(apiHandler.BaudRates);

            cbComPorts.ItemsSource = list;
            if (cbComPorts.Items.Count > 0)
                cbComPorts.SelectedIndex = 0;
            else
                cbComPorts.SelectedIndex = -1;

            cbBaudRate.ItemsSource = apiHandler.BaudRates;
            if (cbBaudRate.Items.Count > 0)
                cbBaudRate.SelectedItem = 500000;
            else
                cbBaudRate.SelectedIndex = -1;

        }






        //Event Handlers
        //-----------------
        private void ApiHandler_OnWarningThrown(string source, string Msg)
        {
            TextField += $"WARNING {(string.IsNullOrWhiteSpace(source) ? String.Empty : $"FROM {source}")} : {Environment.NewLine}\t{Msg}{Environment.NewLine}";
        }
        private void ApiHandler_OnExceptionThrown(string source, Exception e)
        {
            var msg = $"An Error Has Occured in {source}";
            TextField += $"{msg} : {Environment.NewLine}{e.ToString()}{Environment.NewLine}";
            while (e.InnerException != null)
            {
                TextField += $"Inner Exception : {e.InnerException.ToString()}{Environment.NewLine}";
                e = e.InnerException;
            }
            OnPropertyChanged("EnableFunctions");
            MessageBox.Show($"{msg}.{Environment.NewLine}Please Check the Debug Output for more info.", "Error");
        }
        private void ApiHandler_OnInfoThrown(string source, string Msg)
        {
            TextField += $"INFO {(string.IsNullOrWhiteSpace(source) ? String.Empty : $"FROM {source}")} : {Environment.NewLine}\t{Msg}{Environment.NewLine}";
        }
        private void ApiHandler_OnStatusThrown(byte status, APIMode mode, int param2)
        {
            switch(status)
            {
                case GB_API_Protocol.API_TASK_START:
                    switch(mode)
                    {
                        case APIMode.ReadRam:
                        case APIMode.ReadRom:
                            TextField += $"Downloading...{Environment.NewLine}0x{apiHandler.Info.current_addr.ToString("X8")}/0x{apiHandler.Info.FileSize.ToString("X8")}...";
                            break;
                        case APIMode.WriteRam:
                            TextField += $"Uploading...{Environment.NewLine}0x{apiHandler.Info.current_addr.ToString("X8")}/0x{apiHandler.Info.FileSize.ToString("X8")}...";
                            break;
                        default:
                            break;
                    }
                    break;
                case GB_API_Protocol.API_TASK_FINISHED: //we will update the UI & then finish it up
                case GB_API_Protocol.API_OK:
                    var selectText = string.Empty;
                    switch (mode)
                    {
                        case APIMode.ReadRam:
                        case APIMode.ReadRom:
                            selectText = "Downloading";
                            break;
                        case APIMode.WriteRam:
                            selectText = "Uploading";
                            break;
                        default:
                            break;
                    }

                    if (string.IsNullOrWhiteSpace(selectText))
                        break;

                    if (TextField.Contains(selectText))
                    {
                        string display;
                        int place = TextField.LastIndexOf(selectText);
                        if (place > 0)
                        {
                            place += $"{selectText}...{Environment.NewLine}".Length;
                            display = TextField.Remove(place, 24);
                            display += $"0x{apiHandler.Info.current_addr.ToString("X8")}/0x{apiHandler.Info.FileSize.ToString("X8")}...";
                            TextField = display;
                        }
                    }
                    else
                        TextField += $"{selectText}...{Environment.NewLine}0x{apiHandler.Info.current_addr.ToString("X8")}/0x{apiHandler.Info.FileSize.ToString("X8")}...";

                    if (status == GB_API_Protocol.API_TASK_FINISHED)
                    {
                        DateTime ending = DateTime.Now;
                        DateTime start = apiHandler.StartTime.Value;
                        TimeSpan diff = ending - start;
                        //we are done!
                        TextField += $"{Environment.NewLine}Done!{Environment.NewLine}Time : {diff.ToString()}{Environment.NewLine}";
                    }

                    break;
                case GB_API_Protocol.API_RESET:
                    OnPropertyChanged("EnableFunctions");
                    OnPropertyChanged("Connected");
                    OnPropertyChanged("NotConnected");
                    break;
                default:
                    break;
            }
        }


        private void TxtOutput_TextChanged(object sender, TextChangedEventArgs e)
        {
            txtOutput.ScrollToEnd();
        }
        private void RefreshSerial_Click(object sender, RoutedEventArgs e)
        {
            RefreshSerial();
        }
        private void BtnGetRam_Click(object sender, RoutedEventArgs e)
        {
            apiHandler.ReadRam();
            OnPropertyChanged("EnableFunctions");
        }
        private void BtnGetRom_Click(object sender, RoutedEventArgs e)
        {
            apiHandler.ReadRom();
            OnPropertyChanged("EnableFunctions");
        }
        private void BtnSendRam_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "gameboy save (*.sav)|*.sav|All files (*.*)|*.*",
                FilterIndex = 1,
                InitialDirectory = System.IO.Path.GetDirectoryName(Process.GetCurrentProcess().MainModule.FileName)
            };


            if (dialog.ShowDialog() == true)
            {
                apiHandler.WriteRam(dialog.FileName);
            }
            OnPropertyChanged("EnableFunctions");
        }
        private void Connect_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (apiHandler.IsConnected == true)
                {
                    apiHandler.Disconnect();
                    btConnect.IsChecked = false;
                }
                else
                {
                    TextField += $"Connecting to device... ";
                    if(apiHandler.Connect((SerialDevice)cbComPorts.SelectedItem, (int)cbBaudRate.SelectedItem))
                        TextField += $"Connected!{Environment.NewLine}";
                    else
                    {
                        TextField += $"Failed!{Environment.NewLine}";
                        btConnect.IsChecked = false;
                    }
                        
                }
            }
            catch (Exception ex)
            {
                TextField += $"Failed to connect to COM port : {Environment.NewLine}{ex.Message}";
                btConnect.IsChecked = false;
            }

            OnPropertyChanged("Connected");
            OnPropertyChanged("NotConnected");
            OnPropertyChanged("EnableFunctions");
        }
    }
}
