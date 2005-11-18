/*	prefs.h
	Copyright (C) 2005 Mark Tyler

	This file is part of mtPaint.

	mtPaint is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	mtPaint is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with mtPaint in the file COPYING.
*/

#include <gtk/gtk.h>

gboolean tablet_working, tablet_tool_use[3];	// Size, flow, opacity
float tablet_tool_factor[3];			// Size, flow, opacity

void pressed_preferences( GtkMenuItem *menu_item, gpointer user_data );
void init_tablet();				// Set up variables
