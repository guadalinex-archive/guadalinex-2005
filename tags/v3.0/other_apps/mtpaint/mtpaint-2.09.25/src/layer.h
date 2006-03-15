/*	layer.h
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

#define MAX_LAYERS 25
#define LAYERS_HEADER "# mtPaint layers"
#define LAYERS_VERSION 1


///	GLOBALS

typedef struct
{
	char mem_filename[256];
	unsigned char *mem_image;
	int mem_image_bpp, mem_changed, mem_width, mem_height, mem_ics;
	float mem_icx, mem_icy;

	undo_item mem_undo_im[MAX_UNDO];
	int mem_undo_pointer, mem_undo_done, mem_undo_redo;

	png_color mem_pal[256];
	int mem_cols, tool_pat, mem_prot_RGB[256], mem_col_A, mem_col_B;
	png_color mem_col_A24, mem_col_B24;

	int mem_xpm_trans, mem_xbm_hot_x, mem_xbm_hot_y;

	char mem_prot_mask[256];
	int mem_prot;
} layer_image;				// All as per memory.h definitions

typedef struct
{
	char name[35];			// Layer text name
	int x, y, trans, opacity;	// Position of layer, transparency colour, opacity %
	gboolean visible, use_trans;	// Show layer, use transparency
	layer_image *image;		// Pointer to image data - malloc'd when created, free'd after
} layer_node;


GtkWidget *layers_window;

layer_node layer_table[MAX_LAYERS+1];	// Table of layer info

int	layers_total,			// Layers currently in use
	layer_selected,			// Layer currently selected in the layers window
	layers_changed,			// 0=Unchanged
	layer_w, layer_h;		// Layer geometry for saving

char	layers_filename[256];		// Current filename for layers file
unsigned char *layer_rgb;		// Memory containing composite layers image
gboolean show_layers_main;		// Show all layers in main window



///	PROCEDURES

void layers_init();
void pressed_layers( GtkMenuItem *menu_item, gpointer user_data );
void pressed_paste_layer( GtkMenuItem *menu_item, gpointer user_data );
gint delete_layers_window( GtkWidget *widget, GdkEvent *event, gpointer data );

int load_layers( char *file_name );
int save_layers( char *file_name );
gboolean layers_check_header( char *file_name );

void layers_notify_changed();
void layers_remove_all();
int check_layers_for_changes();
void move_layer_relative(int l, int change_x, int change_y);	// Move a layer & update window labels
void layer_new( int w, int h, int type, int cols );	// Types 1=indexed, 2=grey, 3=RGB
void layer_choose( int l );				// Select a new layer from the list

void layer_iconbar_click(GtkWidget *widget, gpointer data);
