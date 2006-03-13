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
 
 /* READ THIS!!
  * Partition isn't really a partition. It's more like a geometry, a continuous part of the disk. 
  * I use it to represent partitions as well as unallocated spaces
  */
 
#ifndef PARTITION
#define PARTITION

#include "../include/Utils.h"
#include "../include/i18n.h"

#include <gdkmm/colormap.h>

#include <sstream>
#include <iostream>

namespace GParted
{
	
enum PartitionType {
	PRIMARY		=	0,
	LOGICAL		=	1,
	EXTENDED	=	2,
	UNALLOCATED	=	3
};

enum PartitionStatus {
	STAT_REAL	=	1,
	STAT_NEW	=	2,
	STAT_COPY	=	3
};

	
class Partition
{
public:
	Partition( ) ;
	~Partition( ) ;

	void Reset( ) ;
	
	//simple Set-functions.  only for convenience, since most members are public
	void Set( 	const Glib::ustring & partition,
			int partition_number,
			PartitionType type,
			const Glib::ustring & filesystem,
			const Sector & sector_start,
			const Sector & sector_end,
			bool inside_extended,
			bool busy ) ;

	void Set_Unused( Sector sectors_unused ) ;

	void Set_Unallocated( Sector sector_start, Sector sector_end, bool inside_extended );

	//update partition number (used when a logical partition is deleted) 
	void Update_Number( int new_number );
	
	long Get_Length_MB( ) const ;
	long Get_Used_MB( ) const ;
	long Get_Unused_MB( ) const ;

	bool Is_Swap( ) const;
		
	//some public members
	Glib::ustring partition;//the symbolic path (e.g. /dev/hda1 )
	int partition_number;
	PartitionType type;// UNALLOCATED, PRIMARY, LOGICAL, etc...
	PartitionStatus status; //STAT_REAL, STAT_NEW, etc..
	Glib::ustring filesystem;// ext2, ext3, ntfs, etc....
	Sector sector_start;
	Sector sector_end;
	Sector sectors_used;
	Sector sectors_unused;
	Gdk::Color color;
	bool inside_extended;//used to check wether partition resides inside extended partition or not.
	bool busy;
	Glib::ustring error;
	Glib::ustring flags;

	std::vector<Partition> logicals ;
	
private:
	

};

}//GParted
#endif //PARTITION
