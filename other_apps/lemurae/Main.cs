// project created on 13/08/2004 at 13:26
using System;
using Gtk;
using Gdk;
using Gecko;
using Gnome;
using Glade;
//using GNU.Gettext;
//using GNU.Gettext;

public class DicRae: Gnome.Program
{

		[Widget] private Gtk.Entry Palabra;
		[Widget] private Gtk.Viewport Marco; 
		[Widget] private Gnome.App appw;  
		
		private Gecko.WebControl web;
		
        public static void Main (string[] args)
        {
               DicRae app= new DicRae (args);               
               app.Run();
        }

        public DicRae (string[] args) 
        : base ("LemuRae" , "0.1", Modules.UI, args)
        {
                LoadWidgets();                
        }

		private void LoadWidgets()
		{
				Glade.XML gxml = new Glade.XML(null,"dicrae.glade", "appw", null);
                gxml.Autoconnect (this);
                
                web=new WebControl();
                web.Show();      
                web.NetStop += on_net_stop;
                          
                Marco.Add(web);
                Pixbuf image = new Pixbuf("/usr/share/pixmaps/lemurae.svg");
                appw.Icon = image;                
                appw.ShowAll();
				    
		}
        /* Connect the Signals defined in Glade */
        public void on_net_stop(object o, EventArgs args)
        {          
          appw.Focus = Palabra;
        }
        public void on_appw_delete_event (object o, DeleteEventArgs args) 
        {
                Application.Quit ();
                args.RetVal = true;
        }
        
        private void on_bBuscar_clicked (object o, EventArgs args)
        {
        	web.LoadUrl("http://buscon.rae.es/draeI/SrvltGUIBusUsual?LEMA=" 
        			+ Palabra.Text.ToLower());
        	Palabra.SelectRegion(0,Palabra.Text.Length);
        	appw.Focus = Palabra;
        	
        }
        
        private void on_bAdelante_clicked (object o, EventArgs args)
        {
        	web.GoForward();
        }
        
        private void on_bAtras_clicked (object o, EventArgs args)
        {
        	web.GoBack();
        }
        
        private void on_bAyuda_clicked (object o, EventArgs args)
        {         
        	string[] authors = {"GoomerkO <goomerko@gmail.com>"};        	
        	Gnome.About ab = new Gnome.About ("LemuRae", "0.1","Gumer Coronel Prez (c) 2005",null,authors,null,null, new Pixbuf("/usr/share/pixmaps/lemurae.svg"));
        	ab.Run();         
        }
        
        
}

