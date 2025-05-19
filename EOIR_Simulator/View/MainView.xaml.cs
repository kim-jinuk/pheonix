using System;
using System.IO;
using System.Windows;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Media;

using EOIR_Simulator.Model;
using EOIR_Simulator.Service;
using EOIR_Simulator.ViewModel;
using System.Collections.Generic;
using System.Windows.Controls;
using System.Windows.Threading;
using System.ComponentModel;


namespace EOIR_Simulator.View
{
    public partial class MainView : Window
    {
        private readonly MainViewModel _vm;

        public MainView()
        {
            InitializeComponent();

            if (!DesignerProperties.GetIsInDesignMode(this))
            {
                _vm = (MainViewModel)DataContext;   // XAML에서 생성된 VM 참조
            }
        }  
    }
}
