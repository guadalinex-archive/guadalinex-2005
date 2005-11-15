// project created on 13/08/2004 at 13:26
using System;
using Gtk;
using Gdk;
using Gecko;
using Glade;

//using GNU.Gettext;
//using GNU.Gettext;

public class DicRae
{

		[Widget] private Gtk.Entry Palabra;
		[Widget] private Gtk.VBox Marco; 
		[Widget] private Gtk.Window appw;  
		[Widget] private Gtk.Button bAtras;
		[Widget] private Gtk.Button bAdelante;
		[Widget] private Gtk.Statusbar statusbar;
		
		private Gecko.WebControl web;
		
        public static void Main (string[] args)
        {
        	Application.Init();
            DicRae dr = new DicRae (args);               
            Application.Run();
        }

        public DicRae (string[] args)        
        {
                LoadWidgets();                
        }

		private void LoadWidgets()
		{
				Glade.XML gxml = new Glade.XML(null,"dicrae.glade", "appw", null);
                gxml.Autoconnect (this);
                
                bAtras.Sensitive = false;
                bAdelante.Sensitive = false;
                
                web=new WebControl();
                web.Show();      
                web.NetStop += on_net_stop;
                web.NetStart += on_net_start;
                statusbar.Push(0, "Listo.");
                                          
                Marco.Add(web);
                Pixbuf image = new Pixbuf("/usr/share/pixmaps/lemurae.svg");
                appw.Icon = image;                
                appw.ShowAll();
				    
		}
        /* Connect the Signals defined in Glade */
        public void on_net_start(object o, EventArgs args)
        {
        	statusbar.Push(0, "Buscando ...");
        }
        public void on_net_stop(object o, EventArgs args)
        {          
          	statusbar.Pop(0);          
          	bAtras.Sensitive = web.CanGoBack();
          	bAdelante.Sensitive = web.CanGoForward();          
          	appw.Focus = Palabra;
        }       
       
        public void on_appw_delete_event (object o, DeleteEventArgs args) 
        {
            Application.Quit();
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

