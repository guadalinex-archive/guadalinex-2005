/*	otherwindow.h
	Copyright (C) 2004, 2005 Mark Tyler

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
#include <png.h>

GtkWidget *clipboard_entry;
png_color brcosa_pal[256];

void pressed_new( GtkMenuItem *menu_item, gpointer user_data );
void generic_new_window(int type);

void pressed_add_cols( GtkMenuItem *menu_item, gpointer user_data );
void pressed_brcosa( GtkMenuItem *menu_item, gpointer user_data );
void pressed_create_pscale( GtkMenuItem *menu_item, gpointer user_data );
void pressed_bacteria( GtkMenuItem *menu_item, gpointer user_data );
void pressed_scale( GtkMenuItem *menu_item, gpointer user_data );
void pressed_size( GtkMenuItem *menu_item, gpointer user_data );
void pressed_allcol( GtkMenuItem *menu_item, gpointer user_data );

void pressed_sort_pal( GtkMenuItem *menu_item, gpointer user_data );
void pressed_quantize( GtkMenuItem *menu_item, gpointer user_data );

void choose_pattern(int typ);				// Bring up pattern chooser
void choose_colours();					// Bring up A/B colour editor

int do_new_one( int nw, int nh, int nc, int nt, int bpp );
void do_new_chores();

void bac_form( int type );
void memory_errors(int type);
