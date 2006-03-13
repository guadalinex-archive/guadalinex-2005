/*	canvas.h
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

GdkWindow *the_canvas;		// Pointer to the canvas we will be drawing on

float can_zoom;			// Zoom factor 1..MAX_ZOOM
int zoom_flag;
int perim_status, perim_x, perim_y, perim_s;		// Tool perimeter
int marq_status, marq_x1, marq_y1, marq_x2, marq_y2;	// Selection marquee
int marq_drag_x, marq_drag_y;				// Marquee dragging offset
int line_status, line_x1, line_y1, line_x2, line_y2;	// Line tool
int poly_status;					// Polygon selection tool
int clone_x, clone_y;					// Clone offsets
int recent_files;					// Current recent files setting

#define MAX_RECENT 20

gboolean col_reverse;					// Painting with right button

gboolean show_paste;					// Show contents of clipboard while pasting
#define STATUS_ITEMS 10
gboolean status_on[STATUS_ITEMS];			// Show status bar items?
gboolean text_paste;					// Are we pasting text?

#define LINE_NONE 0
#define LINE_START 1
#define LINE_LINE 2

#define MARQUEE_NONE 0
#define MARQUEE_SELECTING 1
#define MARQUEE_DONE 2
#define MARQUEE_PASTE 3
#define MARQUEE_PASTE_DRAG 4

#define POLY_NONE 0
#define POLY_SELECTING 1
#define POLY_DRAGGING 2
#define POLY_DONE 3

#define MIN_ZOOM 0.1
#define MAX_ZOOM 20

#define FS_PNG_LOAD 1		// File selector codes
#define FS_PNG_SAVE 2
#define FS_PALETTE_LOAD 3
#define FS_PALETTE_SAVE 4
#define FS_CLIP_FILE 5
#define FS_EXPORT_UNDO 6
#define FS_EXPORT_UNDO2 7
#define FS_EXPORT_ASCII 8
#define FS_LAYER_SAVE 9

int do_a_load( char *fname );
void align_size( float new_zoom );
int alert_box( char *title, char *message, char *text1, char *text2, char *text3 );
void file_selector( int action_type );
void init_pal();			// Initialise palette after loading/palette changes
void update_cols();
void set_new_filename( char *fname );

void main_undo( GtkMenuItem *menu_item, gpointer user_data );
void main_redo( GtkMenuItem *menu_item, gpointer user_data );

void choose_pattern();					// Bring up pattern chooser
void tool_action( int x, int y, int button, gdouble pressure );		// Paint some pixels!
void update_menus();					// Update undo/edit menu

int close_to( int x1, int y1 );

void paint_marquee(int action, int new_x, int new_y);	// Draw/clear marquee
void paint_poly_marquee();				// Paint polygon marquee
void stretch_poly_line(int x, int y);			// Clear old temp line, draw next temp line

void update_sel_bar();			// Update selection stats on status bar
void init_status_bar();			// Initialize status bar

void pressed_lasso( GtkMenuItem *menu_item, gpointer user_data );
void pressed_lasso_cut( GtkMenuItem *menu_item, gpointer user_data );

void pressed_copy( GtkMenuItem *menu_item, gpointer user_data );
void pressed_cut( GtkMenuItem *menu_item, gpointer user_data );
void pressed_paste( GtkMenuItem *menu_item, gpointer user_data );
void pressed_paste_centre( GtkMenuItem *menu_item, gpointer user_data );
void pressed_greyscale( GtkMenuItem *menu_item, gpointer user_data );
void pressed_convert_rgb( GtkMenuItem *menu_item, gpointer user_data );
void pressed_invert( GtkMenuItem *menu_item, gpointer user_data );
void pressed_outline_rectangle( GtkMenuItem *menu_item, gpointer user_data );
void pressed_fill_rectangle( GtkMenuItem *menu_item, gpointer user_data );
void pressed_outline_ellipse( GtkMenuItem *menu_item, gpointer user_data );
void pressed_fill_ellipse( GtkMenuItem *menu_item, gpointer user_data );

void pressed_edge_detect( GtkMenuItem *menu_item, gpointer user_data );
void pressed_sharpen( GtkMenuItem *menu_item, gpointer user_data );
void pressed_soften( GtkMenuItem *menu_item, gpointer user_data );
void pressed_blur( GtkMenuItem *menu_item, gpointer user_data );
void pressed_emboss( GtkMenuItem *menu_item, gpointer user_data );

void pressed_clip_mask( GtkMenuItem *menu_item, gpointer user_data );
void pressed_clip_unmask( GtkMenuItem *menu_item, gpointer user_data );
void pressed_clip_mask_all( GtkMenuItem *menu_item, gpointer user_data );
void pressed_clip_mask_clear( GtkMenuItem *menu_item, gpointer user_data );

void pressed_flip_image_v( GtkMenuItem *menu_item, gpointer user_data );
void pressed_flip_image_h( GtkMenuItem *menu_item, gpointer user_data );
void pressed_flip_sel_v( GtkMenuItem *menu_item, gpointer user_data );
void pressed_flip_sel_h( GtkMenuItem *menu_item, gpointer user_data );

void pressed_rotate_image_clock( GtkMenuItem *menu_item, gpointer user_data );
void pressed_rotate_image_anti( GtkMenuItem *menu_item, gpointer user_data );
void pressed_rotate_sel_clock( GtkMenuItem *menu_item, gpointer user_data );
void pressed_rotate_sel_anti( GtkMenuItem *menu_item, gpointer user_data );
void pressed_rotate_free( GtkMenuItem *menu_item, gpointer user_data );

void pressed_create_dl1( GtkMenuItem *menu_item, gpointer user_data );
void pressed_create_dl3( GtkMenuItem *menu_item, gpointer user_data );
void pressed_create_wu( GtkMenuItem *menu_item, gpointer user_data );

void iso_trans( GtkMenuItem *menu_item, gpointer user_data );

void update_paste_chunk( int x1, int y1, int x2, int y2 );
void check_marquee();
void paste_prepare();
void commit_paste(gboolean undo);
void canvas_undo_chores();

void repaint_line(int mode);			// Repaint or clear line on canvas
void register_file( char *filename );		// Called after successful load/save
void update_recent_files();			// Update the menu items

void scroll_wheel( int x, int y, int d );	// Scroll wheel action from mouse

void update_all_views();			// Update whole canvas on all views

#if GTK_MAJOR_VERSION == 2
void cleanse_txt( char *out, char *in );	// Cleans up non ASCII chars for GTK+2
#endif
