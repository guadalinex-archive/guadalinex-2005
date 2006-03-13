/*	mainwindow.c
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <math.h>

#include "global.h"

#include "mainwindow.h"
#include "viewer.h"
#include "mygtk.h"
#include "otherwindow.h"
#include "inifile.h"
#include "png.h"
#include "canvas.h"
#include "memory.h"
#include "polygon.h"
#include "layer.h"
#include "info.h"
#include "prefs.h"

#include "graphics/icon.xpm"

#include "graphics/xpm_paint.xpm"
#include "graphics/xpm_brush.xpm"

#include "graphics/xbm_square.xbm"
#include "graphics/xbm_square_mask.xbm"

#include "graphics/xbm_circle.xbm"
#include "graphics/xbm_circle_mask.xbm"

#include "graphics/xbm_horizontal.xbm"
#include "graphics/xbm_horizontal_mask.xbm"

#include "graphics/xbm_vertical.xbm"
#include "graphics/xbm_vertical_mask.xbm"

#include "graphics/xbm_slash.xbm"
#include "graphics/xbm_slash_mask.xbm"

#include "graphics/xbm_backslash.xbm"
#include "graphics/xbm_backslash_mask.xbm"

#include "graphics/xbm_spray.xbm"
#include "graphics/xbm_spray_mask.xbm"

#include "graphics/xpm_shuffle.xpm"
#include "graphics/xbm_shuffle.xbm"
#include "graphics/xbm_shuffle_mask.xbm"

#include "graphics/xpm_flood.xpm"
#include "graphics/xbm_flood.xbm"
#include "graphics/xbm_flood_mask.xbm"

#include "graphics/xpm_line.xpm"
#include "graphics/xbm_line.xbm"
#include "graphics/xbm_line_mask.xbm"

#include "graphics/xpm_select.xpm"
#include "graphics/xbm_select.xbm"
#include "graphics/xbm_select_mask.xbm"

#include "graphics/xpm_smudge.xpm"
#include "graphics/xbm_smudge.xbm"
#include "graphics/xbm_smudge_mask.xbm"

#include "graphics/xpm_polygon.xpm"
#include "graphics/xbm_polygon.xbm"
#include "graphics/xbm_polygon_mask.xbm"

#include "graphics/xpm_clone.xpm"
#include "graphics/xbm_clone.xbm"
#include "graphics/xbm_clone_mask.xbm"

#include "graphics/xbm_move.xbm"
#include "graphics/xbm_move_mask.xbm"

#include "graphics/xpm_brcosa.xpm"
#include "graphics/xpm_crop.xpm"
#include "graphics/xpm_flip_vs.xpm"
#include "graphics/xpm_flip_hs.xpm"
#include "graphics/xpm_rotate_cs.xpm"
#include "graphics/xpm_rotate_as.xpm"
#include "graphics/xpm_info.xpm"
#include "graphics/xpm_text.xpm"
#include "graphics/xpm_lasso.xpm"

#include "graphics/xpm_ellipse.xpm"
#include "graphics/xpm_ellipse2.xpm"
#include "graphics/xpm_rect1.xpm"
#include "graphics/xpm_rect2.xpm"
#include "graphics/xpm_cols.xpm"
#include "graphics/xpm_pan.xpm"

#include "graphics/xpm_new.xpm"
#include "graphics/xpm_open.xpm"
#include "graphics/xpm_save.xpm"
#include "graphics/xpm_cut.xpm"
#include "graphics/xpm_copy.xpm"
#include "graphics/xpm_paste.xpm"
#include "graphics/xpm_undo.xpm"
#include "graphics/xpm_redo.xpm"

#include "graphics/xpm_up.xpm"
#include "graphics/xpm_down.xpm"
#include "graphics/xpm_saveas.xpm"
#include "graphics/xpm_dustbin.xpm"
#include "graphics/xpm_centre.xpm"
#include "graphics/xpm_close.xpm"


GtkWidget
		*menu_undo[5], *menu_redo[5], *menu_crop[5],
		*menu_need_marquee[10], *menu_need_selection[20], *menu_need_clipboard[30],
		*menu_help[2], *menu_continuous[5], *menu_only_24[20], *menu_only_indexed[10],
		*menu_recent[23], *menu_clip_load[15], *menu_clip_save[15], *menu_opac[15],
		*menu_cline[2], *menu_view[2], *menu_iso[5], *menu_layer[2], *menu_lasso[15],
		*menu_prefs[2]
		;

GtkWidget *main_hidden[4];
gboolean view_image_only = FALSE, viewer_mode = FALSE, drag_index = FALSE;
int files_passed, file_arg_start = -1, drag_index_vals[2], cursor_corner;
char **global_argv;

GdkCursor *m_cursor[32];		// My mouse cursors
GdkCursor *move_cursor;

GtkWidget *main_window;
GdkPixmap *icon_pix = NULL;
GtkWidget *drawing_palette, *drawing_pat_prev, *drawing_col_prev, *drawing_canvas;
GtkWidget *scrolledwindow_canvas, *scrolledwindow_palette;
GtkWidget *spinbutton_spray, *spinbutton_size;
GtkWidget *label_A, *label_B, *label_bar1, *label_bar2, *label_bar3,
	*label_bar5, *label_bar4, *label_bar6 = NULL, *label_bar7 = NULL, *label_bar8;
GtkWidget *viewport_palette;
GtkWidget *menubar1;

GdkGC *dash_gc;

gboolean q_quit;			// Does q key quit the program?

void clear_perim_real();

void men_item_state( GtkWidget *menu_items[], gboolean state )
{	// Enable or disable menu items
	int i = 0;
	while ( menu_items[i] != NULL )
	{
		gtk_widget_set_sensitive( menu_items[i], state );
		i++;
	}
}

void pop_men_dis( GtkItemFactory *item_factory, char *items[], GtkWidget *menu_items[] )
{	// Populate disable menu item array
	int i = 0;
	while ( items[i] != NULL )
	{
		menu_items[i] = gtk_item_factory_get_item(item_factory, items[i]);
		i++;
	}
	menu_items[i] = NULL;
}

void men_dis_add( GtkWidget *widget, GtkWidget *menu_items[] )		// Add widget to disable list
{
	int i = 0;

	while ( menu_items[i] != NULL ) i++;
	menu_items[i] = widget;
	menu_items[i+1] = NULL;
}

void pressed_swap_AB( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_swap_cols();
	repaint_top_swatch();
	init_pal();
	gtk_widget_queue_draw( drawing_col_prev );
}

void pressed_load_recent( GtkMenuItem *menu_item, gpointer user_data )
{
	int i=1, change;
	char txt[64], *c, old_file[256];

	while ( i<=MAX_RECENT )
	{
		if ( GTK_WIDGET(menu_item) == menu_recent[i] )
		{
			sprintf( txt, "file%i", i );
			c = inifile_get( txt, "." );
			strncpy( old_file, c, 250 );

			if ( layers_total==0 )
				change = check_for_changes();
			else
				change = check_layers_for_changes();

			if ( change == 2 || change == -10 )
				do_a_load(old_file);				// Load requested file
			return;
		}
		else i++;
	}
}

void pressed_opacity_mode( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_undo_opacity = GTK_CHECK_MENU_ITEM(menu_item)->active;
	inifile_set_gboolean( "opacityToggle", mem_undo_opacity );

	if ( label_bar9 != NULL ) init_status_bar();
}

void pressed_continuous( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_continuous = GTK_CHECK_MENU_ITEM(menu_item)->active;
	inifile_set_gboolean( "continuousPainting", mem_continuous );

	if ( label_bar6 != NULL ) init_status_bar();
}

void pressed_crop( GtkMenuItem *menu_item, gpointer user_data )
{
	int res, x1, y1, x2, y2;

	mtMIN( x1, marq_x1, marq_x2 )
	mtMIN( y1, marq_y1, marq_y2 )
	mtMAX( x2, marq_x1, marq_x2 )
	mtMAX( y2, marq_y1, marq_y2 )

	if ( marq_status != MARQUEE_DONE ) return;
	if ( x1==0 && x2>=(mem_width-1) && y1==0 && y2>=(mem_height-1) ) return;

	res = mem_undo_next2( x2-x1+1, y2-y1+1, x1, y1 );
	pen_down = 0;

	if ( res == 0 )
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON]), TRUE );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
		canvas_undo_chores();
	}
	else memory_errors(3-res);
}

void pressed_select_none( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( marq_status != MARQUEE_NONE )
	{
		paint_marquee(0, marq_x1, marq_y1);
		marq_status = MARQUEE_NONE;
		update_menus();
		gtk_widget_queue_draw( drawing_canvas );
		set_cursor();
		update_sel_bar();
	}

	if ( tool_type == TOOL_POLYGON )
	{
		if ( poly_status != POLY_NONE )
		{
			poly_points = 0;
			poly_status = POLY_NONE;
			update_menus();
			gtk_widget_queue_draw( drawing_canvas );
			set_cursor();
			update_sel_bar();
		}
	}
}

void pressed_select_all( GtkMenuItem *menu_item, gpointer user_data )
{
	int i = 0;

	paste_prepare();
	if ( tool_type != TOOL_SELECT )
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE);
		i = 1;
	}

	if ( marq_status >= MARQUEE_PASTE ) i = 1;

	marq_status = MARQUEE_DONE;
	marq_x1 = 0;
	marq_y1 = 0;
	marq_x2 = mem_width - 1;
	marq_y2 = mem_height - 1;

	update_menus();
	paint_marquee(1, marq_x1-mem_width, marq_y1-mem_height);
	update_sel_bar();
	if ( i == 1 ) gtk_widget_queue_draw( drawing_canvas );		// Clear old past stuff
}

void pressed_remove_unused( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	i = mem_remove_unused_check();
	if ( i <= 0 )
		alert_box( _("Error"), _("There were no unused colours to remove!"),
			_("OK"), NULL, NULL);
	if ( i > 0 )
	{
		spot_undo();

		mem_remove_unused();

		if ( mem_col_A >= mem_cols ) mem_col_A = 0;
		if ( mem_col_B >= mem_cols ) mem_col_B = 0;
		init_pal();
		gtk_widget_queue_draw( drawing_canvas );
		gtk_widget_queue_draw(drawing_col_prev);
	}
}

void pressed_default_pal( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	mem_pal_copy( mem_pal, mem_pal_def );
	mem_cols = 256;
	init_pal();
	update_all_views();
	gtk_widget_queue_draw(drawing_col_prev);
}

void pressed_remove_duplicates( GtkMenuItem *menu_item, gpointer user_data )
{
	int dups;
	char mess[256];

	if ( mem_cols < 3 )
	{
		alert_box( _("Error"), _("The palette does not contain enough colours to do a merge"),
			_("OK"), NULL, NULL );
	}
	else
	{
		dups = scan_duplicates();

		if ( dups == 0 )
		{
			alert_box( _("Error"), _("The palette does not contain 2 colours that have identical RGB values"), _("OK"), NULL, NULL );
			return;
		}
		else
		{
			if ( dups == (mem_cols - 1) )
			{
				alert_box( _("Error"), _("There are too many identical palette items to be reduced."), _("OK"), NULL, NULL );
				return;
			}
			else
			{
				snprintf(mess, 250, _("The palette contains %i colours that have identical RGB values.  Do you really want to merge them into one index and realign the canvas?"), dups );
				if ( alert_box( _("Warning"), mess, _("Yes"), _("No"), NULL ) == 1 )
				{
					spot_undo();

					remove_duplicates();
					init_pal();
					gtk_widget_queue_draw( drawing_canvas );
					gtk_widget_queue_draw(drawing_col_prev);
				}
			}
		}
	}
}

void pressed_create_patterns( GtkMenuItem *menu_item, gpointer user_data )
{	// Create a pattern.c file from the current image

	int row, column, pattern, sx, sy, pixel;
	FILE *fp;

//printf("w = %i h = %i c = %i\n\n", mem_width, mem_height, mem_cols );

	if ( mem_width == 94 && mem_height == 94 && mem_cols == 3 )
	{
		fp = fopen("pattern_user.c", "w");
		if ( fp == NULL ) alert_box( _("Error"),
				_("patterns_user.c could not be opened in current directory"),
				_("OK"), NULL, NULL );
		else
		{
			fprintf( fp, "char mem_patterns[81][8][8] = \n{\n" );
			pattern = 0;
			while ( pattern < 81 )
			{
				fprintf( fp, "{ " );
				sy = 2 + (pattern / 9) * 10;		// Start y pixel on main image
				sx = 3 + (pattern % 9) * 10;		// Start x pixel on main image
				for ( column = 0; column < 8; column++ )
				{
					fprintf( fp, "{" );
					for ( row = 0; row < 8; row++ )
					{
						pixel = mem_image[ sx+row + 94*(sy+column) ];
						fprintf( fp, "%i", pixel % 2 );
						if ( row < 10 ) fprintf( fp, "," );
					}
					if ( column < 10 ) fprintf( fp, "},\n" );
					else  fprintf( fp, "}\n" );
				}
				if ( pattern < 80 ) fprintf( fp, "},\n" );
				else  fprintf( fp, "}\n" );
				pattern++;
			}
			fprintf( fp, "};\n" );
			alert_box( _("Done"), _("patterns_user.c created in current directory"),
				_("OK"), NULL, NULL );
			fclose( fp );
		}
	}
	else
	{
		alert_box( _("Error"), _("Current image is not 94x94x3 so I cannot create patterns_user.c"), _("OK"), NULL, NULL );
	}
}

void pressed_mask_all( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_mask_setall( 1 );
	mem_pal_init();
	gtk_widget_queue_draw( drawing_palette );
}

void pressed_mask_none( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_mask_setall( 0 );
	mem_pal_init();
	gtk_widget_queue_draw( drawing_palette );
}

void pressed_export_undo( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( mem_undo_done>0 ) file_selector( FS_EXPORT_UNDO );
}

void pressed_export_undo2( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( mem_undo_done>0 ) file_selector( FS_EXPORT_UNDO2 );
}

void pressed_export_ascii( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( mem_cols <= 16 ) file_selector( FS_EXPORT_ASCII );
	else alert_box( _("Error"), _("You must have 16 or fewer palette colours to export ASCII art."),
		_("OK"), NULL, NULL );
}

void pressed_open_pal( GtkMenuItem *menu_item, gpointer user_data )
{	file_selector( FS_PALETTE_LOAD ); }

void pressed_save_pal( GtkMenuItem *menu_item, gpointer user_data )
{	file_selector( FS_PALETTE_SAVE ); }

void pressed_open_file( GtkMenuItem *menu_item, gpointer user_data )
{	file_selector( FS_PNG_LOAD ); }

void pressed_save_file_as( GtkMenuItem *menu_item, gpointer user_data )
{	file_selector( FS_PNG_SAVE ); }

int gui_save( char *filename )
{
	int res;
	char mess[512];

	res = save_image( filename );
	if ( res < 0 )
	{
		if ( res == NOT_XPM )
		{
			alert_box( _("Error"), _("You are trying to save an RGB image to an XPM file which is not possible.  I would suggest you save with a PNG extension."), _("OK"), NULL, NULL );
		}
		if ( res == NOT_GIF )
		{
			alert_box( _("Error"), _("You are trying to save an RGB image to a GIF file which is not possible.  I would suggest you save with a PNG extension."), _("OK"), NULL, NULL );
		}
		if ( res == NOT_XBM )
		{
			alert_box( _("Error"), _("You are trying to save an XBM file with a palette of more than 2 colours.  Either use another format or reduce the palette to 2 colours."), _("OK"), NULL, NULL );
		}
		if ( res == NOT_JPEG )
		{
			alert_box( _("Error"), _("You are trying to save an indexed canvas to a JPEG file which is not possible.  I would suggest you save with a PNG extension."), _("OK"), NULL, NULL );
		}
		if ( res == NOT_TIFF )
		{
			alert_box( _("Error"), _("You are trying to save an indexed canvas to a TIFF file which is not possible.  I would suggest you save with a PNG extension."), _("OK"), NULL, NULL );
		}
		if ( res == -1 )
		{
			snprintf(mess, 500, _("Unable to save file: %s"), filename);
			alert_box( _("Error"), mess, _("OK"), NULL, NULL );
		}
	}
	else
	{
		notify_unchanged();
		register_file( filename );
	}

	return res;
}

void pressed_save_file( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( strcmp( mem_filename, _("Untitled") ) ) gui_save(mem_filename);
	else pressed_save_file_as( menu_item, user_data );
}

void load_clipboard( int item )
{
	int i;

	snprintf( mem_clip_file[1], 251, "%s%i", mem_clip_file[0], item );
	i = load_png( mem_clip_file[1], 1 );

	if ( i!=1 ) alert_box( _("Error"), _("Unable to load clipboard"), _("OK"), NULL, NULL );
	else text_paste = FALSE;

	update_menus();

	if ( tool_type == TOOL_SELECT && marq_status >= MARQUEE_PASTE )
		pressed_select_none( NULL, NULL );

	if ( tool_type == TOOL_POLYGON && poly_status >= POLY_NONE )
		pressed_select_none( NULL, NULL );

	if ( mem_image_bpp == mem_clip_bpp ) pressed_paste_centre( NULL, NULL );
}

void load_clip( GtkMenuItem *menu_item, gpointer user_data )
{
	int i, j=0;

	for ( i=0; i<12; i++ )
		if ( menu_clip_load[i] == GTK_WIDGET(menu_item) ) j=i;

	load_clipboard(j+1);
}


void save_clipboard( int item )
{
	int i;

	snprintf( mem_clip_file[1], 251, "%s%i", mem_clip_file[0], item );
	i = save_png( mem_clip_file[1], 1 );

	if ( i!=0 ) alert_box( _("Error"), _("Unable to save clipboard"), _("OK"), NULL, NULL );
}

void save_clip( GtkMenuItem *menu_item, gpointer user_data )
{
	int i, j=0;

	for ( i=0; i<12; i++ )
		if ( menu_clip_save[i] == GTK_WIDGET(menu_item) ) j=i;

	save_clipboard(j+1);
}

void pressed_opacity( GtkMenuItem *menu_item, gpointer user_data )
{
	int i, j=0;

	if ( mem_image_bpp == 3 )
	{
		for ( i=0; i<12; i++ )
			if ( menu_opac[i] == GTK_WIDGET(menu_item) ) j=i;

		if ( j<10 ) tool_opacity = (j+1)*10;
		else
		{
			j = 2*(j-10)-1;
			tool_opacity += j;
			mtMIN( tool_opacity, tool_opacity, 100 )
			mtMAX( tool_opacity, tool_opacity, 1 )
		}
		if ( marq_status >= MARQUEE_PASTE )
			gtk_widget_queue_draw(drawing_canvas);
				// Update the paste on the canvas as we have changed the opacity value
	}
	else tool_opacity = 100;

	if ( status_on[8] ) init_status_bar();
}

void toggle_view( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	view_image_only = !view_image_only;

	for ( i=0; i<4; i++ ) if (view_image_only) gtk_widget_hide(main_hidden[i]);
				else gtk_widget_show(main_hidden[i]);
}

void zoom_in( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	if (can_zoom>=1) align_size( can_zoom + 1 );
	else
	{
		i = mt_round(1/can_zoom);
		align_size( 1.0/(((float) i) - 1) );
	}
}

void zoom_out( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	if (can_zoom>1) align_size( can_zoom - 1 );
	else
	{
		i = mt_round(1/can_zoom);
		if (i>9) i=9;
		align_size( 1.0/(((float) i) + 1) );
	}
}

void zoom_grid( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_show_grid = GTK_CHECK_MENU_ITEM(menu_item)->active;
	inifile_set_gboolean( "gridToggle", mem_show_grid );

	if ( drawing_canvas ) gtk_widget_queue_draw( drawing_canvas );
}

void zoom_to1( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 1 );	}

void zoom_to4( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 4 );	}

void zoom_to8( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 8 );	}

void zoom_to12( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 12 );	}

void zoom_to16( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 16 );	}

void zoom_to20( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 20 );	}

void zoom_to10( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 0.10 );	}

void zoom_to25( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 0.25 );	}

void zoom_to50( GtkMenuItem *menu_item, gpointer user_data )
{	align_size( 0.50 );	}

void quit_all( GtkMenuItem *menu_item, gpointer user_data )
{
	delete_event( NULL, NULL, NULL );
}

int move_arrows( int *c1, int *c2, int value )
{
	int ox1 = marq_x1, oy1 = marq_y1, ox2 = marq_x2, oy2 = marq_y2;
	int nx1, ny1, nx2, ny2;

	*c1 = *c1 + value;
	*c2 = *c2 + value;

	nx1 = marq_x1;
	ny1 = marq_y1;
	nx2 = marq_x2;
	ny2 = marq_y2;

	marq_x1 = ox1;
	marq_y1 = oy1;
	marq_x2 = ox2;
	marq_y2 = oy2;

	paint_marquee(0, nx1, ny1);
	*c1 = *c1 + value;
	*c2 = *c2 + value;
	paint_marquee(1, ox1, oy1);

	return 1;
}

int check_arrows( GdkEventKey *event, int change )
{
	int res = 0;

	if ( event->keyval == GDK_Left || event->keyval == GDK_KP_Left )
		res = move_arrows( &marq_x1, &marq_x2, -change );

	if ( event->keyval == GDK_Right || event->keyval == GDK_KP_Right )
		res = move_arrows( &marq_x1, &marq_x2, change );

	if ( event->keyval == GDK_Down || event->keyval == GDK_KP_Down )
		res = move_arrows( &marq_y1, &marq_y2, change );

	if ( event->keyval == GDK_Up || event->keyval == GDK_KP_Up )
		res = move_arrows( &marq_y1, &marq_y2, -change );

	return res;
}

void stop_line()
{
	if ( line_status != LINE_NONE ) repaint_line(0);
	line_status = LINE_NONE;
}

void change_to_tool(int icon)
{
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON+icon]), TRUE );

	if ( perim_status > 0 ) clear_perim_real();
}

gint check_zoom_keys_real( GdkEventKey *event )
{
	int i=-1;
	float zals[9] = { 0.1, 0.25, 0.5, 1, 4, 8, 12, 16, 20 };

	if ( !(event->state & GDK_SHIFT_MASK) && !(event->state & GDK_CONTROL_MASK) )
	{
		switch (event->keyval)
		{
			case GDK_plus:		zoom_in(NULL, NULL); return TRUE;
			case GDK_KP_Add:	zoom_in(NULL, NULL); return TRUE;
			case GDK_equal:		zoom_in(NULL, NULL); return TRUE;
			case GDK_minus:		zoom_out(NULL, NULL); return TRUE;
			case GDK_KP_Subtract:	zoom_out(NULL, NULL); return TRUE;
		}
	}

	if ( !(event->state & GDK_SHIFT_MASK) && !(event->state & GDK_CONTROL_MASK) )
	{
		if ( event->keyval >= GDK_KP_1 && event->keyval <= GDK_KP_9 )
			i = event->keyval - GDK_KP_1;
		if ( event->keyval >= GDK_1 && event->keyval <= GDK_9 )
			i = event->keyval - GDK_1;

		if ( i>=0 && i<=9 )
		{
			align_size( zals[i] );
			return TRUE;
		}
	}

	if ( event->keyval == GDK_Home )
	{
		toggle_view( NULL, NULL );
		return TRUE;
	}

	return FALSE;
}

gint check_zoom_keys( GdkEventKey *event )
{
	if ( (event->keyval == GDK_q || event->keyval == GDK_Q ) && q_quit )
		quit_all( NULL, NULL );

	if ( check_zoom_keys_real(event) ) return TRUE;

	if ( !(event->state & GDK_SHIFT_MASK) && !(event->state & GDK_CONTROL_MASK) )
	{
		switch (event->keyval)
		{
		case GDK_Insert:	pressed_brcosa(NULL, NULL); return TRUE;
		case GDK_End:		pressed_pan(NULL, NULL); return TRUE;
		case GDK_Delete:	pressed_crop(NULL, NULL); return TRUE;
		}

		if ( !(event->state & GDK_MOD1_MASK) )	// We must avoid catching Alt-V, C etc.
		switch (event->keyval)
		{
		case GDK_x:
		case GDK_X:		pressed_swap_AB(NULL, NULL); return TRUE;
		case GDK_c:
		case GDK_C:		if ( allow_cline ) pressed_cline(NULL, NULL); return TRUE;
		case GDK_v:
		case GDK_V:		if ( allow_view ) pressed_view(NULL, NULL); return TRUE;
#if GTK_MAJOR_VERSION == 2
		case GDK_F2:		pressed_choose_patterns(NULL, NULL); return TRUE;
		case GDK_F3:		pressed_choose_brush(NULL, NULL); return TRUE;
#endif
// GTK+1 creates a segfault if you use F2/F3 here - This doesn't matter as only GTK+2 needs it here as in full screen mode GTK+2 does not handle menu keyboard shortcuts
		case GDK_F4:		change_to_tool(0); return TRUE;
		case GDK_F5:		change_to_tool(1); return TRUE;
		case GDK_F6:		change_to_tool(2); return TRUE;
		case GDK_F7:		change_to_tool(3); return TRUE;
		case GDK_F8:		if ( mem_image_bpp != 1 ) change_to_tool(4); return TRUE;
		case GDK_F9:		change_to_tool(6); return TRUE;
		}
	}

	return FALSE;
}

gint handle_keypress( GtkWidget *widget, GdkEventKey *event )
{
	int i, aco[3][2], minx, maxx, miny, maxy, xw, yh;	// Arrow coords
	float llen, uvx, uvy;					// Line length & unit vector lengths

	if ( check_zoom_keys( event ) ) return TRUE;		// Check HOME/zoom keys

	if ( !(event->state & GDK_CONTROL_MASK) && (event->state & GDK_SHIFT_MASK) )
	{
		if ( marq_status > MARQUEE_NONE )
		{
			if ( check_arrows( event, mem_nudge ) == 1 )
			{
				update_sel_bar();
				update_menus();
				return TRUE;
			}
		}
	}

	if ( (event->state & GDK_CONTROL_MASK) )		// Opacity keys, i.e. CTRL + keypad
	{
		if ( event->keyval >= GDK_KP_0 && event->keyval <= GDK_KP_9 )
		{
			i = event->keyval - GDK_KP_1;
			if ( i<0 ) i=i+10;
			pressed_opacity( GTK_MENU_ITEM(menu_opac[i]), NULL );
		}
		if ( event->keyval == GDK_plus || event->keyval == GDK_equal ||
			event->keyval == GDK_KP_Add )
				pressed_opacity( GTK_MENU_ITEM(menu_opac[11]), NULL );
		if ( event->keyval == GDK_minus || event->keyval == GDK_KP_Subtract )
				pressed_opacity( GTK_MENU_ITEM(menu_opac[10]), NULL );
	}

	if ( !(event->state & GDK_CONTROL_MASK) && !(event->state & GDK_SHIFT_MASK) )
	{
		switch (event->keyval)
		{
			case GDK_Escape:	if ( tool_type == TOOL_SELECT ||
							tool_type == TOOL_POLYGON )
								pressed_select_none( NULL, NULL );
						if ( tool_type == TOOL_LINE ) stop_line();
						return TRUE;
			case GDK_Page_Up:	pressed_scale(NULL, NULL); return TRUE;
			case GDK_Page_Down:	pressed_size(NULL, NULL); return TRUE;
		}
		if ( marq_status > MARQUEE_NONE )
		{
			if ( check_arrows( event, 1 ) == 1 )
			{
				update_sel_bar();
				update_menus();
				return TRUE;
			}
		}
		else
		{
			i = 0;
			if ( event->keyval == GDK_Left || event->keyval == GDK_KP_Left )
			{
				mem_col_B--;
				i=2;
			}
			if ( event->keyval == GDK_Right || event->keyval == GDK_KP_Right )
			{
				mem_col_B++;
				i=2;
			}
			if ( event->keyval == GDK_Down || event->keyval == GDK_KP_Down )
			{
				mem_col_A++;
				i=1;
			}
			if ( event->keyval == GDK_Up || event->keyval == GDK_KP_Up )
			{
				mem_col_A--;
				i=1;
			}

			if ( i>0 )
			{
				mtMIN( mem_col_A, mem_col_A, mem_cols-1 )
				mtMAX( mem_col_A, mem_col_A, 0 )
				mtMIN( mem_col_B, mem_col_B, mem_cols-1 )
				mtMAX( mem_col_B, mem_col_B, 0 )
				if ( i==1 ) mem_col_A24 = mem_pal[mem_col_A];
				if ( i==2 ) mem_col_B24 = mem_pal[mem_col_B];
				init_pal();
				return TRUE;
			}
		}
	}

	if ( marq_status >= MARQUEE_PASTE &&
		( event->keyval==GDK_KP_Enter || event->keyval==GDK_Return ) )
	{
		commit_paste(TRUE);
		pen_down = 0;		// Ensure each press of enter is a new undo level
		return TRUE;
	}

	if ( tool_type == TOOL_LINE && line_status > LINE_NONE )
	{
		if ( event->keyval==GDK_a || event->keyval==GDK_A
			|| event->keyval==GDK_s || event->keyval==GDK_S )
		{
			if ( ! (line_x1 == line_x2 && line_y1 == line_y2) )
			{
					// Calculate 2 coords for arrow corners
				llen = sqrt( (line_x1-line_x2) * (line_x1-line_x2) +
						(line_y1-line_y2) * (line_y1-line_y2) );
				uvx = (line_x2 - line_x1) / llen;
				uvy = (line_y2 - line_y1) / llen;

				aco[0][0] = mt_round(line_x1 + tool_flow * uvx);
				aco[0][1] = mt_round(line_y1 + tool_flow * uvy);
				aco[1][0] = mt_round(aco[0][0] + tool_flow / 2 * -uvy);
				aco[1][1] = mt_round(aco[0][1] + tool_flow / 2 * uvx);
				aco[2][0] = mt_round(aco[0][0] - tool_flow / 2 * -uvy);
				aco[2][1] = mt_round(aco[0][1] - tool_flow / 2 * uvx);

				pen_down = 0;
				tool_action( line_x1, line_y1, 1, 0 );
				line_status = LINE_LINE;
				update_menus();

					// Draw arrow lines & circles
				f_circle( aco[1][0], aco[1][1], tool_size );
				f_circle( aco[2][0], aco[2][1], tool_size );
				tline( aco[1][0], aco[1][1], line_x1, line_y1, tool_size );
				tline( aco[2][0], aco[2][1], line_x1, line_y1, tool_size );

				if ( event->keyval==GDK_s || event->keyval==GDK_S )
				{
					// Draw 3rd line and fill arrowhead
					tline( aco[1][0], aco[1][1], aco[2][0], aco[2][1], tool_size );
					poly_points = 0;
					poly_add( line_x1, line_y1 );
					for ( i=1; i<3; i++ )
						poly_add( aco[i][0], aco[i][1] );
					poly_init();
					poly_paint();
					poly_points = 0;
				}

					// Update screen areas
				mtMIN( minx, aco[1][0]-tool_size/2-1, aco[2][0]-tool_size/2-1 )
				mtMIN( minx, minx, line_x1-tool_size/2-1 )
				mtMAX( maxx, aco[1][0]+tool_size/2+1, aco[2][0]+tool_size/2+1 )
				mtMAX( maxx, maxx, line_x1+tool_size/2+1 )

				mtMIN( miny, aco[1][1]-tool_size/2-1, aco[2][1]-tool_size/2-1 )
				mtMIN( miny, miny, line_y1-tool_size/2-1 )
				mtMAX( maxy, aco[1][1]+tool_size/2+1, aco[2][1]+tool_size/2+1 )
				mtMAX( maxy, maxy, line_y1+tool_size/2+1 )

				xw = maxx - minx + 1;
				yh = maxy - miny + 1;

				gtk_widget_queue_draw_area( drawing_canvas,
					minx*can_zoom, miny*can_zoom,
					mt_round(xw*can_zoom), mt_round(yh*can_zoom) );
				vw_update_area( minx, miny, xw+1, yh+1 );
			}
		}
	}

	return FALSE;
}

gint destroy_signal( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	quit_all( NULL, NULL );

	return FALSE;
}

int check_for_changes()			// 1=STOP, 2=IGNORE, 10=ESCAPE, -10=NOT CHECKED
{
	int i = -10;
	char *warning = _("This canvas/palette contains changes that have not been saved.  Do you really want to lose these changes?");

	if ( mem_changed == 1 )
		i = alert_box( _("Warning"), warning, _("Cancel Operation"), _("Lose Changes"), NULL );

	return i;
}

gint delete_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	gint x,y,width,height;

	int i = 2, j = 0;

	if ( layers_total == 0 )
		j = check_for_changes();
	else
		j = check_layers_for_changes();

	if ( j == -10 )
	{
		if ( inifile_get_gboolean( "exitToggle", FALSE ) )
			i = alert_box( VERSION, _("Do you really want to quit?"), _("NO"), _("YES"), NULL );
	}
	else i = j;

	if ( i==2 )
	{
		gdk_window_get_size( main_window->window, &width, &height );
		gdk_window_get_root_origin( main_window->window, &x, &y );
	
		inifile_set_gint32("window_x", x );
		inifile_set_gint32("window_y", y );
		inifile_set_gint32("window_w", width );
		inifile_set_gint32("window_h", height );

		if (cline_window != NULL) delete_cline( NULL, NULL, NULL );
		if (view_window != NULL) delete_view( NULL, NULL, NULL );
		if (layers_window != NULL) delete_layers_window( NULL, NULL, NULL );
			// Get rid of extra windows + remember positions

		gtk_main_quit ();
		return FALSE;
	}
	else return TRUE;
}

#if GTK_MAJOR_VERSION == 2
gint canvas_scroll_gtk2( GtkWidget *widget, GdkEventScroll *event )
{
	int x = event->x / can_zoom, y = event->y / can_zoom;

	if ( !inifile_get_gboolean( "scrollwheelZOOM", TRUE ) )
		return FALSE;		// Normal GTK+2 scrollwheel behaviour

	if (event->direction == GDK_SCROLL_DOWN)
		scroll_wheel( x, y, -1 );
	else
		scroll_wheel( x, y, 1 );

	return TRUE;
}
#endif

gint canvas_release( GtkWidget *widget, GdkEventButton *event )
{
	pen_down = 0;
	if ( col_reverse )
	{
		col_reverse = FALSE;
		mem_swap_cols();
	}

	if ( tool_type == TOOL_LINE && event->button == 1 && line_status == LINE_START )
	{
		line_status = LINE_LINE;
		repaint_line(1);
	}

	if ( (tool_type == TOOL_SELECT || tool_type == TOOL_POLYGON) && event->button == 1 )
	{
		if ( marq_status == MARQUEE_SELECTING ) marq_status = MARQUEE_DONE;
		if ( marq_status == MARQUEE_PASTE_DRAG ) marq_status = MARQUEE_PASTE;
		cursor_corner = -1;
	}

	if ( tool_type == TOOL_POLYGON && poly_status == POLY_DRAGGING )
		tool_action( 0, 0, 0, 0 );		// Finish off dragged polygon selection

	update_menus();

	return FALSE;
}

void mouse_event( int x, int y, guint state, guint button, gdouble pressure )
{	// Mouse event from button/motion on the canvas
	unsigned char pixel;
	png_color pixel24;

	x = x / can_zoom;
	y = y / can_zoom;

	mtMAX( x, x, 0 )
	mtMAX( y, y, 0 )
	mtMIN( x, x, mem_width - 1)
	mtMIN( y, y, mem_height - 1)

	if ( (state & GDK_CONTROL_MASK) && !(state & GDK_SHIFT_MASK) )		// Set colour A/B
	{
		if ( mem_image_bpp == 1 )
		{
			pixel = GET_PIXEL( x, y );
			if (button == 1 && mem_col_A != pixel)
			{
				mem_col_A = pixel;
				mem_col_A24 = mem_pal[pixel];
				repaint_top_swatch();
				update_cols();
			}
			if (button == 3 && mem_col_B != pixel)
			{
				mem_col_B = pixel;
				mem_col_B24 = mem_pal[pixel];
				repaint_top_swatch();
				update_cols();
			}
		}
		if ( mem_image_bpp == 3 )
		{
			pixel24 = get_pixel24( x, y );
			if (button == 1 && png_cmp( mem_col_A24, pixel24 ) )
			{
				mem_col_A24 = pixel24;
				repaint_top_swatch();
				update_cols();
			}
			if (button == 3 && png_cmp( mem_col_B24, pixel24 ) )
			{
				mem_col_B24 = pixel24;
				repaint_top_swatch();
				update_cols();
			}
		}
	}
	else
	{
		if ( tool_fixy < 0 && (state & GDK_SHIFT_MASK) && (state & GDK_CONTROL_MASK) )
		{
			tool_fixx = -1;
			tool_fixy = y;
		}

		if ( tool_fixx < 0 && (state & GDK_SHIFT_MASK) && !(state & GDK_CONTROL_MASK) )
			tool_fixx = x;

		if ( !(state & GDK_SHIFT_MASK) ) tool_fixx = -1;
		if ( !(state & GDK_CONTROL_MASK) ) tool_fixy = -1;

		if ( button == 3 && (state & GDK_SHIFT_MASK) ) set_zoom_centre( x, y );
		else if ( button == 1 || button >= 3 ) tool_action( x, y, button, pressure );
		if ( tool_type == TOOL_SELECT ) update_sel_bar();
	}
	if ( button == 2 ) set_zoom_centre( x, y );
}

static gint main_scroll_changed()
{
	vw_focus_view();	// The user has adjusted a scrollbar so we may need to change view window

	return TRUE;
}

static gint canvas_button( GtkWidget *widget, GdkEventButton *event )
{
	gdouble pressure = 1.0;

#if GTK_MAJOR_VERSION == 1
	if ( tablet_working )
		pressure = event->pressure;
#endif
#if GTK_MAJOR_VERSION == 2
	if ( tablet_working )
		gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);
#endif

	if ( mem_image != NULL )
		mouse_event( event->x, event->y, event->state, event->button, pressure );

	return TRUE;
}

static gint canvas_enter( GtkWidget *widget, GdkEventMotion *event )	// Mouse enters the canvas
{
	tool_flow = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spinbutton_spray) );
	tool_size = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spinbutton_size) );

	return TRUE;
}

static gint canvas_left( GtkWidget *widget, GdkEventMotion *event )
{
	if ( mem_image != NULL )		// Only do this if we have an image
	{
		if ( status_on[2] ) gtk_label_set_text( GTK_LABEL(label_bar2), "" );
		if ( status_on[3] ) gtk_label_set_text( GTK_LABEL(label_bar3), "" );
		if ( perim_status > 0 )
		{
			perim_status = 0;
			clear_perim();
		}
		if ( tool_type == TOOL_LINE && line_status != LINE_NONE ) repaint_line(0);
		if ( tool_type == TOOL_POLYGON && poly_status == POLY_SELECTING ) repaint_line(0);
	}

	return FALSE;
}

void repaint_paste( int px1, int py1, int px2, int py2 )
{
	unsigned char *rgb, pix, mask, pixel[3];
	int pw = (px2 - px1 + 1), ph = (py2 - py1 + 1);
	int ix, iy, mx, my, offset, i, j, k, ipix, lx=0, ly=0;

	if ( pw<=0 || ph<=0 ) return;
	rgb = grab_memory( pw*ph*3, "paste RGB redraw", mem_background );
	if ( rgb == NULL ) return;

	while ( (px1/can_zoom) < marq_x1 )
	{
		px1++; pw--;		// Its a grimy hack, but it avoids a nasty segfault
	}
	while ( (py1/can_zoom) < marq_y1 )
	{
		py1++; ph--;
	}

	if ( mem_clip_mask != NULL || (tool_opacity<100 && mem_image_bpp==3) )
	{
		if ( layers_total == 0 || !show_layers_main )
			main_render_rgb( rgb, px1, py1, pw, ph, can_zoom );
		else
		{
			if ( layer_selected > 0 )
			{
				lx = can_zoom * layer_table[layer_selected].x;
				ly = can_zoom * layer_table[layer_selected].y;
			}
			view_render_rgb( rgb, px1+lx, py1+ly, pw, ph, can_zoom );
		}
	}

	if ( mem_image_bpp == 1 )
	{
		for ( j=0; j<ph; j++ )
		{
			if ( mem_clip_mask == NULL )
			{
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					pix = mem_clipboard[ mx + my*mem_clip_w ];
					offset = 3*(j*pw + i);
					rgb[offset] = mem_pal[pix].red;
					rgb[offset + 1] = mem_pal[pix].green;
					rgb[offset + 2] = mem_pal[pix].blue;
				}
			}
			else
			{
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					mask = mem_clip_mask[mx + my*mem_clip_w];
					offset = 3*(j*pw + i);
					if ( mask == 0 )
					{
						pix = mem_clipboard[ mx + my*mem_clip_w ];
						rgb[offset] = mem_pal[pix].red;
						rgb[offset + 1] = mem_pal[pix].green;
						rgb[offset + 2] = mem_pal[pix].blue;
					}
				}
			}
		}
	}
	if ( mem_image_bpp == 3 )
	{
		for ( j=0; j<ph; j++ )
		{
			if ( mem_clip_mask == NULL )
			{
			  if ( tool_opacity == 100 )	// No transparency, no mask
			  {
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					offset = 3*(j*pw + i);
					rgb[offset]     = mem_clipboard[ 3*(mx + my*mem_clip_w) ];
					rgb[offset + 1] = mem_clipboard[ 1 + 3*(mx + my*mem_clip_w) ];
					rgb[offset + 2] = mem_clipboard[ 2 + 3*(mx + my*mem_clip_w) ];
				}
			  }
			  else				// Transparency, no mask
			  {
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					offset = 3*(j*pw + i);
					for ( k=0; k<3; k++ )
					{
					 ipix = tool_opacity *
						mem_clipboard[ k + 3*(mx + my*mem_clip_w) ] +
						(100 - tool_opacity) *
						rgb[offset + k];
					 rgb[offset + k] = ipix / 100;
					}
				}
			  }
			}
			else
			{
			  if ( tool_opacity == 100 )	// No transparency, mask
			  {
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					offset = 3*(j*pw + i);
					mask = mem_clip_mask[mx + my*mem_clip_w];
					for ( k=0; k<3; k++ )
					{
					  rgb[offset+k] = (
					    mask * rgb[offset + k]
					    + (255-mask) * mem_clipboard[ k + 3*(mx + my*mem_clip_w) ]
					    ) / 255;
					}
				}
			  }
			  else				// Transparency, mask
			  {
				for ( i=0; i<pw; i++ )
				{
					ix = (px1 + i)/can_zoom;	// Image coords
					iy = (py1 + j)/can_zoom;
					mx = ix - marq_x1;		// Clipboard coords
					my = iy - marq_y1;
					offset = 3*(j*pw + i);
					mask = mem_clip_mask[mx + my*mem_clip_w];
					for ( k=0; k<3; k++ )
					{
					    pixel[0] = mem_clipboard[ k + 3*(mx + my*mem_clip_w) ];
					    pixel[1] = rgb[offset + k];
					    pixel[2] = ((255-mask) * pixel[0] + mask * pixel[1] ) / 255;
					    rgb[ k + offset ] = ( pixel[2] * tool_opacity +
					    	pixel[1] * (100-tool_opacity)
					    	) / 100;
					}
				}
			  }
			}
		}
	}

	gdk_draw_rgb_image ( the_canvas, drawing_canvas->style->black_gc,
			px1, py1, pw, ph, GDK_RGB_DITHER_NONE, rgb, pw*3 );
	free( rgb );
}

void main_render_rgb( unsigned char *rgb, int px, int py, int pw, int ph, float zoom )
{
	unsigned char pix[3] = {0,0,0};
	int ix, iy, pw2, ph2, offset, off24, canz = zoom;

	if ( zoom < 1 ) canz = mt_round(1/zoom);

	pw2 = pw;
	ph2 = ph;
	if ( (px+pw2) >= mem_width*zoom )	// Update image + blank space outside
		pw2 = mem_width*zoom - px;
	if ( (py+ph2) >= mem_height*zoom )	// Update image + blank space outside
		ph2 = mem_height*zoom - py;


	if ( mem_image_bpp == 1 )
	{
		if ( zoom>=1 )
		{
			for ( iy=0; iy<ph2; iy++ )
			{
				for ( ix=0; ix<pw2; ix++ )
				{
					pix[0] = mem_image[(px+ix)/canz
						+ (py+iy)/canz*mem_width];
					offset = 3*(iy*pw + ix);
					rgb[offset] = mem_pal[pix[0]].red;
					rgb[offset + 1] = mem_pal[pix[0]].green;
					rgb[offset + 2] = mem_pal[pix[0]].blue;
				}
			}
		}
		else
		{
			for ( iy=0; iy<ph2; iy++ )
			{
				for ( ix=0; ix<pw2; ix++ )
				{
					pix[0] = mem_image[(px+ix)*canz
						+ (py+iy)*canz*mem_width];
					offset = 3*(iy*pw + ix);
					rgb[offset] = mem_pal[pix[0]].red;
					rgb[offset + 1] = mem_pal[pix[0]].green;
					rgb[offset + 2] = mem_pal[pix[0]].blue;
				}
			}
		}
	}
	if ( mem_image_bpp == 3 )
	{
		if ( zoom>=1 )
		{
			for ( iy=0; iy<ph2; iy++ )
			{
				for ( ix=0; ix<pw2; ix++ )
				{
					off24 = 3 * ( (px+ix)/canz +
							(py+iy)/canz*mem_width );
					offset = 3*(iy*pw + ix);

					rgb[offset] = mem_image[ off24 ];
					rgb[offset + 1] = mem_image[1 + off24];
					rgb[offset + 2] = mem_image[2 + off24];
				}
			}
		}
		else
		{
			for ( iy=0; iy<ph2; iy++ )
			{
				for ( ix=0; ix<pw2; ix++ )
				{
					off24 = 3 * ( (px+ix)*canz +
							(py+iy)*canz*mem_width );
					offset = 3*(iy*pw + ix);

					rgb[offset] = mem_image[ off24 ];
					rgb[offset + 1] = mem_image[1 + off24];
					rgb[offset + 2] = mem_image[2 + off24];
				}
			}
		}
	}
}

void draw_grid(unsigned char *rgb, int x, int y, int w, int h)	// Draw grid on rgb memory
{
	int i, j, gap = can_zoom;
	unsigned char r=mem_grid_rgb[0], g=mem_grid_rgb[1], b=mem_grid_rgb[2], *t_rgb;

	if ( gap>=mem_grid_min && mem_show_grid )
	{
		i = y % gap;
		if (i != 0 ) i = gap - i;			// Calculate y offset at start
		while ( i<h )
		{
			t_rgb = rgb + i*w*3;
			for ( j=0; j<w; j++ )			// Horzontal lines
			{
				t_rgb[0] = r;
				t_rgb[1] = g;
				t_rgb[2] = b;
				t_rgb = t_rgb + 3;
			}
			i = i + gap;
		}
		i = x % gap;
		if (i != 0 ) i = gap - i;			// Calculate x offset at start
		while ( i<w )
		{
			t_rgb = rgb + i*3;
			for ( j=0; j<h; j++ )			// Vertical lines
			{
				t_rgb[0] = r;
				t_rgb[1] = g;
				t_rgb[2] = b;
				t_rgb = t_rgb + 3*w;
			}
			i = i + gap;
		}
	}
}

void repaint_canvas( int px, int py, int pw, int ph )
{
	unsigned char *rgb;
	int iy, pw2, ph2, lx = 0, ly = 0,
		rx1, ry1, rx2, ry2,
		ax1, ay1, ax2, ay2;

	if (zoom_flag == 1) return;		// Stops excess jerking in GTK+1 when zooming

	mtMAX(px, px, 0)
	mtMAX(py, py, 0)

	if ( pw<=0 || ph<=0 ) return;
	rgb = grab_memory( pw*ph*3, "screen RGB redraw", mem_background );
	if ( rgb == NULL ) return;

	if ( layers_total == 0 || !show_layers_main )
		main_render_rgb( rgb, px, py, pw, ph, can_zoom );
	else
	{
		if ( layer_selected > 0 )
		{
			lx = can_zoom * layer_table[layer_selected].x;
			ly = can_zoom * layer_table[layer_selected].y;
		}
		view_render_rgb( rgb, px+lx, py+ly, pw, ph, can_zoom );
	}

	pw2 = pw;
	ph2 = ph;
	if ( (px+pw2) >= mem_width*can_zoom )	// Update image + blank space outside
		pw2 = mem_width*can_zoom - px;
	if ( (py+ph2) >= mem_height*can_zoom )	// Update image + blank space outside
		ph2 = mem_height*can_zoom - py;

	if ( mem_preview > 0 && mem_image_bpp == 3 )
	{
		for ( iy=0; iy<ph2; iy++ )	// Don't touch grey area, just RGB
		{
			if ( mem_prev_bcsp[4] != 100 )
				mem_gamma_chunk( rgb + pw*iy*3, pw2 );
			if ( mem_prev_bcsp[0] != 0 || mem_prev_bcsp[1] != 0 ||
				mem_prev_bcsp[2] != 0)
					mem_brcosa_chunk( rgb + pw*iy*3, pw2 );
			if ( mem_prev_bcsp[3] != 8 )
				mem_posterize_chunk( rgb + pw*iy*3, pw2 );
		}
	}

	draw_grid(rgb, px, py, pw, ph);

	gdk_draw_rgb_image ( the_canvas, drawing_canvas->style->black_gc,
		px, py, pw, ph, GDK_RGB_DITHER_NONE, rgb, pw*3 );

	free( rgb );

	if ( marq_status >= MARQUEE_PASTE && show_paste )
	{	// Add clipboard image to redraw if needed
		rx1 = px/can_zoom;
		ry1 = py/can_zoom;
		rx2 = (px + pw2 - 1)/can_zoom;
		ry2 = (py + ph2 - 1)/can_zoom;
		if ( !(marq_x1<rx1 && marq_x2<rx1) && !(marq_x1>rx2 && marq_x2>rx2) &&
			!(marq_y1<ry1 && marq_y2<ry1) && !(marq_y1>ry2 && marq_y2>ry2) )
		{
			mtMAX( ax1, px, marq_x1*can_zoom )
			mtMAX( ay1, py, marq_y1*can_zoom )
			mtMIN( ax2, (px + pw2 - 1), (can_zoom*(marq_x2 + 1) - 1 ) )
			mtMIN( ay2, (py + ph2 - 1), (can_zoom*(marq_y2 + 1) - 1 ) )
			repaint_paste( ax1, ay1, ax2, ay2 );
		}
	}

	if ( marq_status != MARQUEE_NONE ) paint_marquee(11, marq_x1, marq_y1);
	if ( perim_status > 0 ) repaint_perim();
				// Draw perimeter/marquee/line as we may have drawn over them
}

void clear_perim_real( int ox, int oy )
{
	int x = (perim_x + ox)*can_zoom, y = (perim_y + oy)*can_zoom, s = perim_s*can_zoom;

	repaint_canvas( x, y, 1, s );
	repaint_canvas(	x+s-1, y, 1, s );
	repaint_canvas(	x, y, s, 1 );
	repaint_canvas(	x, y+s-1, s, 1 );
}

void clear_perim()
{
	if ( tool_type != TOOL_SELECT && tool_type != TOOL_POLYGON && tool_type != TOOL_FLOOD )
	{		// Don't bother if we are selecting or filling
		clear_perim_real(0, 0);
		if ( tool_type == TOOL_CLONE ) clear_perim_real(clone_x, clone_y);
	}
}

void repaint_perim_real( int r, int g, int b, int ox, int oy )
{
	int x = (perim_x + ox)*can_zoom, y = (perim_y + oy)*can_zoom, s = perim_s*can_zoom;
	int i;
	char *rgb;

	rgb = grab_memory( s*3, "screen RGB redraw", 255 );
	for ( i=0; i<s; i++ )
	{
		rgb[ 0 + 3*i ] = r * ((i/3) % 2);
		rgb[ 1 + 3*i ] = g * ((i/3) % 2);
		rgb[ 2 + 3*i ] = b * ((i/3) % 2);
	}

	gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
		x, y, 1, s, GDK_RGB_DITHER_NONE, rgb, 3 );
	gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
		x+s-1, y, 1, s, GDK_RGB_DITHER_NONE, rgb, 3 );

	gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
		x, y, s, 1, GDK_RGB_DITHER_NONE, rgb, 3*s );
	gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
		x, y+s-1, s, 1, GDK_RGB_DITHER_NONE, rgb, 3*s );
	free(rgb);
}

void repaint_perim()
{
	if ( tool_type != TOOL_SELECT && tool_type != TOOL_POLYGON && tool_type != TOOL_FLOOD )
					// Don't bother if we are selecting or filling
	{
		repaint_perim_real( 255, 255, 255, 0, 0 );
		if ( tool_type == TOOL_CLONE )
			repaint_perim_real( 255, 0, 0, clone_x, clone_y );
	}
}

static gint canvas_motion( GtkWidget *widget, GdkEventMotion *event )
{
	char txt[64];
	GdkCursor *temp_cursor = NULL;
	GdkCursorType pointers[] = {GDK_TOP_LEFT_CORNER, GDK_TOP_RIGHT_CORNER,
		GDK_BOTTOM_LEFT_CORNER, GDK_BOTTOM_RIGHT_CORNER};
	unsigned char pixel;
	png_color pixel24;
	int x, y, r, g, b, s;
	int ox, oy, i, new_cursor;
	int tox = tool_ox, toy = tool_oy;
	GdkModifierType state;
	guint button = 0;
	gdouble pressure = 1.0;


	if ( mem_image != NULL )		// Only do this if we have an image
	{
#if GTK_MAJOR_VERSION == 1
		if (event->is_hint)
		{
			gdk_input_window_get_pointer (event->window, event->deviceid,
					NULL, NULL, &pressure, NULL, NULL, &state);
			gdk_window_get_pointer (event->window, &x, &y, &state);
		}
		else
		{
			x = event->x;
			y = event->y;
			pressure = event->pressure;
			state = event->state;
		}
#endif
#if GTK_MAJOR_VERSION == 2
		if (event->is_hint) gdk_device_get_state (event->device, event->window, NULL, &state);
		x = event->x;
		y = event->y;
		state = event->state;

		if ( tablet_working )
			gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);
#endif

		if (state & GDK_BUTTON2_MASK) button = 2;
		if (state & GDK_BUTTON3_MASK) button = 3;
		if (state & GDK_BUTTON1_MASK) button = 1;
		if ( (state & GDK_BUTTON1_MASK) && (state & GDK_BUTTON3_MASK) ) button = 13;
		mouse_event( x, y, state, button, pressure );

		x = x / can_zoom;
		y = y / can_zoom;
		if ( tool_fixx > 0 ) x = tool_fixx;
		if ( tool_fixy > 0 ) y = tool_fixy;

		if ( tool_type == TOOL_CLONE )
		{
			tool_ox = x;
			tool_oy = y;
		}

		ox = x; oy = y;
		mtMIN( ox, x, mem_width-1 )
		mtMIN( oy, y, mem_height-1 )
		mtMAX( ox, ox, 0 )
		mtMAX( oy, oy, 0 )

		if ( poly_status == POLY_SELECTING && button == 0 )
		{
			stretch_poly_line(ox, oy);
		}

		if ( x >= mem_width ) x = -1;
		if ( y >= mem_height ) y = -1;

		if ( tool_type == TOOL_SELECT || tool_type == TOOL_POLYGON )
		{
			if ( marq_status == MARQUEE_DONE )
			{
				if ( inifile_get_gboolean( "cursorToggle", TRUE ) )
				{
					i = close_to(ox, oy);
					if ( i!=cursor_corner ) // Stops excessive CPU/flickering
					{
					 cursor_corner = i;
					 temp_cursor = gdk_cursor_new(pointers[i]);
					 gdk_window_set_cursor(drawing_canvas->window, temp_cursor);
					 gdk_cursor_destroy(temp_cursor);
					}
				}
				else	set_cursor();
			}
			if ( marq_status >= MARQUEE_PASTE )
			{
				new_cursor = 0;		// Cursor = normal
				if ( ox>=marq_x1 && ox<=marq_x2 && oy>=marq_y1 && oy<=marq_y2 )
					new_cursor = 1;		// Cursor = 4 way arrow

				if ( new_cursor != cursor_corner ) // Stops flickering on slow hardware
				{
					if ( !inifile_get_gboolean( "cursorToggle", TRUE ) ||
						new_cursor == 0 )
							 set_cursor();
					else
					 gdk_window_set_cursor( drawing_canvas->window, move_cursor );
					cursor_corner = new_cursor;
				}
			}
		}
		update_sel_bar();

		if ( x>=0 && y>= 0 )
		{
			if ( status_on[2] )
			{
				snprintf(txt, 60, "%i,%i", x, y);
				gtk_label_set_text( GTK_LABEL(label_bar2), txt );
			}
			if ( mem_image_bpp == 1 )
			{
				pixel = GET_PIXEL( x, y );
				r = mem_pal[pixel].red;
				g = mem_pal[pixel].green;
				b = mem_pal[pixel].blue;
				snprintf(txt, 60, "[%u] = {%i,%i,%i}", pixel, r, g, b);
			}
			if ( mem_image_bpp == 3 )
			{
				pixel24 = get_pixel24( x, y );
				r = pixel24.red;
				g = pixel24.green;
				b = pixel24.blue;
				snprintf(txt, 60, "{%i,%i,%i}", r, g, b);
			}
			if ( status_on[3] )
			{
				gtk_label_set_text( GTK_LABEL(label_bar3), txt );
			}
		}
		else
		{
			if ( status_on[2] ) gtk_label_set_text( GTK_LABEL(label_bar2), "" );
			if ( status_on[3] ) gtk_label_set_text( GTK_LABEL(label_bar3), "" );
		}

///	TOOL PERIMETER BOX UPDATES

		s = tool_size;
		if ( perim_status > 0 )
		{
			perim_status = 0;
			clear_perim();
			perim_status = 1;
					// Remove old perimeter box
		}

		if ( tool_type == TOOL_CLONE && button == 0 && (state & GDK_CONTROL_MASK) )
		{
			clone_x += (tox-ox);
			clone_y += (toy-oy);
		}

		if ( s*can_zoom > 4 )
		{
			perim_status = 1;
			perim_x = ox - (tool_size - (tool_size % 2) )/2;
			perim_y = oy - (tool_size - (tool_size % 2) )/2;
			perim_s = s;
			repaint_perim();			// Repaint 4 sides
		}
		else	perim_status = 0;

///	LINE UPDATES
		if ( tool_type == TOOL_LINE && line_status != LINE_NONE )
		{
			if ( line_x1 != ox || line_y1 != oy )
			{
				repaint_line(0);
				line_x1 = ox;
				line_y1 = oy;
				repaint_line(1);
			}
		}
	}

	return TRUE;
}

static gint expose_canvas( GtkWidget *widget, GdkEventExpose *event )
{
	int px, py, pw, ph;

	if ( the_canvas == NULL) the_canvas = widget->window;

	px = event->area.x;		// Only repaint if we need to
	py = event->area.y;
	pw = event->area.width;
	ph = event->area.height;

	repaint_canvas( px, py, pw, ph );
	paint_poly_marquee();

	return FALSE;
}

static gint expose_palette( GtkWidget *widget, GdkEventExpose *event )
{
	gdk_draw_rgb_image( widget->window, widget->style->black_gc,
				event->area.x, event->area.y, event->area.width, event->area.height,
				GDK_RGB_DITHER_NONE,
				mem_pals + 3*( event->area.x + PALETTE_WIDTH*event->area.y ),
				PALETTE_WIDTH*3
				);

	return FALSE;
}

void pressed_choose_patterns( GtkMenuItem *menu_item, gpointer user_data )
{	choose_pattern(0);	}

void pressed_choose_brush( GtkMenuItem *menu_item, gpointer user_data )
{	choose_pattern(1);	}

void pressed_edit_AB( GtkMenuItem *menu_item, gpointer user_data )
{	choose_colours();	}

static gint motion_palette( GtkWidget *widget, GdkEventMotion *event )
{
	GdkModifierType state;
	int x, y, px, py, pindex;

	px = event->x;
	py = event->y;
	pindex = (py-39+34)/16;

	mtMAX(pindex, pindex, 0)
	mtMIN(pindex, pindex, mem_cols-1)

	if (event->is_hint) gdk_window_get_pointer (event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if ( drag_index && drag_index_vals[1] != pindex )
	{
		mem_pal_index_move( drag_index_vals[1], pindex );
		init_pal();
		drag_index_vals[1] = pindex;
	}

	return TRUE;
}

static gint release_palette( GtkWidget *widget, GdkEventButton *event )
{
	if (drag_index)
	{
		drag_index = FALSE;
		gdk_window_set_cursor( drawing_palette->window, NULL );
		if ( drag_index_vals[0] != drag_index_vals[1] )
		{
			mem_pal_copy( mem_pal, brcosa_pal );		// Get old values back
			pen_down = 0;
			mem_undo_next();				// Do undo stuff
			pen_down = 0;
			mem_pal_index_move( drag_index_vals[0], drag_index_vals[1] );

			if ( mem_image_bpp == 1 )
				mem_canvas_index_move( drag_index_vals[0], drag_index_vals[1] );

			canvas_undo_chores();
		}
	}

	return FALSE;
}

static gint click_palette( GtkWidget *widget, GdkEventButton *event )
{
	int px, py, pindex;

	px = event->x;
	py = event->y;
	pindex = (py-2)/16;
	if (py < 2) pindex = -1;

	if (pindex>=0 && pindex<mem_cols)
	{
		if ( px>22 && px <53 )		// Colour A or B changed
		{
			if ( event->button == 1 )
			{
				if ( (event->state & GDK_CONTROL_MASK) )
				{
					mem_col_B = pindex;
					mem_col_B24 = mem_pal[mem_col_B];
				}
				else if ( (event->state & GDK_SHIFT_MASK) )
				{
					mem_pal_copy( brcosa_pal, mem_pal );
					drag_index = TRUE;
					drag_index_vals[0] = pindex;
					drag_index_vals[1] = pindex;
					gdk_window_set_cursor( drawing_palette->window, move_cursor );
				}
				else
				{
					mem_col_A = pindex;
					mem_col_A24 = mem_pal[mem_col_A];
				}
			}
			if ( event->button == 3 )
			{
				mem_col_B = pindex;
				mem_col_B24 = mem_pal[mem_col_B];
			}

			repaint_top_swatch();
			init_pal();
			gtk_widget_queue_draw( drawing_col_prev );	// Update widget
		}
		if ( px>=53 )			// Mask changed
		{
			if ( mem_prot_mask[pindex] == 0 ) mem_prot_mask[pindex] = 1;
			else mem_prot_mask[pindex] = 0;

			repaint_swatch(pindex);				// Update swatch
			gtk_widget_queue_draw_area( widget, 0, event->y-16,
				PALETTE_WIDTH, 32 );			// Update widget

			mem_mask_init();		// Prepare RGB masks
		}
	}

	return TRUE;
}

static gint click_colours( GtkWidget *widget, GdkEventButton *event )
{
	if ( mem_image != NULL ) choose_colours();

	return FALSE;
}

static gint click_pattern( GtkWidget *widget, GdkEventButton *event )
{
	if ( mem_image != NULL )
	{
		if ( event->y < 32 ) choose_pattern(0); else choose_pattern(1);
	}

	return FALSE;
}

static gint expose_pattern( GtkWidget *widget, GdkEventExpose *event )
{
	int rx, ry, rw, rh;

	rx = event->area.x;
	ry = event->area.y;
	rw = event->area.width;
	rh = event->area.height;

	if ( ry < PATTERN_HEIGHT )
	{
		if ( (ry+rh) >= PATTERN_HEIGHT )
		{
			rh = PATTERN_HEIGHT - ry;
		}
		gdk_draw_rgb_image( widget->window, widget->style->black_gc,
				rx, ry, rw, rh,
				GDK_RGB_DITHER_NONE,
				mem_pats + 3*( rx + PATTERN_WIDTH*ry ),
				PATTERN_WIDTH*3
				);
	}
	return FALSE;
}

static gint expose_preview( GtkWidget *widget, GdkEventExpose *event )
{
	int rx, ry, rw, rh;

	rx = event->area.x;
	ry = event->area.y;
	rw = event->area.width;
	rh = event->area.height;

	if ( ry < PREVIEW_HEIGHT )
	{
		if ( (ry+rh) >= PREVIEW_HEIGHT )
		{
			rh = PREVIEW_HEIGHT - ry;
		}
		gdk_draw_rgb_image( widget->window, widget->style->black_gc,
				rx, ry, rw, rh,
				GDK_RGB_DITHER_NONE,
				mem_prev + 3*( rx + PREVIEW_WIDTH*ry ),
				PREVIEW_WIDTH*3
				);
	}

	return FALSE;
}

void set_cursor()			// Set mouse cursor
{
	if ( inifile_get_gboolean( "cursorToggle", TRUE ) )
		gdk_window_set_cursor( drawing_canvas->window, m_cursor[tool_type] );
	else
		gdk_window_set_cursor( drawing_canvas->window, NULL );
}


GtkWidget *icon_buttons[TOTAL_ICONS_TOOLBAR], *icon_buttons2[TOTAL_ICONS_TOOLBAR];

void toolbar_icon_event2(GtkWidget *widget, gpointer data)
{
	gint j = (gint) data;

	switch (j)
	{
		case 0:  pressed_new( NULL, NULL ); break;
		case 1:  pressed_open_file( NULL, NULL ); break;
		case 2:  pressed_save_file( NULL, NULL ); break;
		case 3:  pressed_cut( NULL, NULL ); break;
		case 4:  pressed_copy( NULL, NULL ); break;
		case 5:  pressed_paste_centre( NULL, NULL ); break;
		case 6:  main_undo( NULL, NULL ); break;
		case 7:  main_redo( NULL, NULL ); break;
		case 8:  pressed_outline_ellipse( NULL, NULL ); break;
		case 9:  pressed_fill_ellipse( NULL, NULL ); break;
		case 10: pressed_outline_rectangle( NULL, NULL ); break;
		case 11: pressed_fill_rectangle( NULL, NULL ); break;
		case 12: pressed_flip_sel_v( NULL, NULL ); break;
		case 13: pressed_flip_sel_h( NULL, NULL ); break;
		case 14: pressed_rotate_sel_clock( NULL, NULL ); break;
		case 15: pressed_rotate_sel_anti( NULL, NULL ); break;
	}
}


void toolbar_icon_event (GtkWidget *widget, gpointer data)
{
	gint i = tool_type, j = (gint) data;

	switch (j)
	{
		case 0:  pressed_choose_brush( NULL, NULL ); break;
		case 1:  tool_type = brush_tool_type; break;
		case 2:  tool_type = TOOL_SHUFFLE; break;
		case 3:  tool_type = TOOL_FLOOD; break;
		case 4:  tool_type = TOOL_LINE; break;
		case 5:  tool_type = TOOL_SMUDGE; break;
		case 6:  tool_type = TOOL_CLONE; break;
		case 7:  tool_type = TOOL_SELECT; break;
		case 8:  tool_type = TOOL_POLYGON; break;
		case 9:  pressed_lasso( NULL, NULL ); break;
		case 10: pressed_crop( NULL, NULL ); break;
		case 11: pressed_text( NULL, NULL ); break;
		case 12: pressed_brcosa( NULL, NULL ); break;
		case 13: pressed_allcol( NULL, NULL ); break;
		case 14: pressed_information( NULL, NULL ); break;
		case 15: pressed_pan( NULL, NULL ); break;
	}

	if ( tool_type != i )		// User has changed tool
	{
		if ( i == TOOL_LINE && tool_type != TOOL_LINE ) stop_line();
		if ( marq_status != MARQUEE_NONE)
		{
			if ( marq_status >= MARQUEE_PASTE &&
				inifile_get_gboolean( "pasteCommit", FALSE ) )
			{
				commit_paste(TRUE);
				pen_down = 0;
			}

			marq_status = MARQUEE_NONE;			// Marquee is on so lose it!
			update_sel_bar();
			gtk_widget_queue_draw( drawing_canvas );	// Needed to clear selection
		}
		if ( poly_status != POLY_NONE)
		{
			poly_status = POLY_NONE;			// Marquee is on so lose it!
			poly_points = 0;
			update_sel_bar();
			gtk_widget_queue_draw( drawing_canvas );	// Needed to clear selection
		}
		if ( tool_type == TOOL_CLONE )
		{
			clone_x = -tool_size;
			clone_y = tool_size;
		}
		update_menus();
		set_cursor();
	}
}

GtkWidget *layer_iconbar(GtkWidget *window, GtkWidget *box, GtkWidget **icons)
{		// Create iconbar for layers window
	static char **icon_list[10] = {
		xpm_new_xpm, xpm_up_xpm, xpm_down_xpm, xpm_copy_xpm, xpm_centre_xpm,
		xpm_cut_xpm, xpm_save_xpm, xpm_saveas_xpm, xpm_dustbin_xpm, xpm_close_xpm
		};

	char *hint_text[10] = {
		_("New Layer"), _("Raise"), _("Lower"), _("Duplicate Layer"), _("Centralise Layer"),
		_("Delete Layer"), _("Save Layer Information & Composite Image"),
		_("Save As ..."), _("Delete All Layers"), _("Close Layers Window")
		};

	gint i, offset[10] = {3, 1, 2, 4, 6, 5, 0, 0, 7, 8};

	GtkWidget *toolbar, *iconw;
	GdkPixmap *icon, *mask;

// we need to realize the window because we use pixmaps for 
// items on the toolbar in the context of it
	gtk_widget_realize( window );

#if GTK_MAJOR_VERSION == 1
	toolbar = gtk_toolbar_new( GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS );
#endif
#if GTK_MAJOR_VERSION == 2
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );
#endif

	gtk_box_pack_start ( GTK_BOX (box), toolbar, FALSE, FALSE, 0 );

	for (i=0; i<10; i++)
	{
		icon = gdk_pixmap_create_from_xpm_d ( main_window->window, &mask,
			NULL, icon_list[i] );
		iconw = gtk_pixmap_new ( icon, mask );
		gdk_pixmap_unref( icon );
		gdk_pixmap_unref( mask );

		icons[ offset[i] ] =
			gtk_toolbar_append_element( GTK_TOOLBAR(toolbar),
			GTK_TOOLBAR_CHILD_BUTTON, NULL, "None", hint_text[i],
			"Private", iconw, GTK_SIGNAL_FUNC(layer_iconbar_click), (gpointer) i);
	}
	gtk_widget_show ( toolbar );

	return toolbar;
}

void main_init()
{
	gint i;

	static char **icon_list2[TOTAL_ICONS_TOOLBAR] = {
		xpm_new_xpm, xpm_open_xpm, xpm_save_xpm, xpm_cut_xpm,
		xpm_copy_xpm, xpm_paste_xpm, xpm_undo_xpm, xpm_redo_xpm,
		xpm_ellipse2_xpm, xpm_ellipse_xpm, xpm_rect1_xpm, xpm_rect2_xpm,
		xpm_flip_vs_xpm, xpm_flip_hs_xpm, xpm_rotate_cs_xpm, xpm_rotate_as_xpm
		};		// Top icon bar

	static char **icon_list[TOTAL_ICONS_TOOLBAR] = {
		xpm_brush_xpm, xpm_paint_xpm, xpm_shuffle_xpm, xpm_flood_xpm,
		xpm_line_xpm, xpm_smudge_xpm, xpm_clone_xpm, xpm_select_xpm,
		xpm_polygon_xpm, xpm_lasso_xpm, xpm_crop_xpm, xpm_text_xpm,
		xpm_brcosa_xpm, xpm_cols_xpm, xpm_info_xpm, xpm_pan_xpm
		};		// Bottom icon bar

	static unsigned char *xbm_list[TOTAL_CURSORS] = { xbm_square_bits, xbm_circle_bits,
		xbm_horizontal_bits, xbm_vertical_bits, xbm_slash_bits, xbm_backslash_bits,
		xbm_spray_bits, xbm_shuffle_bits, xbm_flood_bits, xbm_select_bits, xbm_line_bits,
		xbm_smudge_bits, xbm_polygon_bits, xbm_clone_bits };

	static char *xbm_mask_list[TOTAL_CURSORS] = { xbm_square_mask_bits, xbm_circle_mask_bits,
		xbm_horizontal_mask_bits, xbm_vertical_mask_bits, xbm_slash_mask_bits,
		xbm_backslash_mask_bits, xbm_spray_mask_bits, xbm_shuffle_mask_bits,
		xbm_flood_mask_bits, xbm_select_mask_bits, xbm_line_mask_bits,
		xbm_smudge_mask_bits, xbm_polygon_mask_bits, xbm_clone_mask_bits };

	char *hint_text2[TOTAL_ICONS_TOOLBAR] = {
		_("New Image"), _("Load Image File"), _("Save Image File"), _("Cut"),
		_("Copy"), _("Paste"), _("Undo"), _("Redo"),
		_("Ellipse Outline"), _("Filled Ellipse"), _("Outline Selection"), _("Fill Selection"),
		_("Flip Selection Vertically"), _("Flip Selection Horizontally"),
		_("Rotate Selection Clockwise"), _("Rotate Selection Anti-Clockwise"),
		 };
	char *hint_text[TOTAL_ICONS_TOOLBAR] = {
		_("Choose Brush"), _("Paint"), _("Shuffle"), _("Flood Fill"),
		_("Straight Line"), _("Smudge"), _("Clone"), _("Make Selection"),
		_("Polygon Selection"), _("Lasso Selection"), _("Crop"), _("Paste Text"),
		_("Transform Colour"), _("Edit All Colours"), _("Information"), _("Pan Window")
		};
	char txt[64];

	GtkToolbarChildType child_type;
	GtkWidget *previous = NULL;
	GdkPixmap *icon, *mask;
	GtkWidget *iconw;
	GdkColor cfg = { -1, -1, -1, -1 }, cbg = { 0, 0, 0, 0 };

	GtkWidget *vbox_main, *hbox_top, *hbox_bar;
	GtkWidget *table_RGB, *table_tools;
	GtkWidget *hbox_bottom, *vbox_right, *vbox_top, *toolbar, *toolbar2;
	GtkAccelGroup *accel_group;

	GtkItemFactory *item_factory;

	GtkItemFactoryEntry menu_items[] = {
		{ _("/_File"),			NULL,		NULL,0, "<Branch>" },
		{ _("/File/tear"),		NULL,		NULL,0, "<Tearoff>" },
		{ _("/File/New"),		"<control>N",	pressed_new,0, NULL },
		{ _("/File/Open ..."),		"<control>O",	pressed_open_file, 0, NULL },
		{ _("/File/Save"),		"<control>S",	pressed_save_file,0, NULL },
		{ _("/File/Save As ..."),	"<shift><control>S", pressed_save_file_as, 0, NULL },
		{ _("/File/sep1"),		NULL, 	  NULL,0, "<Separator>" },
		{ _("/File/Export Undo Images ..."), NULL,	pressed_export_undo,0, NULL },
		{ _("/File/Export Undo Images (reversed) ..."), NULL, pressed_export_undo2,0, NULL },
		{ _("/File/Export ASCII Art ..."), NULL, 	pressed_export_ascii,0, NULL },
		{ _("/File/sep3"),		NULL,		NULL,0, "<Separator>" },
		{ _("/File/1"),  		"<shift><control>F1", pressed_load_recent,0, NULL },
		{ _("/File/2"),  		"<shift><control>F2", pressed_load_recent,0, NULL },
		{ _("/File/3"),  		"<shift><control>F3", pressed_load_recent,0, NULL },
		{ _("/File/4"),  		"<shift><control>F4", pressed_load_recent,0, NULL },
		{ _("/File/5"),  		"<shift><control>F5", pressed_load_recent,0, NULL },
		{ _("/File/6"),  		"<shift><control>F6", pressed_load_recent,0, NULL },
		{ _("/File/7"),  		"<shift><control>F7", pressed_load_recent,0, NULL },
		{ _("/File/8"),  		"<shift><control>F8", pressed_load_recent,0, NULL },
		{ _("/File/9"),  		"<shift><control>F9", pressed_load_recent,0, NULL },
		{ _("/File/10"), 		"<shift><control>F10", pressed_load_recent,0, NULL },
		{ _("/File/11"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/12"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/13"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/14"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/15"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/16"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/17"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/18"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/19"), 		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/20"),		NULL,		pressed_load_recent,0, NULL },
		{ _("/File/sep2"),		NULL,		NULL,0, "<Separator>" },
		{ _("/File/Quit"),		"<control>Q",	quit_all,	0, NULL },

		{ _("/_Edit"),			NULL,		NULL,0, "<Branch>" },
		{ _("/Edit/tear"),		NULL,		NULL,0, "<Tearoff>" },
		{ _("/Edit/Undo"),		"<control>Z",	main_undo,0, NULL },
		{ _("/Edit/Redo"),		"<control>R",	main_redo,0, NULL },
		{ _("/Edit/sep1"),		NULL,		NULL,0, "<Separator>" },
		{ _("/Edit/Cut"),		"<control>X",	pressed_cut, 0, NULL },
		{ _("/Edit/Copy"),		"<control>C",	pressed_copy, 0, NULL },
		{ _("/Edit/Paste To Centre"),	"<control>V",	pressed_paste_centre, 0, NULL },
		{ _("/Edit/Paste To New Layer"), "<control><shift>V", pressed_paste_layer, 0, NULL },
		{ _("/Edit/Paste"),		"<control>K",	pressed_paste, 0, NULL },
		{ _("/Edit/Paste Text"),	"T",		pressed_text, 0, NULL },
		{ _("/Edit/sep2"),			NULL, 	  NULL,0, "<Separator>" },
		{ _("/Edit/Load Clipboard"),		NULL, 	  NULL, 0, "<Branch>" },
		{ _("/Edit/Load Clipboard/tear"),	NULL, 	  NULL, 0, "<Tearoff>" },
		{ _("/Edit/Load Clipboard/1"),		"<shift>F1",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/2"),		"<shift>F2",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/3"),		"<shift>F3",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/4"),		"<shift>F4",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/5"),		"<shift>F5",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/6"),		"<shift>F6",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/7"),		"<shift>F7",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/8"),		"<shift>F8",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/9"),		"<shift>F9",    load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/10"),		"<shift>F10",   load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/11"),		"<shift>F11",   load_clip, 0, NULL },
		{ _("/Edit/Load Clipboard/12"),		"<shift>F12",   load_clip, 0, NULL },
		{ _("/Edit/Save Clipboard"),		NULL, 	  NULL, 0, "<Branch>" },
		{ _("/Edit/Save Clipboard/tear"),	NULL, 	  NULL, 0, "<Tearoff>" },
		{ _("/Edit/Save Clipboard/1"),		"<control>F1",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/2"),		"<control>F2",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/3"),		"<control>F3",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/4"),		"<control>F4",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/5"),		"<control>F5",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/6"),		"<control>F6",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/7"),		"<control>F7",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/8"),		"<control>F8",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/9"),		"<control>F9",  save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/10"),  	"<control>F10", save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/11"),  	"<control>F11", save_clip, 0, NULL },
		{ _("/Edit/Save Clipboard/12"),  	"<control>F12", save_clip, 0, NULL },
		{ _("/Edit/sep4"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Edit/Choose Pattern ..."),	"F2",	pressed_choose_patterns,0, NULL },
		{ _("/Edit/Choose Brush ..."),		"F3",	pressed_choose_brush,0, NULL },
		{ _("/Edit/Create Patterns"),		NULL,	pressed_create_patterns,0, NULL },
		{ _("/Edit/sep5"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Edit/Continuous Painting"),	"F11",	pressed_continuous, 0, "<CheckItem>" },
		{ _("/Edit/Opacity Undo Mode"),		"F12",	pressed_opacity_mode, 0, "<CheckItem>" },

		{ _("/_View"),			NULL,		NULL,0, "<Branch>" },
		{ _("/View/tear"),		NULL,		NULL,0, "<Tearoff>" },
		{ _("/View/10%"),		"1",		zoom_to10,0, NULL },
		{ _("/View/25%"),		"2",		zoom_to25,0, NULL },
		{ _("/View/50%"),		"3",		zoom_to50,0, NULL },
		{ _("/View/100%"),		"4",		zoom_to1,0, NULL },
		{ _("/View/400%"),		"5",		zoom_to4,0, NULL },
		{ _("/View/800%"),		"6",		zoom_to8,0, NULL },
		{ _("/View/1200%"),		"7",		zoom_to12,0, NULL },
		{ _("/View/1600%"),		"8",		zoom_to16,0, NULL },
		{ _("/View/2000%"),		"9",		zoom_to20,0, NULL },
		{ _("/View/sep1"),		NULL,		NULL,0, "<Separator>" },
		{ _("/View/Zoom in (+)"),	NULL,		zoom_in,0, NULL },
		{ _("/View/Zoom out (-)"),	NULL,		zoom_out,0, NULL },
		{ _("/View/Show zoom grid"),	NULL,		zoom_grid,0, "<CheckItem>" },
		{ _("/View/sep2"),		NULL,		NULL,0, "<Separator>" },
		{ _("/View/Toggle Image View (Home)"), NULL,	toggle_view,0, NULL },
		{ _("/View/Pan Window (End)"),	NULL,		pressed_pan,0, NULL },
		{ _("/View/Command Line Window"),	"C",	pressed_cline,0, NULL },
		{ _("/View/View Window"),		"V",	pressed_view,0, NULL },
		{ _("/View/Layers Window"),		"L",	pressed_layers, 0, NULL },

		{ _("/_Image"),  			NULL, 	NULL,0, "<Branch>" },
		{ _("/Image/tear"),			NULL, 	NULL,0, "<Tearoff>" },
		{ _("/Image/Convert To RGB"),		NULL, 	pressed_convert_rgb,0, NULL },
		{ _("/Image/Convert To Indexed"),	NULL,	pressed_quantize,0, NULL },
		{ _("/Image/sep1"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Image/Scale Canvas ..."),  	NULL,	pressed_scale,0, NULL },
		{ _("/Image/Resize Canvas ..."), 	NULL,	pressed_size,0, NULL },
		{ _("/Image/Crop"),			"<control><shift>X", pressed_crop, 0, NULL },
		{ _("/Image/sep5"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Image/Flip Vertically"),		NULL,	pressed_flip_image_v,0, NULL },
		{ _("/Image/Flip Horizontally"), 	"<control>M", pressed_flip_image_h,0, NULL },
		{ _("/Image/Rotate Clockwise"),  	NULL,	pressed_rotate_image_clock,0, NULL },
		{ _("/Image/Rotate Anti-Clockwise"),	NULL,	pressed_rotate_image_anti,0, NULL },
		{ _("/Image/Free Rotate ..."),		NULL,	pressed_rotate_free,0, NULL },
		{ _("/Image/sep2"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Image/Information ..."),		"<control>I", pressed_information,0, NULL },
		{ _("/Image/Preferences ..."),		"<control>P", pressed_preferences,0, NULL },

		{ _("/_Selection"),			NULL,	NULL,0, "<Branch>" },
		{ _("/Selection/tear"),  		NULL,	NULL,0, "<Tearoff>" },
		{ _("/Selection/Select All"),		"<control>A",   pressed_select_all, 0, NULL },
		{ _("/Selection/Select None (Esc)"), "<shift><control>A", pressed_select_none,0, NULL },
		{ _("/Selection/Lasso Selection"),	NULL,	pressed_lasso, 0, NULL },
		{ _("/Selection/Lasso Selection Cut"),	NULL,	pressed_lasso_cut, 0, NULL },
		{ _("/Selection/sep1"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Selection/Outline Selection"), "<control>T", pressed_outline_rectangle, 0, NULL },
		{ _("/Selection/Fill Selection"), "<shift><control>T", pressed_fill_rectangle, 0, NULL },
		{ _("/Selection/Outline Ellipse"), "<control>L", pressed_outline_ellipse, 0, NULL },
		{ _("/Selection/Fill Ellipse"), "<shift><control>L", pressed_fill_ellipse, 0, NULL },
		{ _("/Selection/sep2"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Selection/Flip Vertically"),	NULL,	pressed_flip_sel_v,0, NULL },
		{ _("/Selection/Flip Horizontally"),	NULL,	pressed_flip_sel_h,0, NULL },
		{ _("/Selection/Rotate Clockwise"),	NULL,	pressed_rotate_sel_clock, 0, NULL },
		{ _("/Selection/Rotate Anti-Clockwise"), NULL,	pressed_rotate_sel_anti, 0, NULL },
		{ _("/Selection/sep3"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Selection/Mask Colour A,B"),	NULL,	pressed_clip_mask,0, NULL },
		{ _("/Selection/Unmask Colour A,B"),	NULL,	pressed_clip_unmask,0, NULL },
		{ _("/Selection/Mask All Colours"),	NULL,	pressed_clip_mask_all,0, NULL },
		{ _("/Selection/Clear Mask"),		NULL,	pressed_clip_mask_clear,0, NULL },

		{ _("/_Palette"),			NULL, 	NULL,0, "<Branch>" },
		{ _("/Palette/tear"),			NULL,	NULL,0, "<Tearoff>" },
		{ _("/Palette/Open ..."),		NULL,	pressed_open_pal,0, NULL },
		{ _("/Palette/Save As ..."),		NULL,	pressed_save_pal,0, NULL },
		{ _("/Palette/Create Scale ..."),	NULL,	pressed_create_pscale,0, NULL },
		{ _("/Palette/Load Default"),		NULL, 	pressed_default_pal,0, NULL },
		{ _("/Palette/sep1"),			NULL, 	NULL,0, "<Separator>" },
		{ _("/Palette/Mask All"),		NULL, 	pressed_mask_all,0, NULL },
		{ _("/Palette/Mask None"),		NULL, 	pressed_mask_none,0, NULL },
		{ _("/Palette/sep2"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Palette/Swap A & B"),		"X",	pressed_swap_AB,0, NULL },
		{ _("/Palette/Edit Colour A & B ..."), "<control>E", pressed_edit_AB,0, NULL },
		{ _("/Palette/Edit All Colours ..."), "<control>W", pressed_allcol,0, NULL },
		{ _("/Palette/Set Palette Size ..."),	NULL,	pressed_add_cols,0, NULL },
		{ _("/Palette/Merge Duplicate Colours"), NULL,	pressed_remove_duplicates,0, NULL },
		{ _("/Palette/Remove Unused Colours"),	NULL,	pressed_remove_unused,0, NULL },
		{ _("/Palette/sep3"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Palette/Create Quantized (DL1)"), NULL,	pressed_create_dl1,0, NULL },
		{ _("/Palette/Create Quantized (DL3)"), NULL,	pressed_create_dl3,0, NULL },
		{ _("/Palette/Create Quantized (Wu)"),  NULL,	pressed_create_wu,0, NULL },
		{ _("/Palette/sep4"),			NULL,	NULL,0, "<Separator>" },
		{ _("/Palette/Sort Colours ..."),	NULL,	pressed_sort_pal,0, NULL },

		{ _("/_Opacity"),		NULL,		NULL, 0, "<Branch>" },
		{ _("/Opacity/tear"),		NULL,		NULL, 0, "<Tearoff>" },
		{ _("/Opacity/10%"),		"<control>1",	pressed_opacity, 0, NULL },
		{ _("/Opacity/20%"),		"<control>2",	pressed_opacity, 0, NULL },
		{ _("/Opacity/30%"),		"<control>3",	pressed_opacity, 0, NULL },
		{ _("/Opacity/40%"),		"<control>4",	pressed_opacity, 0, NULL },
		{ _("/Opacity/50%"),		"<control>5",	pressed_opacity, 0, NULL },
		{ _("/Opacity/60%"),		"<control>6",	pressed_opacity, 0, NULL },
		{ _("/Opacity/70%"),		"<control>7",	pressed_opacity, 0, NULL },
		{ _("/Opacity/80%"),		"<control>8",	pressed_opacity, 0, NULL },
		{ _("/Opacity/90%"),		"<control>9",	pressed_opacity, 0, NULL },
		{ _("/Opacity/100%"),		"<control>0",	pressed_opacity, 0, NULL },
		{ _("/Opacity/sep1"),		NULL,		NULL,0, "<Separator>" },
		{ _("/Opacity/-1% Ctrl+ -"),	NULL,		pressed_opacity, 0, NULL },
		{ _("/Opacity/+1% Ctrl+ +"),	NULL,		pressed_opacity, 0, NULL },

		{ _("/Effe_cts"),		NULL,	 	NULL, 0, "<Branch>" },
		{ _("/Effects/tear"),		NULL,	 	NULL, 0, "<Tearoff>" },
		{ _("/Effects/Transform Colour ..."), "<control><shift>C", pressed_brcosa,0, NULL },
		{ _("/Effects/Invert"),		"<control><shift>I", pressed_invert,0, NULL },
		{ _("/Effects/Greyscale"),	"<control>G",	pressed_greyscale,0, NULL },
		{ _("/Effects/Isometric Transformation"), NULL, NULL, 0, "<Branch>" },
		{ _("/Effects/Isometric Transformation/tear"), NULL, NULL, 0, "<Tearoff>" },
		{ _("/Effects/Isometric Transformation/Left Side Down"), NULL, iso_trans, 0, NULL },
		{ _("/Effects/Isometric Transformation/Right Side Down"), NULL, iso_trans, 0, NULL },
		{ _("/Effects/Isometric Transformation/Top Side Right"), NULL, iso_trans, 0, NULL },
		{ _("/Effects/Isometric Transformation/Bottom Side Right"), NULL, iso_trans, 0, NULL },
		{ _("/Effects/sep1"),		NULL,		NULL,0, "<Separator>" },
		{ _("/Effects/Edge Detect"),	NULL,		pressed_edge_detect,0, NULL },
		{ _("/Effects/Sharpen ..."),	NULL,		pressed_sharpen,0, NULL },
		{ _("/Effects/Soften ..."),	NULL,		pressed_soften,0, NULL },
		{ _("/Effects/Blur ..."),	NULL,		pressed_blur,0, NULL },
		{ _("/Effects/Emboss"),		NULL,		pressed_emboss,0, NULL },
		{ _("/Effects/sep2"),		NULL,		NULL,0, "<Separator>" },
		{ _("/Effects/Bacteria ..."),	NULL,		pressed_bacteria, 0, NULL },

		{ _("/_Help"),			NULL,		NULL,0, "<LastBranch>" },
		{ _("/Help/About"),		"F1",		pressed_help,0, NULL }
	};

char
	*item_undo[] = {_("/Edit/Undo"), _("/File/Export Undo Images ..."),
			_("/File/Export Undo Images (reversed) ..."),
			NULL},
	*item_redo[] = {_("/Edit/Redo"),
			NULL},
	*item_need_marquee[] = {_("/Selection/Select None (Esc)"),
			NULL},
	*item_need_selection[] = { _("/Edit/Cut"), _("/Edit/Copy"), _("/Selection/Fill Selection"),
				_("/Selection/Outline Selection"), _("/Selection/Fill Ellipse"),
				_("/Selection/Outline Ellipse"),
			NULL},
	*item_crop[] = { _("/Image/Crop"),
			NULL},
	*item_need_clipboard[] = {_("/Edit/Paste"), _("/Edit/Paste To Centre"),
			_("/Edit/Paste To New Layer"),
			_("/Selection/Flip Horizontally"), _("/Selection/Flip Vertically"),
			_("/Selection/Rotate Clockwise"), _("/Selection/Rotate Anti-Clockwise"),
			_("/Selection/Mask Colour A,B"), _("/Selection/Clear Mask"),
			_("/Selection/Unmask Colour A,B"), _("/Selection/Mask All Colours"),
			NULL},
	*item_help[] = {_("/Help/About"), NULL},
	*item_prefs[] = {_("/Image/Preferences ..."), NULL},
	*item_only_24[] = { _("/Image/Convert To Indexed"), _("/Opacity"), _("/Effects/Edge Detect"),
			_("/Effects/Blur ..."), _("/Effects/Emboss"), _("/Effects/Sharpen ..."),
			_("/Effects/Soften ..."), _("/Palette/Create Quantized (DL1)"),
			_("/Palette/Create Quantized (DL3)"), _("/Palette/Create Quantized (Wu)"),
			NULL },
	*item_only_indexed[] = { _("/Image/Convert To RGB"), _("/Effects/Bacteria ..."),
			_("/Palette/Merge Duplicate Colours"), _("/Palette/Remove Unused Colours"),
			_("/File/Export ASCII Art ..."),
			NULL },
	*item_continuous[] = {_("/Edit/Continuous Painting"), _("/Edit/Opacity Undo Mode"),
			_("/View/Show zoom grid"),
			NULL},
	*item_cline[] = {_("/View/Command Line Window"),
			NULL},
	*item_view[] = {_("/View/View Window"),
			NULL},
	*item_iso[] = {_("/Effects/Isometric Transformation/Left Side Down"),
			_("/Effects/Isometric Transformation/Right Side Down"),
			_("/Effects/Isometric Transformation/Top Side Right"),
			_("/Effects/Isometric Transformation/Bottom Side Right"), NULL},
	*item_layer[] = {_("/View/Layers Window"),
			NULL},
	*item_lasso[] = {_("/Selection/Lasso Selection"), _("/Selection/Lasso Selection Cut"),
			_("/Edit/Cut"), _("/Edit/Copy"),
			_("/Selection/Fill Selection"), _("/Selection/Outline Selection"),
			NULL}
	;

	for ( i=0; i<STATUS_ITEMS; i++ )
	{
		sprintf(txt, "status%iToggle", i);
		status_on[i] = inifile_get_gboolean(txt, TRUE);
	}

	mem_background = inifile_get_gint32("backgroundGrey", 180 );
	mem_undo_limit = inifile_get_gint32("undoMBlimit", 32 );
	mem_nudge = inifile_get_gint32("pixelNudge", 8 );

	accel_group = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR,"<main>", accel_group);
	gtk_item_factory_create_items_ac(item_factory, (sizeof(menu_items)/sizeof((menu_items)[0])),
		menu_items, NULL, 2);

	menu_recent[0] = gtk_item_factory_get_item(item_factory, _("/File/sep2") );
	for ( i=1; i<21; i++ )
	{
		sprintf(txt, _("/File/%i"), i);
		menu_recent[i] = gtk_item_factory_get_widget(item_factory, txt );
	}
	menu_recent[21] = NULL;

	pop_men_dis( item_factory, item_undo, menu_undo );
	pop_men_dis( item_factory, item_redo, menu_redo );
	pop_men_dis( item_factory, item_need_marquee, menu_need_marquee );
	pop_men_dis( item_factory, item_need_selection, menu_need_selection );
	pop_men_dis( item_factory, item_need_clipboard, menu_need_clipboard );
	pop_men_dis( item_factory, item_crop, menu_crop );
	pop_men_dis( item_factory, item_continuous, menu_continuous );
	pop_men_dis( item_factory, item_help, menu_help );
	pop_men_dis( item_factory, item_prefs, menu_prefs );
	pop_men_dis( item_factory, item_only_24, menu_only_24 );
	pop_men_dis( item_factory, item_only_indexed, menu_only_indexed );
	pop_men_dis( item_factory, item_cline, menu_cline );
	pop_men_dis( item_factory, item_view, menu_view );
	pop_men_dis( item_factory, item_iso, menu_iso );
	pop_men_dis( item_factory, item_layer, menu_layer );
	pop_men_dis( item_factory, item_lasso, menu_lasso );

	menu_clip_load[0] = NULL;
	menu_clip_save[0] = NULL;
	for ( i=1; i<=12; i++ )		// Set up load/save clipboard stuff
	{
		snprintf( txt, 60, "%s/%i", _("/Edit/Load Clipboard"), i );
		men_dis_add( gtk_item_factory_get_item(item_factory, txt), menu_clip_load );
		snprintf( txt, 60, "%s/%i", _("/Edit/Save Clipboard"), i );
		men_dis_add( gtk_item_factory_get_item(item_factory, txt), menu_clip_save );
		men_dis_add( gtk_item_factory_get_item(item_factory, txt), menu_need_clipboard );
	}
	menu_opac[0] = NULL;
	for ( i=1; i<=10; i++ )
	{
		snprintf( txt, 60, _("/Opacity/%i0%%"), i );
		men_dis_add( gtk_item_factory_get_item(item_factory, txt), menu_opac );
	}
	men_dis_add( gtk_item_factory_get_item(item_factory, _("/Opacity/-1% Ctrl+ -")), menu_opac );
	men_dis_add( gtk_item_factory_get_item(item_factory, _("/Opacity/+1% Ctrl+ +")), menu_opac );

	mem_continuous = inifile_get_gboolean( "continuousPainting", TRUE );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(menu_continuous[0]), mem_continuous );

	mem_undo_opacity = inifile_get_gboolean( "opacityToggle", TRUE );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(menu_continuous[1]), mem_undo_opacity );

	mem_show_grid = inifile_get_gboolean( "gridToggle", TRUE );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(menu_continuous[2]), mem_show_grid );
	mem_grid_min = inifile_get_gint32("gridMin", 8 );
	mem_grid_rgb[0] = inifile_get_gint32("gridR", 50 );
	mem_grid_rgb[1] = inifile_get_gint32("gridG", 50 );
	mem_grid_rgb[2] = inifile_get_gint32("gridB", 50 );

	main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize(main_window, 100, 100);		// Set minimum width/height
	gtk_window_set_default_size( GTK_WINDOW(main_window),
		inifile_get_gint32("window_w", 630 ), inifile_get_gint32("window_h", 400 ) );
	gtk_widget_set_uposition( main_window,
		inifile_get_gint32("window_x", 0 ), inifile_get_gint32("window_y", 0 ) );
	gtk_window_set_title (GTK_WINDOW (main_window), VERSION );

	vbox_main = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_main);
	gtk_container_add (GTK_CONTAINER (main_window), vbox_main);

	menubar1 = gtk_item_factory_get_widget(item_factory,"<main>");
	gtk_accel_group_lock( accel_group );	// Stop dynamic allocation of accelerators during runtime
	gtk_window_add_accel_group(GTK_WINDOW(main_window), accel_group);

	gtk_widget_show (menubar1);
	gtk_box_pack_start (GTK_BOX (vbox_main), menubar1, FALSE, FALSE, 0);

	hbox_top = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_top);
	gtk_box_pack_start (GTK_BOX (vbox_main), hbox_top, FALSE, FALSE, 0);

///	PREVIEW AREA

	drawing_pat_prev = gtk_drawing_area_new ();
	gtk_widget_set_usize( drawing_pat_prev, PATTERN_WIDTH, PATTERN_HEIGHT );
	gtk_box_pack_start( GTK_BOX(hbox_top), drawing_pat_prev, FALSE, FALSE, 0 );
	gtk_widget_show( drawing_pat_prev );
	gtk_signal_connect_object( GTK_OBJECT(drawing_pat_prev), "expose_event",
		GTK_SIGNAL_FUNC (expose_pattern), GTK_OBJECT(drawing_pat_prev) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_pat_prev), "button_release_event",
		GTK_SIGNAL_FUNC (click_pattern), GTK_OBJECT(drawing_pat_prev) );
	gtk_widget_set_events (drawing_pat_prev, GDK_ALL_EVENTS_MASK);

	drawing_col_prev = gtk_drawing_area_new ();
	gtk_widget_set_usize( drawing_col_prev, PREVIEW_WIDTH, PREVIEW_HEIGHT );
	gtk_box_pack_start( GTK_BOX(hbox_top), drawing_col_prev, FALSE, FALSE, 0 );
	gtk_widget_show( drawing_col_prev );
	gtk_signal_connect_object( GTK_OBJECT(drawing_col_prev), "button_press_event",
		GTK_SIGNAL_FUNC (click_colours), GTK_OBJECT(drawing_col_prev) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_col_prev), "expose_event",
		GTK_SIGNAL_FUNC (expose_preview), GTK_OBJECT(drawing_col_prev) );
	gtk_widget_set_events (drawing_col_prev, GDK_BUTTON_PRESS_MASK);


///	TOOLBAR

	vbox_top = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_top);
	gtk_box_pack_start ( GTK_BOX (hbox_top), vbox_top, FALSE, FALSE, 5 );

// we need to realize the window because we use pixmaps for 
// items on the toolbar in the context of it
	gtk_widget_realize( main_window );

#if GTK_MAJOR_VERSION == 1
	toolbar = gtk_toolbar_new( GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS );
	toolbar2 = gtk_toolbar_new( GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS );
#endif
#if GTK_MAJOR_VERSION == 2
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );
	toolbar2 = gtk_toolbar_new();
	gtk_toolbar_set_style( GTK_TOOLBAR(toolbar2), GTK_TOOLBAR_ICONS );
#endif

	gtk_box_pack_start ( GTK_BOX (vbox_top), toolbar2, FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX (vbox_top), toolbar, FALSE, FALSE, 0 );

	for (i=0; i<TOTAL_CURSORS; i++)
	{
		icon = gdk_bitmap_create_from_data (NULL, xbm_list[i], 20, 20);
		mask = gdk_bitmap_create_from_data (NULL, xbm_mask_list[i], 20, 20);

		if ( i <= TOOL_SHUFFLE || i == TOOL_LINE || i == TOOL_SMUDGE || i == TOOL_CLONE )
			m_cursor[i] = gdk_cursor_new_from_pixmap (icon, mask, &cfg, &cbg, 0, 0);
		else
		{
			if ( i == TOOL_FLOOD)
				m_cursor[i] = gdk_cursor_new_from_pixmap (icon, mask, &cfg, &cbg, 2, 19);
			else
				m_cursor[i] = gdk_cursor_new_from_pixmap (icon, mask, &cfg, &cbg, 10, 10);
		}

		gdk_pixmap_unref( icon );
		gdk_pixmap_unref( mask );
	}
	icon = gdk_bitmap_create_from_data (NULL, xbm_move_bits, 20, 20);
	mask = gdk_bitmap_create_from_data (NULL, xbm_move_mask_bits, 20, 20);
	move_cursor = gdk_cursor_new_from_pixmap (icon, mask, &cfg, &cbg, 10, 10);
	gdk_pixmap_unref( icon );
	gdk_pixmap_unref( mask );

	previous = NULL;
	for (i=0; i<TOTAL_ICONS_TOOLBAR; i++)		// Lower icon toolbar
	{
		icon = gdk_pixmap_create_from_xpm_d ( main_window->window, &mask,
			NULL, icon_list[i] );
		iconw = gtk_pixmap_new ( icon, mask );
		gdk_pixmap_unref( icon );
		gdk_pixmap_unref( mask );

		if ( i<1 || i>8 )
		{
			child_type = GTK_TOOLBAR_CHILD_BUTTON;
			previous = NULL;
		}
		else
		{
			child_type = GTK_TOOLBAR_CHILD_RADIOBUTTON;
			previous = icon_buttons[i-1];
		}
		if ( i == 1 ) previous = NULL;

		icon_buttons[i] = gtk_toolbar_append_element( GTK_TOOLBAR(toolbar),
			child_type, previous, "None", hint_text[i],
			"Private", iconw, GTK_SIGNAL_FUNC(toolbar_icon_event), (gpointer) i);
	}
	men_dis_add( icon_buttons[5], menu_only_24 );		// Smudge - Only RGB images
	men_dis_add( icon_buttons[9], menu_lasso );		// Lasso
	men_dis_add( icon_buttons[10], menu_crop );		// Crop

	for (i=0; i<TOTAL_ICONS_TOOLBAR; i++)		// Upper icon toolbar
	{
		icon = gdk_pixmap_create_from_xpm_d ( main_window->window, &mask,
			NULL, icon_list2[i] );
		iconw = gtk_pixmap_new ( icon, mask );
		gdk_pixmap_unref( icon );
		gdk_pixmap_unref( mask );

		icon_buttons2[i] = gtk_toolbar_append_element( GTK_TOOLBAR(toolbar2),
			GTK_TOOLBAR_CHILD_BUTTON, NULL, "None", hint_text2[i],
			"Private", iconw, GTK_SIGNAL_FUNC(toolbar_icon_event2), (gpointer) i);
	}
	men_dis_add( icon_buttons2[3], menu_need_selection );	// Cut
	men_dis_add( icon_buttons2[3], menu_lasso );		// Cut
	men_dis_add( icon_buttons2[4], menu_need_selection );	// Copy
	men_dis_add( icon_buttons2[4], menu_lasso );		// Copy
	men_dis_add( icon_buttons2[5], menu_need_clipboard );	// Paste
	men_dis_add( icon_buttons2[6], menu_undo );		// Undo
	men_dis_add( icon_buttons2[7], menu_redo );		// Undo

	men_dis_add( icon_buttons2[8], menu_need_selection );	// Ellipse outline
	men_dis_add( icon_buttons2[9], menu_need_selection );	// Ellipse

	men_dis_add( icon_buttons2[10], menu_need_selection );	// Outline
	men_dis_add( icon_buttons2[11], menu_need_selection );	// Fill
	men_dis_add( icon_buttons2[10], menu_lasso );		// Outline
	men_dis_add( icon_buttons2[11], menu_lasso );		// Fill

	men_dis_add( icon_buttons2[12], menu_need_clipboard );	// Flip sel V
	men_dis_add( icon_buttons2[13], menu_need_clipboard );	// Flip sel H
	men_dis_add( icon_buttons2[14], menu_need_clipboard );	// Rot sel clock
	men_dis_add( icon_buttons2[15], menu_need_clipboard );	// Rot sel anti

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON]), TRUE );
	gtk_widget_show ( toolbar );
	gtk_widget_show ( toolbar2 );

///	Table for size & flow spin buttons

	table_tools = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table_tools);
	gtk_box_pack_start (GTK_BOX (hbox_top), table_tools, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table_tools), 5);

	add_to_table( _("Size"), table_tools, 0, 0, 5, GTK_JUSTIFY_LEFT, 0, 0.5 );
	add_to_table( _("Flow"), table_tools, 1, 0, 5, GTK_JUSTIFY_LEFT, 0, 0.5 );

	spin_to_table( table_tools, &spinbutton_size, 0, 1, 0, 1, 1, 200 );
	GTK_WIDGET_UNSET_FLAGS (spinbutton_size, GTK_CAN_FOCUS);
	gtk_signal_connect_object( GTK_OBJECT(spinbutton_size), "changed",
		GTK_SIGNAL_FUNC (canvas_enter), GTK_OBJECT(spinbutton_size) );

	spin_to_table( table_tools, &spinbutton_spray, 1, 1, 0, 1, 1, 200 );
	GTK_WIDGET_UNSET_FLAGS (spinbutton_spray, GTK_CAN_FOCUS);
	gtk_signal_connect_object( GTK_OBJECT(spinbutton_spray), "changed",
		GTK_SIGNAL_FUNC (canvas_enter), GTK_OBJECT(spinbutton_spray) );



///	COLOUR LABELS
	table_RGB = gtk_table_new (2, 1, FALSE);
	gtk_widget_show (table_RGB);
	gtk_box_pack_start (GTK_BOX (hbox_top), table_RGB, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table_RGB), 5);

	label_A = add_to_table( "-", table_RGB, 0, 0, 5, GTK_JUSTIFY_LEFT, 0, 0.5 );
	label_B = add_to_table( "-", table_RGB, 1, 0, 5, GTK_JUSTIFY_LEFT, 0, 0.5 );



///	PALETTE

	hbox_bottom = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_bottom);
	gtk_box_pack_start (GTK_BOX (vbox_main), hbox_bottom, TRUE, TRUE, 0);

	scrolledwindow_palette = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow_palette);
	gtk_box_pack_start (GTK_BOX (hbox_bottom), scrolledwindow_palette, FALSE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow_palette),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	viewport_palette = gtk_viewport_new (NULL, NULL);

	gtk_widget_set_usize( viewport_palette, PALETTE_WIDTH, 64 );
	gtk_widget_show (viewport_palette);
	gtk_container_add (GTK_CONTAINER (scrolledwindow_palette), viewport_palette);

	drawing_palette = gtk_drawing_area_new ();
	gtk_widget_set_usize( drawing_palette, PALETTE_WIDTH, 64 );
	gtk_container_add (GTK_CONTAINER (viewport_palette), drawing_palette);
	gtk_widget_show( drawing_palette );
	gtk_signal_connect_object( GTK_OBJECT(drawing_palette), "expose_event",
		GTK_SIGNAL_FUNC (expose_palette), GTK_OBJECT(drawing_palette) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_palette), "button_press_event",
		GTK_SIGNAL_FUNC (click_palette), GTK_OBJECT(drawing_palette) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_palette), "motion_notify_event",
		GTK_SIGNAL_FUNC (motion_palette), GTK_OBJECT(drawing_palette) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_palette), "button_release_event",
		GTK_SIGNAL_FUNC (release_palette), GTK_OBJECT(drawing_palette) );

	gtk_widget_set_events (drawing_palette, GDK_ALL_EVENTS_MASK);


	vbox_right = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_right);
	gtk_box_pack_start (GTK_BOX (hbox_bottom), vbox_right, TRUE, TRUE, 0);


///	DRAWING AREA

	drawing_canvas = gtk_drawing_area_new ();
	gtk_widget_set_usize( drawing_canvas, 48, 48 );
	gtk_widget_show( drawing_canvas );

	scrolledwindow_canvas = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow_canvas);
	gtk_box_pack_start (GTK_BOX (vbox_right), scrolledwindow_canvas, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow_canvas),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_signal_connect_object(
		GTK_OBJECT(
			GTK_HSCROLLBAR(
				GTK_SCROLLED_WINDOW(scrolledwindow_canvas)->hscrollbar
			)->scrollbar.range.adjustment
		),
		"value_changed", GTK_SIGNAL_FUNC (main_scroll_changed), NULL );
	gtk_signal_connect_object(
		GTK_OBJECT(
			GTK_VSCROLLBAR(
				GTK_SCROLLED_WINDOW(scrolledwindow_canvas)->vscrollbar
			)->scrollbar.range.adjustment
		),
		"value_changed", GTK_SIGNAL_FUNC (main_scroll_changed), NULL );

	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(scrolledwindow_canvas),
		drawing_canvas);

	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "expose_event",
		GTK_SIGNAL_FUNC (expose_canvas), GTK_OBJECT(drawing_canvas) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "button_press_event",
		GTK_SIGNAL_FUNC (canvas_button), NULL );
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "motion_notify_event",
		GTK_SIGNAL_FUNC (canvas_motion), NULL );
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "enter_notify_event",
		GTK_SIGNAL_FUNC (canvas_enter), GTK_OBJECT(drawing_canvas) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "leave_notify_event",
		GTK_SIGNAL_FUNC (canvas_left), GTK_OBJECT(drawing_canvas) );
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "button_release_event",
		GTK_SIGNAL_FUNC (canvas_release), GTK_OBJECT(drawing_canvas) );
#if GTK_MAJOR_VERSION == 2
	gtk_signal_connect_object( GTK_OBJECT(drawing_canvas), "scroll_event",
		GTK_SIGNAL_FUNC (canvas_scroll_gtk2), GTK_OBJECT(drawing_canvas) );
#endif

	gtk_widget_set_events (drawing_canvas, GDK_ALL_EVENTS_MASK);
	gtk_widget_set_extension_events (drawing_canvas, GDK_EXTENSION_EVENTS_CURSOR);

////	STATUS BAR

	hbox_bar = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_bar);
	gtk_box_pack_start (GTK_BOX (vbox_right), hbox_bar, FALSE, FALSE, 0);

	label_bar1 = gtk_label_new("");
	gtk_widget_show (label_bar1);
	gtk_box_pack_start (GTK_BOX (hbox_bar), label_bar1, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (label_bar1), 0, 0);

	label_bar2 = gtk_label_new("");
	gtk_widget_show (label_bar2);
	gtk_box_pack_start (GTK_BOX (hbox_bar), label_bar2, FALSE, FALSE, 0);
	if ( status_on[2] ) gtk_widget_set_usize(label_bar2, 90, -2);
	gtk_misc_set_alignment (GTK_MISC (label_bar2), 0.5, 0);

	label_bar3 = gtk_label_new("");		// Pixel info
	gtk_widget_show (label_bar3);
	gtk_box_pack_start (GTK_BOX (hbox_bar), label_bar3, FALSE, FALSE, 0);
	if ( status_on[3] ) gtk_widget_set_usize(label_bar3, 160, -2);
	gtk_misc_set_alignment (GTK_MISC (label_bar3), 0, 0);

	label_bar4 = gtk_label_new("");
	gtk_widget_show (label_bar4);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar4, FALSE, FALSE, 0);

	label_bar9 = gtk_label_new("");
	gtk_widget_show (label_bar9);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar9, FALSE, FALSE, 0);

	label_bar6 = gtk_label_new("");
	gtk_widget_show (label_bar6);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar6, FALSE, FALSE, 0);

	label_bar8 = gtk_label_new("");
	gtk_widget_show (label_bar8);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar8, FALSE, FALSE, 0);

	if ( status_on[7] ) label_bar7 = gtk_label_new("0+0");
	else label_bar7 = gtk_label_new("");
	gtk_widget_show (label_bar7);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar7, FALSE, FALSE, 0);
	if ( status_on[7] ) gtk_widget_set_usize(label_bar7, 50, -2);

	label_bar5 = gtk_label_new("");		// Selection geometry
	gtk_widget_show (label_bar5);
	gtk_box_pack_end (GTK_BOX (hbox_bar), label_bar5, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (label_bar5), 0, 0);

/////////	End of main window widget setup

	gtk_signal_connect_object (GTK_OBJECT (main_window), "delete_event",
		GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_signal_connect_object (GTK_OBJECT (main_window), "key_press_event",
		GTK_SIGNAL_FUNC (handle_keypress), NULL);

//	gtk_container_set_border_width (GTK_CONTAINER (main_window), 2);

	men_item_state( menu_undo, FALSE );
	men_item_state( menu_redo, FALSE );
	men_item_state( menu_need_marquee, FALSE );
	men_item_state( menu_need_selection, FALSE );
	men_item_state( menu_need_clipboard, FALSE );

	recent_files = inifile_get_gint32( "recentFiles", 10 );
	mtMIN( recent_files, recent_files, 20 )
	mtMAX( recent_files, recent_files, 0 )

	update_recent_files();

	main_hidden[0] = hbox_bar;			// Hide status bar
	main_hidden[1] = scrolledwindow_palette;	// Hide palette area
	main_hidden[2] = hbox_top;			// Hide top area
	main_hidden[3] = menubar1;			// Hide menu bar

	if (viewer_mode) toggle_view(NULL, NULL);

	gtk_widget_show (main_window);

	icon_pix = gdk_pixmap_create_from_xpm_d( main_window->window, NULL, NULL, icon_xpm );
	gdk_window_set_icon( main_window->window, NULL, icon_pix, NULL );

	gdk_rgb_init();
	set_cursor();
	show_paste = inifile_get_gboolean( "pasteToggle", TRUE );
	mem_jpeg_quality = inifile_get_gint32( "jpegQuality", 85 );
	q_quit = inifile_get_gboolean( "quitToggle", TRUE );
	init_status_bar();

	snprintf( mem_clip_file[1], 250, "%s/.clipboard", get_home_directory() );
	snprintf( mem_clip_file[0], 250, "%s", inifile_get( "clipFilename", mem_clip_file[1] ) );

	if (files_passed > 1)
		pressed_cline(NULL, NULL);
	else
		men_item_state(menu_cline, FALSE);

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );

	dash_gc = gdk_gc_new(drawing_canvas->window);		// Set up gc for polygon marquee
	gdk_gc_set_background(dash_gc, &cbg);
	gdk_gc_set_foreground(dash_gc, &cfg);
//	gdk_gc_set_line_attributes( dash_gc, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_ROUND, GDK_JOIN_ROUND);
	gdk_gc_set_line_attributes( dash_gc, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);

	init_tablet();						// Set up the tablet
}

void spot_undo()
{
	pen_down = 0;			// Ensure previous tool action is treated separately
	mem_undo_next();		// Do memory stuff for undo
	update_menus();			// Update menu undo issues
	pen_down = 0;			// Ensure next tool action is treated separately
}

#ifdef U_NLS
void setup_language()			// Change language
{
	char *txt = inifile_get( "languageSETTING", "system" ), txt2[64];

	if ( strcmp( "system", txt ) != 0 )
	{
		snprintf( txt2, 60, "LANGUAGE=%s", txt );
		putenv( txt2 );
		snprintf( txt2, 60, "LANG=%s", txt );
		putenv( txt2 );
		snprintf( txt2, 60, "LC_ALL=%s", txt );
		putenv( txt2 );
	}
#if GTK_MAJOR_VERSION == 1
	else	txt="";

	setlocale(LC_ALL, txt);
#endif
#if GTK_MAJOR_VERSION == 2
	gtk_set_locale();	// GTK+1 hates this - it really slows things down
#endif
}
#endif

void update_titlebar()		// Update filename in titlebar
{
	char txt[300], txt2[600], *extra = "-";

#if GTK_MAJOR_VERSION == 2
	cleanse_txt( txt2, mem_filename );		// Clean up non ASCII chars
#else
	strcpy( txt2, mem_filename );
#endif

	if ( mem_changed == 1 ) extra = _("(Modified)");

	snprintf( txt, 290, "%s %s %s", VERSION, extra, txt2 );

	gtk_window_set_title (GTK_WINDOW (main_window), txt );
}

void notify_changed()		// Image/palette has just changed - update vars as needed
{
	if ( mem_changed != 1 )
	{
		mem_changed = 1;
		update_titlebar();
	}
}

void notify_unchanged()		// Image/palette has just been unchanged (saved) - update vars as needed
{
	if ( mem_changed != 0 )
	{
		mem_changed = 0;
		update_titlebar();
	}
}

