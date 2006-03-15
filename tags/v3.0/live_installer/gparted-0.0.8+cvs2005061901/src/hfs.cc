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
 
 
#include "../include/hfs.h"

namespace GParted
{

FS hfs::get_filesystem_support( )
{
	FS fs ;
	
	fs .filesystem = "hfs" ;
	if ( ! system( "which hfs 1>/dev/null 2>/dev/null" ) ) 
		fs .read = true ;
	
	if ( ! system( "which hformat 1>/dev/null 2>/dev/null" ) ) 
		fs .create = true ;
	
	if ( ! system( "which dd 1>/dev/null 2>/dev/null" ) ) 
		fs .copy = true ;
	
	fs .MAX = 2048 ;
	
	return fs ;
}

void hfs::Set_Used_Sectors( Partition & partition ) 
{
	char c_buf[ 512 ] ;
	FILE *f ;
	
	Glib::ustring output ;
	
        //get free bytes..
	f = popen( ( "echo 'quit' | LC_ALL=C hfs " + partition .partition ) .c_str( ), "r" ) ;
	while ( fgets( c_buf, 512, f ) )
	{
		output = Glib::locale_to_utf8( c_buf ) ;
		
		if ( output .find( "bytes free" ) < output .length( ) )
		{
			output = output .substr( 0, output .find( " bytes free" ) ) ;
			
			partition .Set_Unused( atol( (output .substr( output .find_last_of( ' ' ) ) ) .c_str( ) ) / 512 ) ;
			break ;
		}
	}
	pclose( f ) ;
}

bool hfs::Create( const Partition & new_partition )
{
	return ! Execute_Command( "hformat " + new_partition .partition ) ;
}

bool hfs::Resize( const Partition & partition_new, bool fill_partition )
{
	return true ;
}

bool hfs::Copy( const Glib::ustring & src_part_path, const Glib::ustring & dest_part_path )
{
	return ! Execute_Command( "LC_NUMERIC=C dd bs=8192 if=" + src_part_path + " of=" + dest_part_path ) ;
}

bool hfs::Check_Repair( const Partition & partition )
{
	return true ;
}

int hfs::get_estimated_time( long MB_to_Consider )
{
	return -1 ;
}
	

} //GParted


