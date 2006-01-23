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
 
#include "../include/TreeView_Detail.h"
#include <gtkmm/cellrenderercombo.h>

namespace GParted
{ 

TreeView_Detail::TreeView_Detail( )
{
	treestore_detail = Gtk::TreeStore::create( treeview_detail_columns );
	this->set_model( treestore_detail );
	this->set_rules_hint(true);
	this->treeselection = this->get_selection();

	//append columns
	this->append_column( _("Partition"), treeview_detail_columns.partition );
	this->append_column( _("Filesystem"), treeview_detail_columns.type_square );
	this->append_column( _("Size(MB)"), treeview_detail_columns.size );
	this->append_column( _("Used(MB)"), treeview_detail_columns.used );
	this->append_column( _("Unused(MB)"), treeview_detail_columns.unused );
	this->append_column( _("Flags"), treeview_detail_columns.flags );

	//status_icon
	this->get_column( 0 ) ->pack_start( treeview_detail_columns.status_icon, false );
	
	//colored text in Partition column (used for darkgrey unallocated)
	Gtk::CellRendererText *cell_renderer_text = dynamic_cast<Gtk::CellRendererText*>( this->get_column( 0 ) ->get_first_cell_renderer( ) );
	this->get_column( 0 ) ->add_attribute( cell_renderer_text ->property_foreground( ), treeview_detail_columns .text_color );
	
	//colored square in Filesystem column
	cell_renderer_text = dynamic_cast<Gtk::CellRendererText*>( this ->get_column( 1 ) ->get_first_cell_renderer( ) );
	this ->get_column( 1 ) ->add_attribute( cell_renderer_text ->property_foreground( ), treeview_detail_columns. color);
	
	//colored text in Filesystem column
	this ->get_column( 1 ) ->pack_start( treeview_detail_columns .type, true );
	
	//this sucks :) get_cell_renderers doesn't return them in some order, so i have to check manually...
	std::vector <Gtk::CellRenderer *> renderers = this ->get_column( 1 ) ->get_cell_renderers( ) ;
	
	if ( renderers .front( ) != this ->get_column( 1 ) ->get_first_cell_renderer( ) )	
		cell_renderer_text = dynamic_cast <Gtk::CellRendererText*> ( renderers .front( ) ) ;
	else 
		cell_renderer_text = dynamic_cast <Gtk::CellRendererText*> ( renderers .back( ) ) ;
	
	this ->get_column( 1 ) ->add_attribute( cell_renderer_text ->property_foreground( ), treeview_detail_columns .text_color );
	
	
	//set alignment of numeric columns to right
	for( int t = 2 ; t < 5 ; t++ )
	{
		cell_renderer_text = dynamic_cast<Gtk::CellRendererText*>( this ->get_column( t ) ->get_first_cell_renderer( ) );
		cell_renderer_text ->property_xalign( ) = 1;
	}

	treeselection ->signal_changed( )
		.connect( sigc::mem_fun( *this, &TreeView_Detail::on_selection_changed ) );
}

void TreeView_Detail::Add_Mount_Point_Column( )
{
	liststore_mount_point = Gtk::ListStore::create( mount_point_columns );
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/boot";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/home";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/pub";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/usr";
	( *liststore_mount_point ->append( ) )[ mount_point_columns .path ] = "/var";

	liststore_use_swap = Gtk::ListStore::create( mount_point_columns );
	( *liststore_use_swap ->append( ) )[ mount_point_columns .path ] = "";
	( *liststore_use_swap ->append( ) )[ mount_point_columns .path ] = "swap";

	Gtk::TreeView::Column * column = new Gtk::TreeView::Column( _("Mount point") );
	this->append_column( *Gtk::manage( column ) );

	Gtk::CellRendererCombo * renderer = new Gtk::CellRendererCombo( );
	column->pack_start( *Gtk::manage( renderer ) );

	renderer ->property_text_column( ) = mount_point_columns .path .index( );

	column->set_cell_data_func( *renderer,
		sigc::mem_fun( *this, &TreeView_Detail::mount_point_cell_data_func ) );
	renderer->signal_edited( ) .connect(
		sigc::mem_fun( *this, &TreeView_Detail::on_mount_point_edited ) );
}

void TreeView_Detail::Load_Partitions( const std::vector<Partition> & partitions ) 
{
	treestore_detail ->clear( ) ;
	
	for ( unsigned int i = 0 ; i < partitions .size( ) ; i++ ) 
	{
		row = *( treestore_detail ->append( ) );
		Create_Row( row, partitions[ i ] );
		
		if ( partitions[ i ] .type == GParted::EXTENDED )
		{
			for ( unsigned int t = 0 ; t < partitions[ i ] .logicals .size( ) ; t++ ) 
			{
				childrow = *( treestore_detail ->append( row.children( ) ) );
				Create_Row( childrow, partitions[ i ] .logicals[ t ] );
			}
			
		}
	}
	
	//show logical partitions ( if any )
	this ->expand_all( );
	
	this ->columns_autosize( );
}

void TreeView_Detail::Set_Selected( const Partition & partition )
{ 
	//look for appropiate row
	for( iter = treestore_detail ->children( ) .begin( ) ; iter != treestore_detail ->children( ) .end( ) ; iter++ )
	{
		row = *iter;
		partition_temp = row[ treeview_detail_columns.partition_struct ] ;
		//primary's
		if (	partition .sector_start >= partition_temp .sector_start &&
			partition .sector_end <=partition_temp .sector_end &&
			partition.inside_extended == partition_temp.inside_extended )
		{
			this ->set_cursor( static_cast <Gtk::TreePath> ( row ) );
			return;
		}
		
		//logicals
		if ( row .children( ) .size( ) > 0 ) //this is the row with the extended partition, search it's childrows...
			for( iter_child = row .children( ) .begin( ) ; iter_child != row.children( ) .end( ) ; iter_child++ )
			{
				childrow = *iter_child;
				partition_temp = childrow[ treeview_detail_columns.partition_struct ] ;
						
				if ( partition .sector_start >= partition_temp .sector_start && partition .sector_end <= partition_temp .sector_end )
				{
					this ->expand_all( );
					this ->set_cursor( static_cast <Gtk::TreePath> ( childrow ) );
					return;
				}
			}

	}
}

void TreeView_Detail::Clear( )
{
	treestore_detail ->clear( ) ;
}

void TreeView_Detail::Create_Row( const Gtk::TreeRow & treerow, const Partition & partition )
{
	//hereby i assume these 2 are mutual exclusive. is this wise?? Time (and bugreports) will tell :)
	if ( partition .busy )
		treerow[ treeview_detail_columns .status_icon ] = render_icon( Gtk::Stock::DIALOG_AUTHENTICATION, Gtk::ICON_SIZE_BUTTON );
	else if ( partition .error != "" )
		treerow[ treeview_detail_columns .status_icon ] = render_icon( Gtk::Stock::DIALOG_WARNING, Gtk::ICON_SIZE_BUTTON );

	treerow[ treeview_detail_columns .partition ] = partition .partition;
	treerow[ treeview_detail_columns .color ] = Get_Color( partition .filesystem ) ;

	treerow[ treeview_detail_columns .text_color ] = ( partition .type == GParted::UNALLOCATED ) ? "darkgrey" : "black" ;
	treerow[ treeview_detail_columns .type ] = partition .filesystem ;
	treerow[ treeview_detail_columns .type_square ] = "██" ;
	
	//size
	treerow[ treeview_detail_columns .size ] = num_to_str( partition .Get_Length_MB( ) ) ;

	//used
	if ( partition .sectors_used != -1 )
		treerow[ treeview_detail_columns .used ] = num_to_str( partition .Get_Used_MB( ) ) ;
	else
		treerow[ treeview_detail_columns .used ] = "---" ;

	//unused
	if ( partition .sectors_unused != -1 )
		treerow[ treeview_detail_columns .unused ] = num_to_str( partition .Get_Unused_MB( ) ) ;
	else
		treerow[ treeview_detail_columns .unused ] = "---" ;
	
	//flags	
	treerow[ treeview_detail_columns .flags ] = " " + partition .flags ;
	
	//hidden column (partition object)
	treerow[ treeview_detail_columns .partition_struct ] = partition;
}

bool TreeView_Detail::on_button_press_event( GdkEventButton * event )
{
	bool handled = Gtk::TreeView::on_button_press_event( event );

	if ( event ->type == GDK_BUTTON_PRESS && event ->button == 3 )
	{
		signal_popup_menu .emit( event ->button, event ->time );
		handled = true;
	}

	return handled;
}

void TreeView_Detail::on_row_activated( const Gtk::TreeModel::Path & path, Gtk::TreeView::Column * column )
{
	Gtk::TreeView::on_row_activated( path, column );

	if ( Gtk::TreeModel::iterator iter = treestore_detail ->get_iter( path ) )
		signal_partition_activated .emit( ( *iter )[ treeview_detail_columns .partition_struct ] );
}

void TreeView_Detail::on_selection_changed( )
{ 
	if ( Gtk::TreeModel::iterator iter = treeselection ->get_selected( ) )
		signal_partition_selected .emit( ( *iter )[ treeview_detail_columns .partition_struct ] );
}

void TreeView_Detail::mount_point_cell_data_func( Gtk::CellRenderer * cell, const Gtk::TreeModel::iterator & iter )
{
	Gtk::CellRendererCombo * renderer = dynamic_cast<Gtk::CellRendererCombo*>( cell );

	Partition partition = ( *iter )[ treeview_detail_columns .partition_struct ];

	if ( ( partition .type == GParted::PRIMARY || partition .type == GParted::LOGICAL ) )
	{
		if ( ! partition .Is_Swap( ) )
		{
			renderer ->property_model( ) = liststore_mount_point;
			renderer ->property_has_entry( ) = true;
		}
		else
		{
			renderer ->property_model( ) = liststore_use_swap;
			renderer ->property_has_entry( ) = false;
		}
		renderer ->property_text( ) = signal_get_mount_point .emit( partition );
		renderer ->property_editable( ) = true;
	}
	else
	{
		renderer ->property_text( ) = Glib::ustring( );
		renderer ->property_editable( ) = false;
	}
}

void TreeView_Detail::on_mount_point_edited( const Glib::ustring & path, const Glib::ustring & new_text )
{
	Gtk::TreeModel::iterator iter = treestore_detail ->get_iter( path );
	g_return_if_fail( iter );

	Partition partition = ( *iter )[ treeview_detail_columns .partition_struct ];
	signal_set_mount_point .emit( partition, new_text );
}

} //GParted
