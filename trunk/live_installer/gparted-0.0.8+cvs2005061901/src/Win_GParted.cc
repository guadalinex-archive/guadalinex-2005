/* Copyright (C) 2004 Bart
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include "../include/Win_GParted.h"
#include <gtk/gtk.h>

namespace GParted
{
	
Win_GParted::Win_GParted( guint installer )
{

	installer_mode = installer;

	copied_partition .Reset( ) ;
	selected_partition .Reset( ) ;
	new_count = 1;
	current_device = 0 ;
	vbox_visual_disk = NULL;
	pulse = false ;
	
	// Plug hacks for embedding gparted into ubuntu-express
	if ( installer_mode > 0)
	{
		GtkWidget * plug;
		plug = gtk_plug_new(installer_mode);
		gtk_container_add(GTK_CONTAINER(plug), GTK_WIDGET(vbox_main.gobj()));
		gtk_widget_show(GTK_WIDGET(vbox_main.gobj()));
		gtk_widget_show(plug);
		this ->set_position(Gtk::WIN_POS_NONE);
		this ->set_default_size( 0, 0 );
		this ->move(-10, 0);
		this ->set_keep_above();
		this ->set_resizable(false);
		init_partition_menu( );
	}
	else
	{
		//==== GUI =========================
		this ->set_title( _("GParted") );
		this ->set_default_size( 775, 500 );
	
		//Pack the main box
		this ->add( vbox_main ); 
	
		//menubar....
		init_menubar( ) ;
		vbox_main .pack_start( menubar_main, Gtk::PACK_SHRINK );
	}
	
	//toolbar....
	init_toolbar( ) ;
	vbox_main.pack_start( hbox_toolbar, Gtk::PACK_SHRINK );
	
	//hbox_visual...  ( contains the visual represenation of the disks )
	vbox_main.pack_start( hbox_visual, Gtk::PACK_SHRINK );
		
	//hpaned_main (NOTE: added to vpaned_main)
	init_hpaned_main( ) ;
	vpaned_main .pack1( hpaned_main, true, true ) ;
	
	//vpaned_main....
	vbox_main .pack_start( vpaned_main );
	
	//device info...
	init_device_info( ) ;
	
	//operationslist...
	init_operationslist( ) ;
	vpaned_main .pack2( hbox_operations, true, true ) ;
	
	//statusbar... 
	pulsebar = manage( new Gtk::ProgressBar( ) );
	pulsebar ->set_pulse_step( 0.01 );
	statusbar .add( *pulsebar );
	vbox_main .pack_start( statusbar, Gtk::PACK_SHRINK );
	
	if ( installer_mode > 0 )
	    vbox_main .show_all_children( );
	else
	    this ->show_all_children( );
	
	//make sure harddisk information and operationlist are closed..
	hpaned_main .get_child1( ) ->hide( ) ;
	close_operationslist( ) ;

	conn = dispatcher .connect( sigc::mem_fun( *this, &Win_GParted::menu_gparted_refresh_devices ) );
	dispatcher( ) ;

}

void Win_GParted::init_menubar( )
{
	//fill menubar_main and connect callbacks 
	//gparted
	menu = manage( new Gtk::Menu( ) ) ;
	image = manage( new Gtk::Image( Gtk::Stock::REFRESH, Gtk::ICON_SIZE_MENU ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::ImageMenuElem( _("_Refresh devices"), Gtk::AccelKey("<control>r"), *image, sigc::mem_fun(*this, &Win_GParted::menu_gparted_refresh_devices) ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("Filesystems"), sigc::mem_fun( *this, &Win_GParted::menu_gparted_filesystems ) ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::QUIT, sigc::mem_fun(*this, &Win_GParted::menu_gparted_quit) ) );
	menubar_main .items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("_GParted"), *menu ) );
	
	//view
	menu = manage( new Gtk::Menu( ) ) ;
	menu ->items( ) .push_back( Gtk::Menu_Helpers::CheckMenuElem( _("Harddisk Information"), sigc::mem_fun(*this, &Win_GParted::menu_view_harddisk_info) ) );
	menu ->items( ) .push_back( Gtk::Menu_Helpers::CheckMenuElem( _("Operations"), sigc::mem_fun(*this, &Win_GParted::menu_view_operations) ) );
	menubar_main .items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("_View"), *menu ) );
	
	//device
	menu = manage( new Gtk::Menu( ) ) ;
	menu ->items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("Set Disklabel"), sigc::mem_fun(*this, &Win_GParted::activate_disklabel) ) );
	menubar_main .items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("_Device"), *menu ) );
		
	//partition
	init_partition_menu( ) ;
	menubar_main .items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("_Partition"), menu_partition ) );
	
	//help
	menu = manage( new Gtk::Menu() ) ;
	menu ->items( ) .push_back(Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::HELP, sigc::mem_fun(*this, &Win_GParted::menu_help_contents) ) );
	image = manage( new Gtk::Image( "/usr/share/icons/hicolor/16x16/stock/generic/stock_about.png" ) );
	menu ->items( ) .push_back(Gtk::Menu_Helpers::ImageMenuElem( _("About"), *image, sigc::mem_fun(*this, &Win_GParted::menu_help_about) ) );
	menubar_main.items( ) .push_back( Gtk::Menu_Helpers::MenuElem(_("_Help"), *menu ) );
}

void Win_GParted::init_toolbar( ) 
{
	//initialize and pack toolbar_main 
	hbox_toolbar.pack_start( toolbar_main );
	
	//NEW and DELETE
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::NEW));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_new) );	toolbar_main.append(*toolbutton);
	toolbutton ->set_tooltip(tooltips, _("Create a new partition in the selected unallocated space") );		
	toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::DELETE));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_delete) );	toolbar_main.append(*toolbutton);
	toolbutton ->set_tooltip(tooltips, _("Delete the selected partition") );		
	toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	
	//RESIZE/MOVE
	image = manage( new Gtk::Image( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_BUTTON ) );
	toolbutton = Gtk::manage(new Gtk::ToolButton( *image, _("Resize/Move") ));
	toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_resize) ); 	toolbar_main.append(*toolbutton);
	toolbutton ->set_tooltip(tooltips, _("Resize/Move the selected partition") );		
	toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	
	if ( installer_mode == 0 )
	{
		//COPY and PASTE
		toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::COPY));
		toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_copy) );
		toolbar_main.append(*toolbutton);
		toolbutton ->set_tooltip(tooltips, _("Copy the selected partition to the clipboard") );

		toolbutton = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::PASTE));
		toolbutton ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_paste) );
		toolbar_main.append(*toolbutton);
		toolbutton ->set_tooltip(tooltips, _("Paste the partition from the clipboard") );

		toolbar_main.append( *(Gtk::manage(new Gtk::SeparatorToolItem)) );
	}
	
	//UNDO and APPLY
	toolbutton_undo = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::UNDO));
	toolbutton_undo ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_undo) );	toolbar_main.append(*toolbutton_undo); toolbutton_undo ->set_sensitive( false );
	toolbutton_undo ->set_tooltip(tooltips, _("Undo last operation") );		

	/** if ( installer_mode > 0 )
	{
		image = manage( new Gtk::Image( Gtk::Stock::SAVE, Gtk::ICON_SIZE_LARGE_TOOLBAR ) );
		toolbutton_apply = Gtk::manage(new Gtk::ToolButton( *image, _("Save and Exit") ));
	}
	else **/
		toolbutton_apply = Gtk::manage(new Gtk::ToolButton(Gtk::Stock::SAVE));

	toolbutton_apply ->signal_clicked().connect( sigc::mem_fun(*this, &Win_GParted::activate_apply) );	toolbar_main.append(*toolbutton_apply); toolbutton_apply ->set_sensitive( false );
	toolbutton_apply ->set_tooltip(tooltips, _("Apply all operations") );		
	
	//initizialize and pack optionmenu_devices
	optionmenu_devices .set_menu( * manage( new Gtk::Menu( ) ) );
	optionmenu_devices .signal_changed( ) .connect( sigc::mem_fun(*this, &Win_GParted::optionmenu_devices_changed) );
	hbox_toolbar .pack_start( optionmenu_devices, Gtk::PACK_SHRINK );
}

void Win_GParted::init_partition_menu( ) 
{
	//fill menu_partition
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::NEW, sigc::mem_fun(*this, &Win_GParted::activate_new) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::DELETE, Gtk::AccelKey( GDK_Delete, Gdk::BUTTON1_MASK ), sigc::mem_fun(*this, &Win_GParted::activate_delete) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	image = manage( new Gtk::Image( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_MENU ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::ImageMenuElem( _("Resize/Move"), *image, sigc::mem_fun(*this, &Win_GParted::activate_resize) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::COPY, sigc::mem_fun(*this, &Win_GParted::activate_copy) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::PASTE, sigc::mem_fun(*this, &Win_GParted::activate_paste) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	image = manage( new Gtk::Image( Gtk::Stock::CONVERT, Gtk::ICON_SIZE_MENU ) );
	/*TO TRANSLATORS: menuitem which holds a submenu with filesystems.. */
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::ImageMenuElem( _("_Convert to"), *image, menu_convert ) ) ;
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::MenuElem( _("Unmount"), sigc::mem_fun( *this, &Win_GParted::activate_unmount ) ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::SeparatorElem( ) );
	menu_partition .items( ) .push_back( Gtk::Menu_Helpers::StockMenuElem( Gtk::Stock::DIALOG_INFO, sigc::mem_fun(*this, &Win_GParted::activate_info) ) );
	init_convert_menu( ) ;
	
	menu_partition .accelerate( *this ) ;  
}

void Win_GParted::init_convert_menu()
{
	for ( unsigned int t=0; t < gparted_core .get_filesystems( ) .size( ) -1 ; t++ )
	{
		color .set( Get_Color( gparted_core .get_filesystems( )[ t ] .filesystem ) );
		hbox = manage( new Gtk::HBox( ) );
			
		//the colored square
		entry =  manage ( new Gtk::Entry( ) );
		entry ->set_sensitive( false );
		entry ->set_size_request( 12, 12 );
		entry ->modify_base( entry ->get_state( ), color );
		hbox ->pack_start( *entry, Gtk::PACK_SHRINK );
			
		//the label...
		hbox ->pack_start( * mk_label( " " + gparted_core .get_filesystems( )[ t ] .filesystem ), Gtk::PACK_SHRINK );	
				
		menu_item = manage( new Gtk::MenuItem( *hbox ) ) ;
		menu_convert.items( ) .push_back( *menu_item );
		menu_convert.items( ) .back( ) .signal_activate( ) .connect( sigc::bind<Glib::ustring>(sigc::mem_fun(*this, &Win_GParted::activate_convert), gparted_core .get_filesystems( )[ t ] .filesystem ) ) ;
	}
	
	menu_convert.show_all_children() ;
}

void Win_GParted::init_device_info()
{
	vbox_info.set_spacing( 5 );
	int top =0, bottom = 1;
	
	//title
	vbox_info .pack_start( * mk_label( " <b>" + (Glib::ustring) _( "Harddisk Information" ) + ":</b>" ), Gtk::PACK_SHRINK );
	
	//GENERAL DEVICE INFO
	table = manage( new Gtk::Table() ) ;
	table ->set_col_spacings( 10 ) ;
	
	//model
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Model:" ) + "</b>" ) , 0,1,top, bottom ,Gtk::FILL);
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back(), 1,2, top++, bottom++, Gtk::FILL);
	
	//size
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Size:" ) + "</b>" ) , 0,1,top, bottom ,Gtk::FILL);
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back(), 1,2, top++, bottom++, Gtk::FILL);
	
	//path
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Path:" ) + "</b>" ) , 0,1,top, bottom ,Gtk::FILL);
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back(), 1,2, top++, bottom++, Gtk::FILL);
	
	//real path
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Real Path:" ) + "</b>" ) , 0,1,top, bottom ,Gtk::FILL);
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back(), 1,2, top++, bottom++, Gtk::FILL);
		
	vbox_info .pack_start( *table, Gtk::PACK_SHRINK );
	
	//DETAILED DEVICE INFO 
	top = 0; bottom = 1;
	table = manage( new Gtk::Table( ) ) ;
	table ->set_col_spacings( 10 ) ;
	
	//one blank line
	table ->attach( * mk_label( "" ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//disktype
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "DiskType:" ) + "</b>" ), 0, 1, top, bottom, Gtk::FILL );
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back( ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//heads
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Heads:" ) + "</b>" ), 0, 1, top, bottom, Gtk::FILL );
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back( ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//sectors/track
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Sectors/Track:" ) + "</b>" ) , 0, 1, top, bottom, Gtk::FILL );
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back( ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//cylinders
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Cylinders:" ) + "</b>" ), 0, 1, top, bottom, Gtk::FILL );
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back( ), 1, 2, top++, bottom++, Gtk::FILL );
	
	//total sectors
	table ->attach( * mk_label( " <b>" + (Glib::ustring) _( "Total Sectors:" ) + "</b>" ), 0, 1, top, bottom, Gtk::FILL );
	device_info .push_back( mk_label( "" ) ) ;
	table ->attach( * device_info .back( ), 1, 2, top++, bottom++, Gtk::FILL );
	
	vbox_info .pack_start( *table, Gtk::PACK_SHRINK );
}

void Win_GParted::init_operationslist( ) 
{
	//create listview for pending operations
	liststore_operations = Gtk::ListStore::create( treeview_operations_columns );
	treeview_operations .set_model( liststore_operations );
	treeview_operations .set_headers_visible( false );
	treeview_operations .append_column( "", treeview_operations_columns .operation_number );
	treeview_operations .append_column( "", treeview_operations_columns .operation_description );
	treeview_operations .get_column( 0 ) ->pack_start( treeview_operations_columns .operation_icon, false );
	treeview_operations .get_selection( ) ->set_mode( Gtk::SELECTION_NONE );

	//init scrollwindow_operations
	scrollwindow = manage( new Gtk::ScrolledWindow( ) ) ;
	scrollwindow ->set_shadow_type( Gtk::SHADOW_ETCHED_IN );
	scrollwindow ->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
	scrollwindow ->add ( treeview_operations ) ;
	
	//set up the close and clear buttons and pack them in a vbox
	vbox = manage( new Gtk::VBox( ) ) ;
	//CLOSE
	button = manage( new Gtk::Button( ) );
	image = manage( new Gtk::Image( Gtk::Stock::CLOSE, Gtk::ICON_SIZE_MENU ) );
	button ->add( *image ) ;
	button ->set_relief( Gtk::RELIEF_NONE );
	tooltips .set_tip( *button, _("Hide operationslist") );
	button ->signal_clicked( ) .connect( sigc::mem_fun( *this, &Win_GParted::close_operationslist ) );
	vbox ->pack_start( *button, Gtk::PACK_SHRINK );
	
	//CLEAR
	button = manage( new Gtk::Button( ) );
	image = manage( new Gtk::Image( Gtk::Stock::CLEAR, Gtk::ICON_SIZE_MENU ) );
	button ->add( *image ) ;
	button ->set_relief( Gtk::RELIEF_NONE );
	tooltips .set_tip( *button, _("Clear operationslist") );
	button ->signal_clicked( ) .connect( sigc::mem_fun( *this, &Win_GParted::clear_operationslist ) );
	vbox ->pack_start( *button, Gtk::PACK_SHRINK );
	
	//add vbox and scrollwindow_operations to hbox_operations
	hbox_operations .pack_start( *vbox, Gtk::PACK_SHRINK );
	hbox_operations .pack_start( *scrollwindow, Gtk::PACK_EXPAND_WIDGET );
}

void Win_GParted::init_hpaned_main( ) 
{
	//left scrollwindow (holds device info)
	scrollwindow = manage( new Gtk::ScrolledWindow( ) ) ;
	scrollwindow ->set_shadow_type( Gtk::SHADOW_ETCHED_IN );
	scrollwindow ->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );

	hpaned_main.pack1( *scrollwindow, true, true );
	scrollwindow ->add( vbox_info );

	//right scrollwindow (holds treeview with partitions)
	scrollwindow = manage( new Gtk::ScrolledWindow( ) ) ;
	scrollwindow ->set_shadow_type( Gtk::SHADOW_ETCHED_IN );
	scrollwindow ->set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
	
	//connect signal and add treeview_detail
	treeview_detail.signal_partition_selected.
		connect( sigc::mem_fun( *this, &Win_GParted::on_partition_selected ) );
	treeview_detail.signal_partition_activated.
		connect( sigc::mem_fun( *this, &Win_GParted::on_partition_activated ) );
	treeview_detail.signal_popup_menu.
		connect( sigc::mem_fun( *this, &Win_GParted::on_popup_menu ) );

	if ( installer_mode > 0 )
	{
		treeview_detail .signal_set_mount_point
			.connect( sigc::mem_fun( *this, &Win_GParted::on_set_mount_point ) );
		treeview_detail .signal_get_mount_point
			.connect( sigc::mem_fun( *this, &Win_GParted::on_get_mount_point ) );

		treeview_detail .Add_Mount_Point_Column( );
	}

	scrollwindow ->add( treeview_detail );
	hpaned_main.pack2( *scrollwindow, true, true );
}

void Win_GParted::Refresh_OptionMenu( ) 
{
	optionmenu_devices .get_menu( ) ->items( ) .clear( ) ;
	
	//fill optionmenu_devices
	for ( unsigned int i = 0 ; i < devices .size( ) ; i++ )
	{ 
		hbox = manage( new Gtk::HBox( ) );
		
		//the image...
		image = manage( new Gtk::Image( "/usr/share/icons/gnome/24x24/devices/gnome-dev-harddisk.png" ) );
		hbox ->pack_start( *image, Gtk::PACK_SHRINK );
			
		//the label...
		hbox ->pack_start( *mk_label( " " + devices[ i ] .path + "\t(" + String::ucompose( _("%1 MB"), Sector_To_MB( devices[ i ] .length ) ) + ")" ), Gtk::PACK_SHRINK );
		
		menu_item = manage( new Gtk::MenuItem( *hbox ) ) ;
		optionmenu_devices .get_menu( ) ->items( ) .push_back( *menu_item );
	}
		
	hbox_toolbar .show_all_children( );	
	
	optionmenu_devices .set_history( current_device ) ;
}

void Win_GParted::Show_Pulsebar( ) 
{
	pulsebar ->show( );
	statusbar .push( _("Scanning all devices...") ) ;
	
	//disable all input stuff
	toolbar_main .set_sensitive( false ) ;

	if ( installer_mode == 0 )
		menubar_main .set_sensitive( false ) ;

	optionmenu_devices .set_sensitive( false ) ;
	menu_partition .set_sensitive( false ) ;
		
	//the actual 'pulsing'
	while ( pulse )
	{
		pulsebar ->pulse( );
		while ( Gtk::Main::events_pending( ) )
			Gtk::Main::iteration( );
		
		usleep( 10000 );
	}
	
	thread ->join( ) ;
	conn .disconnect( ) ;
	
	pulsebar ->hide( );
	statusbar .pop( ) ;
		
	//enable all disabled stuff
	toolbar_main .set_sensitive( true ) ;

	if ( installer_mode == 0 )
		menubar_main .set_sensitive( true ) ;

	optionmenu_devices .set_sensitive( true ) ;
	menu_partition .set_sensitive( true ) ;
}

void Win_GParted::Fill_Label_Device_Info( bool clear ) 
{
	if ( clear )
		for ( unsigned int t = 0 ; t < device_info .size( ) ; t++ )
			device_info[ t ] ->set_text( "" ) ;
		
	else
	{		
		short t = 0;
		
		//global info...
		device_info[ t++ ] ->set_text( devices[ current_device ] .model ) ;
		device_info[ t++ ] ->set_text( String::ucompose( _("%1 MB"), Sector_To_MB( devices[ current_device ] .length ) ) ) ;
		device_info[ t++ ] ->set_text( devices[ current_device ] .path ) ;
		device_info[ t++ ] ->set_text( devices[ current_device ] .realpath ) ;
		
		//detailed info
		device_info[ t++ ] ->set_text( devices[ current_device ] .disktype ) ;
		device_info[ t++ ] ->set_text( num_to_str( devices[ current_device ] .heads ) );
		device_info[ t++ ] ->set_text( num_to_str( devices[ current_device ] .sectors ) );
		device_info[ t++ ] ->set_text( num_to_str( devices[ current_device ] .cylinders ) );
		device_info[ t++ ] ->set_text( num_to_str( devices[ current_device ] .length ) );
	}
}

bool Win_GParted::on_delete_event( GdkEventAny *event )
{
	return ! Quit_Check_Operations( );
}	

void Win_GParted::Add_Operation( OperationType operationtype, const Partition & new_partition)
{
	Operation operation( devices[ current_device ], selected_partition, new_partition, operationtype );
		
	operations.push_back( operation );
	
	allow_undo( true );
	allow_apply( true );
	
	Refresh_Visual( );
	
	if ( operations .size( ) == 1 ) //first operation, open operationslist
		open_operationslist( ) ;
	
	//make scrollwindow focus on the last operation in the list	
	Gtk::TreeIter iter = liststore_operations ->children( ) .end( ) ;
	iter-- ;
	treeview_operations .set_cursor( static_cast <Gtk::TreePath> ( static_cast <Gtk::TreeRow> ( *iter ) ) ) ;
}

void Win_GParted::Refresh_Visual( )
{
	std::vector<Partition> partitions = devices[ current_device ] .device_partitions ; 
	liststore_operations ->clear( );
	
	//make all operations visible
	for ( unsigned int t = 0 ; t < operations .size( ); t++ )
	{	
		if ( operations[ t ] .device .path == devices[ current_device ] .path )
			operations[ t ] .Apply_Operation_To_Visual( partitions ) ;
			
		treerow = *( liststore_operations ->append( ) );
		treerow[ treeview_operations_columns .operation_number ] = t +1;
		treerow[ treeview_operations_columns .operation_description ] = operations[ t ] .str_operation ;
		switch ( operations[ t ] .operationtype )
		{		
			case GParted::DELETE	:	treerow[ treeview_operations_columns.operation_icon ] =render_icon(Gtk::Stock::DELETE, Gtk::ICON_SIZE_MENU);	
							break;
			case GParted::CREATE	: 	treerow[ treeview_operations_columns.operation_icon ] =render_icon(Gtk::Stock::NEW, Gtk::ICON_SIZE_MENU);
							break;
			case GParted::RESIZE_MOVE: 	treerow[ treeview_operations_columns.operation_icon ] =render_icon(Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_MENU);
							break;
			case GParted::CONVERT	: 	treerow[ treeview_operations_columns.operation_icon ] =render_icon(Gtk::Stock::CONVERT, Gtk::ICON_SIZE_MENU);
							break;
			case GParted::COPY	: 	treerow[ treeview_operations_columns.operation_icon ] =render_icon(Gtk::Stock::COPY, Gtk::ICON_SIZE_MENU);
							break;
		}
		
	}
	
	//set new statusbartext
	statusbar .pop( ) ;
	if ( operations .size( ) != 1 )
		statusbar .push( String::ucompose( _("%1 operations pending"), operations .size( ) ) );
	else
		statusbar .push( _( "1 operation pending" ) );
		
	if ( ! operations .size( ) ) 
	{
		allow_undo( false );
		allow_apply( false );
	}
			
	//count primary's, check for extended and logic and see if any logical is busy
	any_logic = any_extended = false;
	primary_count = 0;
	for ( unsigned int t = 0 ; t < partitions .size( ) ; t++ )
	{
		if ( partitions[ t ] .partition == copied_partition .partition )
			copied_partition = partitions[ t ] ;
		
		switch ( partitions[ t ] .type )
		{
			case GParted::PRIMARY	:	primary_count++;
							break;
			case GParted::EXTENDED	: 	any_extended = true;
							primary_count++;
							any_logic = partitions[ t ] .logicals .size( ) -1 ;
							break;
			default			:	break;
		}
	}
	
	//vbox visual
	if ( vbox_visual_disk != NULL )
	{
		hbox_visual .remove( *vbox_visual_disk );
		delete ( vbox_visual_disk );
	}
	
	vbox_visual_disk = new VBox_VisualDisk ( partitions, devices[ current_device ] .length ) ;
	vbox_visual_disk ->signal_partition_selected.
		connect( sigc::mem_fun( treeview_detail, &TreeView_Detail::Set_Selected ) );
	vbox_visual_disk ->signal_partition_activated.
		connect( sigc::mem_fun( *this, &Win_GParted::on_partition_activated ) );
	vbox_visual_disk ->signal_popup_menu.
		connect( sigc::mem_fun( *this, &Win_GParted::on_popup_menu ) );
	hbox_visual .pack_start( *vbox_visual_disk, Gtk::PACK_EXPAND_PADDING ) ;
	hbox_visual .show_all_children( ) ;

	//treeview details
	treeview_detail .Load_Partitions( partitions ) ;
	
	//no partition can be selected after a refresh..
	selected_partition .Reset( ) ;
	Set_Valid_Operations( ) ;
}

bool Win_GParted::Quit_Check_Operations( )
{
	if ( operations .size( ) )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" + (Glib::ustring) _( "Quit GParted?" ) + "</span>\n\n" ;
	
		if ( operations .size( ) != 1 )
			str_temp += String::ucompose( _("%1 operations are currently pending."), operations .size( ) ) ;
		else
			str_temp += _("1 operation is currently pending.");
				
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
		dialog .set_keep_above( true );
		dialog .add_button( Gtk::Stock::QUIT, Gtk::RESPONSE_CLOSE );
		dialog .add_button( Gtk::Stock::CANCEL,Gtk::RESPONSE_CANCEL );

		//In installer mode, don't dump the mount point list
		//if the user requested to discard pending operations.
		return ( dialog .run( ) == Gtk::RESPONSE_CLOSE );
	}

	return true; //close GParted
}

void Win_GParted::Set_Valid_Operations( )
{
	allow_new( false ); allow_delete( false ); allow_resize( false ); allow_copy( false );
	allow_paste( false ); allow_convert( false ); allow_unmount( false ) ; allow_info( false ) ;
	
	//no partition selected...	
	if ( selected_partition .partition .empty( ) )
		return ;
	
	//if there's something, there's some info ;)
	allow_info( true ) ;	
	
	//only unmount is allowed
	if ( selected_partition .busy )
	{
		if ( ! selected_partition .Is_Swap( ) && selected_partition .type != GParted::EXTENDED )
			allow_unmount( true ) ;
		
		return;
	}
	
	//UNALLOCATED
	if ( selected_partition .type == GParted::UNALLOCATED )
	{
		allow_new( true );
		
		//find out if there is a copied partition and if it fits inside this unallocated space
		if ( ! copied_partition .partition .empty( ) && ! devices[ current_device ] .readonly )
		{
			if (	(copied_partition .Get_Length_MB( ) + devices[ current_device ] .cylsize) < selected_partition .Get_Length_MB( ) ||
				(copied_partition .filesystem == "xfs" && (copied_partition .Get_Used_MB( ) + devices[ current_device ] .cylsize) < selected_partition .Get_Length_MB( ) )
			)
				allow_paste( true ) ;
		}			
		
		return ;
	}
	
	//EXTENDED
	if ( selected_partition .type == GParted::EXTENDED )
	{
		if (  ! any_logic ) //deletion is only allowed when there are no logical partitions inside.
			allow_delete( true ) ;
		
		if ( ! devices[ current_device ] .readonly )
			allow_resize( true ); 
		
		return ;
	}	
	
	//PRIMARY and LOGICAL
	if (  selected_partition .type == GParted::PRIMARY || selected_partition .type == GParted::LOGICAL )
	{
		fs = gparted_core .get_fs( selected_partition .filesystem ) ;
		
		allow_delete( true ) ;
		allow_convert( true ) ;
		
		//find out if resizing/moving is possible
		if ( (fs .grow || fs .shrink) && ! devices[ current_device ] .readonly ) 
			allow_resize( true ) ;
			
		//only allow copying of real partitions
		if ( selected_partition .status == GParted::STAT_REAL && fs .copy )
			allow_copy( true ) ;	
						
		return ;
	}
}

void Win_GParted::Set_Valid_Convert_Filesystems( ) 
{
	//disable conversion to the same filesystem
	for ( unsigned int t = 0 ; t < gparted_core .get_filesystems( ) .size( ) -1 ; t++ )
	{
		if ( gparted_core .get_filesystems( )[ t ] .filesystem == selected_partition .filesystem || ! gparted_core .get_filesystems( )[ t ] .create )
			menu_convert .items( )[ t ] .set_sensitive( false ) ;
		else 
			menu_convert .items( )[ t ] .set_sensitive( true ) ;
	}
}

void Win_GParted::open_operationslist( ) 
{
	hbox_operations .show( ) ;
	int x,y; this ->get_size( x, y );
	y -= 300;
		
	for ( int t = vpaned_main .get_position( ) ; t > y ; t-=5 )
	{
		vpaned_main .set_position( t );
		while ( Gtk::Main::events_pending( ) ) 
			Gtk::Main::iteration( );
	}
	
	if ( installer_mode == 0 )
		( (Gtk::CheckMenuItem *) & menubar_main .items( ) [ 1 ] .get_submenu( ) ->items( ) [ 1 ] ) ->set_active( true ) ;
}

void Win_GParted::close_operationslist( ) 
{
	int x,y; this ->get_size( x, y );
	y -= 210 ; //height of whole app - menubar - visualdisk - statusbar ....
	for ( int t = vpaned_main .get_position( ) ; t < y ; t+=5 )
	{
		vpaned_main .set_position( t );
		while ( Gtk::Main::events_pending( ) )
			Gtk::Main::iteration( );
	}
	
	hbox_operations .hide( ) ;

	if ( installer_mode == 0 )
		( (Gtk::CheckMenuItem *) & menubar_main .items( ) [ 1 ] .get_submenu( ) ->items() [ 1 ] ) ->set_active( false ) ;
}

void Win_GParted::clear_operationslist( ) 
{
	operations .clear( ) ;
	Refresh_Visual( ) ;
}

void Win_GParted::optionmenu_devices_changed( )
{	
	//set new current device
	current_device = optionmenu_devices .get_history( ) ;
	
	//refresh label_device_info
	Fill_Label_Device_Info( );
	
	//rebuild visualdisk and treeview
	Refresh_Visual( );
}

void Win_GParted::menu_gparted_refresh_devices( )
{
	//find out if there was any change in available devices (think about flexible media like zipdisks/usbsticks/whatever ;-) )
	pulse = true ;//set to true before creating the thread to prevent _possible_ infinite loop in Show_Pulsebar( )
	thread = Glib::Thread::create( SigC::slot_class( *this, &Win_GParted::find_devices_thread ), true );
	
	Show_Pulsebar( ) ;	
	
	//check if current_device is still available (think about hotpluggable shit like usbdevices)
	if ( current_device >= devices .size( ) )
		current_device = 0 ;

	if ( operations .empty( ) )
		gparted_core .Realign_Mount_Point_Map( devices );

	//show read-only warning if necessary
	Glib::ustring readonly_paths ;
	
	for ( unsigned int t = 0 ; t < devices .size( ) ; t++ )
		if ( devices[ t ] .readonly )
			readonly_paths += "\n- " + devices[ t ] .path ;
		
	if ( ! readonly_paths .empty( ) )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += _("The kernel is unable to re-read the partitiontables on the following devices:") ;
		str_temp += readonly_paths ;
		str_temp += "</span>\n\n" ;
		
		str_temp += _("Because of this you will only have limited access to these devices.") ;
		str_temp += "\n" ;
		str_temp += _("Unmount all mounted partitions on a device to get full access.") ;
		
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true ) ;
		dialog .set_keep_above( true );
		dialog .run( ) ;
	}
	
	//see if there are any pending operations on non-existent devices
	//NOTE that this isn't 100% foolproof since some stuff (e.g. sourcedevice of copy) may slip through.
	//but anyone who removes the sourcedevice before applying the operations gets what he/she deserves :-)
	unsigned int i ;
	for ( unsigned int t = 0 ; t < operations .size( ) ; t++ )
	{
		for ( i = 0 ; i < devices .size( ) && devices[ i ] .path != operations[ t ] .device .path ; i++ ) {}
			
		if ( i >= devices .size( ) )
			operations .erase( operations .begin( ) + t-- ) ;//decrease t bij one..
	}
		
	//if no devices were detected we disable some stuff and show a message in the statusbar
	if ( devices .empty( ) )
	{	
		optionmenu_devices .hide( ) ;
		
		if ( installer_mode == 0 )
		{
			menubar_main .items( )[ 2 ] .set_sensitive( false ) ;
			menubar_main .items( )[ 3 ] .set_sensitive( false ) ;
		}
		toolbar_main .set_sensitive( false ) ;
		optionmenu_devices .set_sensitive( false ) ;
		
		Fill_Label_Device_Info( true ) ;
		
		if ( vbox_visual_disk != NULL )
		{
			hbox_visual .remove( *vbox_visual_disk );
			delete( vbox_visual_disk ) ;
			vbox_visual_disk = NULL ;
		}
		
		treeview_detail .Clear( ) ;
		
		//hmzz, this is really paranoid, but i think it's the right thing to do ;)
		liststore_operations ->clear( ) ;
		close_operationslist( ) ;
		operations .clear( ) ;
		
		statusbar .pop( ) ;
		statusbar .push( _( "No devices detected" ) );
	}
	
	else //at least one device detected
	{
		if ( installer_mode == 0 )
		{
			menubar_main .items( )[ 2 ] .set_sensitive( true ) ;
			menubar_main .items( )[ 3 ] .set_sensitive( true ) ;
		}
		Refresh_OptionMenu( ) ;	
	}
}

void Win_GParted::menu_gparted_filesystems( )
{
	Dialog_Filesystems dialog ;
	dialog .set_transient_for( *this ) ;
	
	dialog .Load_Filesystems( gparted_core .get_filesystems( ) ) ;
	while ( dialog .run( ) == Gtk::RESPONSE_OK )
	{
		gparted_core .find_supported_filesystems( ) ;
		dialog .Load_Filesystems( gparted_core .get_filesystems( ) ) ;
	}
}

void Win_GParted::menu_gparted_quit( )
{
	if ( Quit_Check_Operations( ) )
		this ->hide( );
}

void Win_GParted::menu_view_harddisk_info( )
{ 
	if ( ( (Gtk::CheckMenuItem *) & menubar_main .items( ) [ 1 ] .get_submenu( ) ->items( ) [ 0 ] ) ->get_active( ) )
	{ //open harddisk information
		hpaned_main .get_child1( ) ->show( ) ;
		for ( int t = hpaned_main .get_position( ) ; t < 250 ; t +=15 )
		{
			hpaned_main .set_position( t );
			while ( Gtk::Main::events_pending( ) )
				Gtk::Main::iteration( );
		}
	} 
	else 
	{ 	//close harddisk information
		for ( int t=hpaned_main .get_position( ) ;  t > 0 ; t -=15 )
		{
			hpaned_main .set_position( t );
			while ( Gtk::Main::events_pending( ) )
				Gtk::Main::iteration( );
		}
		hpaned_main .get_child1( ) ->hide( ) ;
	}
}

void Win_GParted::menu_view_operations( )
{
	if ( ( (Gtk::CheckMenuItem *) & menubar_main .items( ) [ 1 ] .get_submenu( ) ->items( ) [ 1 ] ) ->get_active( ) )
		open_operationslist( ) ;
	else
		close_operationslist( ) ;
}

void Win_GParted::menu_help_contents( )
{
	str_temp =  _("Sorry, not yet implemented.") ;
	str_temp += "\n" ;
	str_temp += _( "Please visit http://gparted.sf.net for more information and support.") ;
	Gtk::MessageDialog dialog( *this, str_temp, false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true );
	dialog .set_keep_above( true );
	dialog .run( );
}


void Win_GParted::menu_help_about( )
{
	Dialog_About dialog ;
	dialog .set_transient_for( *this ) ;
	
	dialog .run( );
}

void Win_GParted::on_partition_selected( const Partition & partition )
{
	vbox_visual_disk ->Set_Selected( partition );

	selected_partition = partition;

	Set_Valid_Operations( );
}

void Win_GParted::on_partition_activated( const Partition & partition )
{
	if ( ! pulse )
		activate_info( );
}

void Win_GParted::on_popup_menu( unsigned int button, guint32 time )
{
	//prepare convert menu
	if ( selected_partition .type != GParted::UNALLOCATED )
		Set_Valid_Convert_Filesystems( ) ;

	menu_partition .popup( button, time );
}

void Win_GParted::on_set_mount_point( const Partition & partition, const Glib::ustring & mount_point )
{
	gparted_core .Set_Mount_Point( devices[ current_device ] .path, partition, mount_point );
}

Glib::ustring Win_GParted::on_get_mount_point( const Partition & partition )
{
	return gparted_core .Get_Mount_Point( devices[ current_device ] .path, partition );
}

bool Win_GParted::max_amount_prim_reached( ) 
{
	//Display error if user tries to create more primary partitions than the partition table can hold. 
	if ( ! selected_partition .inside_extended && primary_count >= devices[ current_device ] .max_prims )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += String::ucompose( _("It is not possible to create more than %1 primary partitions"), devices[ current_device ] .max_prims ) ;
		str_temp += "</span>\n\n" ;
		str_temp += _( "If you want more partitions you should first create an extended partition. Such a partition can contain other partitions.") ;
										
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true ) ;
		dialog .set_keep_above( true );
		dialog .run( ) ;
		return true ;
	}
	
	return false ;
}

void Win_GParted::activate_resize( )
{
	//show warning when one tries to resize a fat16 filesystem
	if ( selected_partition .filesystem == "fat16" )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += _( "Are you sure you want to resize/move this partition?" ) ;
		str_temp += "</span>\n\n" ;
		str_temp += _( "Resizing a fat16 partition can be quite tricky! Especially growing such a partition is very error-prone. It is advisable to first convert the filesystem to fat32.") ;
		str_temp += "\n";
						
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CANCEL, true) ; 
		dialog .set_keep_above( true );
		//pffff this whole mess only for this f*cked up filesystem :-(
		Gtk::Button button_resize_move ;
		Gtk::HBox hbox_resize_move;
				
		image = manage( new Gtk::Image( Gtk::Stock::GOTO_LAST, Gtk::ICON_SIZE_BUTTON ) );
		hbox_resize_move .pack_start( *image, Gtk::PACK_SHRINK ) ;
		hbox_resize_move .pack_start( * mk_label( _("Resize/Move") ), Gtk::PACK_SHRINK ) ;
		button_resize_move .add( hbox_resize_move ) ;
				
		dialog .add_action_widget( button_resize_move, Gtk::RESPONSE_OK ) ;
		dialog .show_all_children( ) ;
		
		if ( dialog .run( ) == Gtk::RESPONSE_CANCEL )
			return ;
	}
	
	std::vector <Partition> partitions = devices[ current_device ] .device_partitions ;
	
	if ( operations .size( ) )
		for (unsigned int t = 0 ; t < operations .size( ) ; t++ )
			if ( operations[ t ] .device .path == devices[ current_device ] .path )
				operations[ t ] .Apply_Operation_To_Visual( partitions ) ;
	
	Dialog_Partition_Resize_Move dialog( gparted_core .get_fs( selected_partition .filesystem ), devices[ current_device ] .cylsize ) ;
			
	if ( selected_partition .type == GParted::LOGICAL )
	{
		unsigned int ext = 0 ;
		while ( ext < partitions .size( ) && partitions[ ext ] .type != GParted::EXTENDED ) ext++ ;
		dialog .Set_Data( selected_partition, partitions[ ext ] .logicals );
	}
	else
		dialog .Set_Data( selected_partition, partitions );
		
	dialog .set_transient_for( *this ) ;	
			
	if ( dialog .run( ) == Gtk::RESPONSE_OK )
	{
		dialog .hide( ) ;//i want to be sure the dialog is gone _before_ operationslist shows up (only matters if first operation)
		
	//unset any mount point associated with this partition
	gparted_core .Set_Mount_Point( devices[ current_device ] .path, selected_partition, Glib::ustring( ) );

		//if selected_partition is NEW we simply remove the NEW operation from the list and add it again with the new size and position ( unless it's an EXTENDED )
		if ( selected_partition .status == GParted::STAT_NEW && selected_partition.type != GParted::EXTENDED )
		{
			//remove operation which creates this partition
			for ( unsigned int t = 0 ; t < operations .size( ) ; t++ )
			{
				if ( operations[ t ] .partition_new .partition == selected_partition .partition )
				{
					operations.erase( operations .begin( ) + t ) ;
					
					//And add the new partition to the end of the operations list
					Add_Operation( GParted::CREATE, dialog .Get_New_Partition( ) );
					
					break;
				}
			}
		}
		else//normal move/resize on existing partition
			Add_Operation( GParted::RESIZE_MOVE, dialog .Get_New_Partition( ) );
	}
}

void Win_GParted::activate_copy( )
{
	copied_partition = selected_partition ;
}

void Win_GParted::activate_paste( )
{
	if ( ! max_amount_prim_reached( ) )
	{
		Dialog_Partition_Copy dialog( gparted_core .get_fs( copied_partition .filesystem ), devices[ current_device ] .cylsize ) ;
		copied_partition .error .clear( ) ; //we don't need the errors of the source partition.
		dialog .Set_Data( selected_partition, copied_partition ) ;
		dialog .set_transient_for( *this );
		
		if ( dialog .run( ) == Gtk::RESPONSE_OK )
		{
			dialog .hide( ) ;//i want to be sure the dialog is gone _before_ operationslist shows up (only matters if first operation)
			Add_Operation( GParted::COPY, dialog .Get_New_Partition( ) );		
		}
	}
}

void Win_GParted::activate_new( )
{
	//if max_prims == -1 the current device has an unrecognised disklabel (see also GParted_Core::get_devices)
	if ( devices [ current_device ] .max_prims == -1 )
		activate_disklabel( ) ;
			
	else if ( ! max_amount_prim_reached( ) )
	{	
		Dialog_Partition_New dialog;
		
		dialog .Set_Data( selected_partition, any_extended, new_count, gparted_core .get_filesystems( ), devices[ current_device ] .readonly, devices[ current_device ] .cylsize ) ;
		dialog .set_transient_for( *this );
		
		if ( dialog .run( ) == Gtk::RESPONSE_OK )
		{
			dialog .hide( ) ;//make sure the dialog is gone _before_ operationslist shows up (only matters if first operation)
			new_count++ ;
			Add_Operation( GParted::CREATE, dialog .Get_New_Partition( ) );
		}
	}
}

void Win_GParted::activate_delete( )
{ 
	//since logicals are *always* numbered from 5 to <last logical> there can be a shift in numbers after deletion.
	//e.g. consider /dev/hda5 /dev/hda6 /dev/hda7. Now after removal of /dev/hda6, /dev/hda7 is renumbered to /dev/hda6
	//the new situation is now /dev/hda5 /dev/hda6. If /dev/hda7 was mounted the OS cannot find /dev/hda7 anymore and the results aren't that pretty
	//it seems best to check for this and prohibit deletion with some explanation to the user.
	if ( 	selected_partition .type == GParted::LOGICAL &&
		selected_partition .status != GParted::STAT_NEW && 
		selected_partition .partition_number < devices [ current_device ] .highest_busy )
	{	
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += _( "Unable to delete partition!") ;
		str_temp += "</span>\n\n" ;
		str_temp += String::ucompose( _("Please unmount any logical partitions having a number higher than %1"), selected_partition .partition_number ) ;
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true ) ;
		dialog .set_keep_above( true );
		dialog .run( ) ;
		return;
	}
	
	str_temp = "<span weight=\"bold\" size=\"larger\">" ;
	str_temp += String::ucompose( _( "Are you sure you want to delete %1?"), selected_partition .partition ) + "</span>" ;
	if ( selected_partition .partition == copied_partition .partition )
	{
		str_temp += "\n\n" ;
		str_temp += _( "After deletion this partition is no longer available for copying.") ;
	}
	
	Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
	dialog .set_keep_above( true );
	/*TO TRANSLATORS: dialogtitle, looks like   Delete /dev/hda2 (ntfs, 2345 MB) */
	dialog .set_title( String::ucompose( _("Delete %1 (%2, %3 MB)"), selected_partition .partition, selected_partition .filesystem, selected_partition .Get_Length_MB() ) );
	dialog .add_button( Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL );
	dialog .add_button( Gtk::Stock::DELETE, Gtk::RESPONSE_OK );
	
	dialog .show_all_children( );
	if ( dialog .run( ) == Gtk::RESPONSE_OK )
	{
		dialog .hide( ) ;//i want to be sure the dialog is gone _before_ operationslist shows up (only matters if first operation)

		//unset any mount point associated with this partition
		gparted_core .Set_Mount_Point( devices[ current_device ] .path, selected_partition, Glib::ustring( ) );

		//if deleted partition was on the clipboard we erase it...
		if ( selected_partition .partition == copied_partition .partition )
			copied_partition .Reset( ) ;
			
		//if deleted one is NEW, it doesn't make sense to add it to the operationslist, we erase its creation
		//and possible modifications like resize etc.. from the operationslist.   Calling Refresh_Visual will wipe every memory of its existence ;-)
		if ( selected_partition .status == GParted::STAT_NEW )
		{
			//remove all operations done on this new partition (this includes creation)	
			for ( int t = 0 ; t < static_cast<int> ( operations .size( ) ) ; t++ ) //I removed the unsigned because t will be negative at times...
				if ( operations[ t ] .partition_new .partition == selected_partition .partition )
					operations.erase( operations .begin( ) + t-- ) ;
				
			//determine lowest possible new_count
			new_count = 0 ; 
			for ( unsigned int t = 0 ; t < operations .size( ) ; t++ )
				if ( operations[ t ] .partition_new .status == GParted::STAT_NEW && operations[ t ] .partition_new .partition_number > new_count )
					new_count = operations[ t ] .partition_new .partition_number ;
			
			new_count += 1 ;
			
			Refresh_Visual( ); 
				
			if ( ! operations .size( ) )
				close_operationslist( ) ;
		}
		else //deletion of a real partition...
			Add_Operation( GParted::DELETE, selected_partition ); //in this case selected_partition is just a "dummy" 
	}
}

void Win_GParted::activate_info( )
{
	Dialog_Partition_Info dialog( selected_partition );
	dialog .set_transient_for( *this );
	dialog .run( );
}

void Win_GParted::activate_convert( const Glib::ustring & new_fs )
{
	//standard warning..
	str_temp = "<span weight=\"bold\" size=\"larger\">" ;
	str_temp += String::ucompose( _("Are you sure you want to convert this filesystem to %1?"), new_fs ) + "</span>\n\n" ;
	str_temp += String::ucompose( _("This operation will destroy all data on %1"), selected_partition .partition ) ;
	
	Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_CANCEL, true );
	dialog .set_keep_above( true );
	
	dialog. add_button( Gtk::Stock::CONVERT, Gtk::RESPONSE_OK ) ;
	dialog. show_all_children( ) ;
	
	if ( dialog .run( ) == Gtk::RESPONSE_CANCEL )
		return ;
	
	dialog .hide( ) ;//i want to be sure the dialog is gone _before_ operationslist shows up (only matters if first operation)
	
	//check for some limits...
	fs = gparted_core .get_fs( new_fs ) ;
	
	if ( selected_partition .Get_Length_MB( ) < fs .MIN || ( fs .MAX && selected_partition .Get_Length_MB( ) > fs .MAX ) )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += String::ucompose( _("Can not convert this filesystem to %1."), new_fs ) ;
		str_temp += "</span>\n\n" ;
		
		if ( selected_partition .Get_Length_MB( ) < fs .MIN )
			str_temp += String::ucompose( _( "A %1 filesystem requires a partition of at least %2 MB."), new_fs, fs .MIN ) ;
		else
			str_temp += String::ucompose( _( "A partition with a %1 filesystem has a maximum size of %2 MB."), new_fs, fs .MAX ) ;
		
		
		Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true );
		dialog .set_keep_above( true );
		dialog .run( ) ;
		return ;
	}
	
	//unset any mount point associated with this partition
	gparted_core .Set_Mount_Point( devices[ current_device ] .path, selected_partition, Glib::ustring( ) );


	//ok we made it :P lets create an fitting partition object
	Partition part_temp;
	part_temp .Set( selected_partition .partition, selected_partition .partition_number, selected_partition .type, new_fs, selected_partition .sector_start, selected_partition .sector_end, selected_partition .inside_extended, false ) ;
	
	
	//if selected_partition is NEW we simply remove the NEW operation from the list and add it again with the new filesystem
	if ( selected_partition .status == GParted::STAT_NEW  )
	{
		//remove operation which creates this partition
		for ( unsigned int t = 0 ; t < operations .size( ) ; t++ )
		{
			if ( operations[ t ] .partition_new .partition == selected_partition .partition )
			{
				operations .erase( operations .begin( ) +t ) ;
				
				//And add the new partition to the end of the operations list (NOTE: in this case we set status to STAT_NEW)
				part_temp .status = STAT_NEW ;
				Add_Operation( GParted::CREATE, part_temp );
					
				break;
			}
		}
	}
	else//normal converting of an existing partition
		Add_Operation( GParted::CONVERT, part_temp ) ;
}

void  Win_GParted::activate_unmount( ) 
{
	char c_buf[ 512 ] ;
	Glib::ustring output ;

	FILE *f = popen( ( "umount " + selected_partition .partition + " 2>&1" ) .c_str( ), "r" ) ;
	
	while ( fgets( c_buf, 512, f ) )
		output += c_buf ;
	
	pclose( f ) ;
	
	if ( ! output .empty( ) ) 
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += String::ucompose( _("Could not unmount %1"), selected_partition .partition ) ;
		str_temp += "</span>\n\n" ;
				
		Gtk::MessageDialog dialog( *this, str_temp + output, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true );
		dialog .set_keep_above( true );
		dialog.run( ) ;
	} 
	else
		menu_gparted_refresh_devices( ) ;
}

void Win_GParted::activate_disklabel( )
{
	Dialog_Disklabel dialog( devices[ current_device ] .path ) ;
	dialog .set_transient_for( *this );
		
	if ( dialog .run( ) == Gtk::RESPONSE_OK )
	{
		str_temp = "<span weight=\"bold\" size=\"larger\">" ;
		str_temp += String::ucompose( _("Are you sure you want to create a %1 disklabel on %2?"), dialog .Get_Disklabel( ), devices[ current_device ] .path ) ;
		str_temp += "</span>\n\n" ;
		str_temp += String::ucompose( _("This operation will destroy all data on %1"), devices[ current_device ] .path ) ;
		
		Gtk::MessageDialog m_dialog( *this, str_temp, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_CANCEL, true ) ;
		dialog .set_keep_above( true );
		m_dialog .add_button( _("Create"), Gtk::RESPONSE_OK );
		
		if ( m_dialog .run( ) == Gtk::RESPONSE_OK && ! gparted_core .Set_Disklabel( devices[ current_device ] .path, dialog .Get_Disklabel( ) ) )
		{
			Gtk::MessageDialog dialog( *this, _("Error while setting new disklabel"), true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true ) ;
			dialog .set_keep_above( true );
			dialog .run( ) ;
		}

		//unset any mount point associated with this partition
		gparted_core .Unset_Device_Mount_Points( devices[ current_device ] .path );

		menu_gparted_refresh_devices( ) ;
	}
}

void Win_GParted::activate_undo( )
{
	//when undoing an creation it's safe to decrease the newcount by one
	if ( operations .back( ) .operationtype == GParted::CREATE )
		new_count-- ;
	
	operations.erase( operations .end( ) );
	
	Refresh_Visual( );
	
	if ( ! operations .size( ) )
		close_operationslist( ) ;
}

void Win_GParted::activate_apply( )
{
	str_temp = "<span weight=\"bold\" size=\"larger\">" ;
	str_temp += _( "Are you sure you want to apply the pending operations?" ) ;
	str_temp += "</span>\n\n" ;
	str_temp += _( "It is recommended to backup valueable data before proceeding.") ;
	
	Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_NONE, true );
	dialog .set_keep_above( true );
	dialog .set_title( _( "Apply operations to harddisk" ) );
	
	dialog .add_button( Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL );
	dialog .add_button( Gtk::Stock::APPLY, Gtk::RESPONSE_OK );
	
	dialog .show_all_children( ) ;
	if ( dialog.run( ) == Gtk::RESPONSE_OK )
	{
		dialog .hide( ) ; //hide confirmationdialog
		
		apply = true;
		dialog_progress = new Dialog_Progress ( operations .size( ), gparted_core .get_textbuffer( ) ) ;
		
		conn = dispatcher .connect( sigc::mem_fun(*dialog_progress, &Dialog_Progress::Set_Operation) );
				
		thread = Glib::Thread::create( SigC::slot_class( *this, &Win_GParted::apply_operations_thread ), true );
		
		dialog_progress ->set_transient_for( *this );
		while ( dialog_progress ->run( ) != Gtk::RESPONSE_OK ) 
			apply = false ;//finish current operation . then stop applying operations
			
		//after hiding the progressdialog
		delete ( dialog_progress ) ;
		thread ->join( ) ;
		conn .disconnect( ) ;
		
		//find out if any of the involved devices is busy
		bool any_busy = false ;
		for ( unsigned int t = 0; t < devices .size( ) && ! any_busy; t++ )
			if ( devices[ t ] .highest_busy )
				for (unsigned int i = 0; i < operations .size( ) && ! any_busy; i++ )
					if ( operations[ i ] .device .path == devices[ t ] .path )
						any_busy = true ;
				
		//show warning if necessary
		if ( any_busy )
		{
			str_temp = "<span weight=\"bold\" size=\"larger\">" ;	
			str_temp += _("At least one operation was applied to a busy device.") ;
			str_temp += "</span>\n\n" ;
			str_temp += _("A busy device is a device with at least one mounted partition.") ;
			str_temp += "\n";
			str_temp += _("Because making changes to a busy device may confuse the kernel, you are advised to reboot your computer.") ;

			Gtk::MessageDialog dialog( *this, str_temp, true, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true );
			dialog .set_keep_above( true );
			dialog .run( ) ;
		}			
					
		//wipe operations...
		operations.clear( ) ;
		liststore_operations ->clear( ) ;
		close_operationslist( ) ;
							
		//reset new_count to 1
		new_count = 1 ;
		
		//reread devices and their layouts...
		menu_gparted_refresh_devices( ) ;

		// if ( installer_mode > 0 )
		// {
		// 	gparted_core .Dump_Mount_Point_List( devices, std::cout );
		// 	hide( );
		// }
	}
}

void Win_GParted::apply_operations_thread( )
{ 
	for ( unsigned int t = 0 ; t < operations .size( ) && apply ; t++ )
	{ 			
		dialog_progress ->Set_Current( operations[ t ] .str_operation,
					       gparted_core .get_estimated_time( operations[ t ] ) );
		dispatcher( ) ;
		
		gparted_core .Apply_Operation_To_Disk( operations[ t ] );
	}
	dispatcher( );
}


} // GParted
