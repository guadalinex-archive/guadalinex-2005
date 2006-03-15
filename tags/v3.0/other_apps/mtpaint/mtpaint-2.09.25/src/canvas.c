/*	canvas.c
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

#include <math.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "global.h"

#include "mainwindow.h"
#include "otherwindow.h"
#include "mygtk.h"
#include "inifile.h"
#include "canvas.h"
#include "png.h"
#include "memory.h"
#include "quantizer.h"
#include "viewer.h"
#include "layer.h"
#include "polygon.h"
#include "wu.h"
#include "prefs.h"

GdkWindow *the_canvas = NULL;		// Pointer to the canvas we will be drawing on

float can_zoom = 1;			// Zoom factor 1..MAX_ZOOM
int zoom_flag = 0;
int fs_type = 0;
int perim_status = 0, perim_x = 0, perim_y = 0, perim_s = 2;		// Tool perimeter
int marq_status = MARQUEE_NONE,
	marq_x1 = 0, marq_y1 = 0, marq_x2 = 0, marq_y2 = 0;		// Selection marquee
int marq_drag_x = 0, marq_drag_y = 0;					// Marquee dragging offset
int line_status = LINE_NONE,
	line_x1 = 0, line_y1 = 0, line_x2 = 0, line_y2 = 0;		// Line tool
int poly_status = POLY_NONE;						// Polygon selection tool
int clone_x, clone_y;							// Clone offsets

int recent_files;					// Current recent files setting

gboolean show_paste;					// Show contents of clipboard while pasting
gboolean col_reverse = FALSE;				// Painting with right button
gboolean text_paste = FALSE;				// Are we pasting text?

void commit_paste( gboolean undo )
{
	int fx, fy, fw, fh, fx2, fy2;		// Screen coords
	int mx = 0, my = 0;			// Mem coords
	int i, j, k, offs;
	unsigned char pixel[3], mask;
	unsigned char *old_image = NULL;	// Used for <100% opacity

	if ( marq_x1 < 0 ) mx = -marq_x1;
	if ( marq_y1 < 0 ) my = -marq_y1;

	mtMAX( fx, marq_x1, 0 )
	mtMAX( fy, marq_y1, 0 )
	mtMIN( fx2, marq_x2, mem_width-1 )
	mtMIN( fy2, marq_y2, mem_height-1 )

	fw = fx2 - fx + 1;
	fh = fy2 - fy + 1;

	if ( undo ) mem_undo_next();		// Do memory stuff for undo
	update_menus();				// Update menu undo issues

	if ( mem_image_bpp == 1 )
	{
		for ( j=0; j<fh; j++ )
		{
			if ( mem_clip_mask == NULL )
			{
				for ( i=0; i<fw; i++ )
				{
					pixel[0] = mem_clipboard[ i+mx + mem_clip_w*(j+my) ];
					PUT_PIXEL_C( fx+i, fy+j, pixel[0] )
				}
			}
			else
			{
				for ( i=0; i<fw; i++ )
				{
					pixel[0] = mem_clipboard[ i+mx + mem_clip_w*(j+my) ];
					mask = mem_clip_mask[ i+mx + mem_clip_w*(j+my) ];
					if ( mask == 0 ) PUT_PIXEL_C( fx+i, fy+j, pixel[0] )
				}
			}
		}
	}
	if ( mem_image_bpp == 3 )
	{
		if ( mem_undo_opacity )
			old_image = mem_undo_previous();
		else
			old_image = mem_image;

		for ( j=0; j<fh; j++ )
		{
			if ( mem_clip_mask == NULL )
			{
			    for ( i=0; i<fw; i++ )
			    {
				offs = 3*(fx+i + mem_width*(fy+j));
				k = MEM_2_INT(mem_image, offs);
				if ( !mem_protected_RGB(k) )
				{
				 for ( k=0; k<3; k++ )
				 {
				  pixel[0] = mem_clipboard[ k + 3*(i+mx + mem_clip_w*(j+my)) ];
				  pixel[1] = old_image[ k + offs ];
				  pixel[2] = (pixel[0]*tool_opacity + pixel[1]*(100-tool_opacity)+50) / 100;
				  mem_image[ k + offs ] = pixel[2];
				 }
				}
			    }
			}
			else
			{
			    for ( i=0; i<fw; i++ )
			    {
				offs = 3*(fx+i + mem_width*(fy+j));
				k = MEM_2_INT(mem_image, offs);
				if ( !mem_protected_RGB(k) )
				{
				  mask = mem_clip_mask[ i+mx + mem_clip_w*(j+my) ];
				  if ( mask != 255 ) for ( k=0; k<3; k++ )
				  {
				    pixel[0] = mem_clipboard[ k + 3*(i+mx + mem_clip_w*(j+my)) ];
				    pixel[1] = old_image[ k + offs ];
				    pixel[2] = ((255-mask) * pixel[0] + mask * pixel[1] ) / 255;
				    mem_image[ k + offs ] = ( pixel[2] * tool_opacity +
				    	pixel[1] * (100-tool_opacity)
				    	) / 100;

				  }
				}
			    }
			}
		}
	}
	vw_update_area( fx, fy, fw, fh );
	gtk_widget_queue_draw_area( drawing_canvas, fx*can_zoom, fy*can_zoom, fw*can_zoom, fh*can_zoom );
}

void paste_prepare()
{
	poly_status = POLY_NONE;
	poly_points = 0;
	if ( tool_type != TOOL_SELECT && tool_type != TOOL_POLYGON )
	{
		perim_status = 0;
		clear_perim();
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
	}
	else
	{
		if ( marq_status != MARQUEE_NONE ) paint_marquee(0, marq_x1, marq_y1);
	}
}

void iso_trans( GtkMenuItem *menu_item, gpointer user_data )
{
	int i, j = 0;

	for ( i=0; i<4; i++ ) if ( menu_iso[i] == GTK_WIDGET(menu_item) ) j=i;

	i = mem_isometrics(j);

	if ( i==0 ) canvas_undo_chores();
	else
	{
		if ( i==-666 ) alert_box( _("Error"), _("The image is too large to transform."),
					_("OK"), NULL, NULL );
		else memory_errors(i);
	}
}

void create_pal_quantized(int dl)
{
	int i = 0;
	unsigned char newpal[3][256];

	pen_down = 0;
	mem_undo_next();
	pen_down = 0;

	if ( dl==1 )
		i = dl1quant(mem_image, mem_width, mem_height, mem_cols, newpal);
	if ( dl==3 )
		i = dl3quant(mem_image, mem_width, mem_height, mem_cols, newpal);
	if ( dl==5 )
		i = wu_quant(mem_image, mem_width, mem_height, mem_cols, newpal);

	if ( i!=0 ) memory_errors(i);
	else
	{
		for ( i=0; i<mem_cols; i++ )
		{
			mem_pal[i].red = newpal[0][i];
			mem_pal[i].green = newpal[1][i];
			mem_pal[i].blue = newpal[2][i];
		}

		update_menus();
		init_pal();
	}
}

void pressed_create_dl1( GtkMenuItem *menu_item, gpointer user_data )
{	create_pal_quantized(1);	}

void pressed_create_dl3( GtkMenuItem *menu_item, gpointer user_data )
{	create_pal_quantized(3);	}

void pressed_create_wu( GtkMenuItem *menu_item, gpointer user_data )
{	create_pal_quantized(5);	}

void pressed_invert( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();

	mem_invert();

	init_pal();
	update_all_views();
	gtk_widget_queue_draw( drawing_col_prev );
}

void pressed_edge_detect( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	do_effect(0, 0);
	update_all_views();
}

void pressed_blur( GtkMenuItem *menu_item, gpointer user_data )
{	bac_form(1); }

void pressed_sharpen( GtkMenuItem *menu_item, gpointer user_data )
{	bac_form(2); }

void pressed_soften( GtkMenuItem *menu_item, gpointer user_data )
{	bac_form(3); }

void pressed_emboss( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	do_effect(2, 0);
	update_all_views();
}

void pressed_convert_rgb( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	i = mem_convert_rgb();

	if ( i!=0 ) memory_errors(i);
	else
	{
		if ( tool_type == TOOL_SELECT && marq_status >= MARQUEE_PASTE )
			pressed_select_none( NULL, NULL );
				// If the user is pasting, lose it!

		update_menus();
		init_pal();
		gtk_widget_queue_draw( drawing_canvas );
		gtk_widget_queue_draw( drawing_col_prev );
	}
}

void pressed_greyscale( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();

	mem_greyscale();

	init_pal();
	update_all_views();
	gtk_widget_queue_draw( drawing_col_prev );
}

void rot_im(int dir)
{
	if ( mem_image_rot(dir) == 0 )
	{
		check_marquee();
		canvas_undo_chores();
	}
	else alert_box( _("Error"), _("Not enough memory to rotate image"), _("OK"), NULL, NULL );
}

void pressed_rotate_image_clock( GtkMenuItem *menu_item, gpointer user_data )
{	rot_im(0);	}

void pressed_rotate_image_anti( GtkMenuItem *menu_item, gpointer user_data )
{	rot_im(1);	}

void rot_sel(int dir)
{
	if ( mem_sel_rot(dir) == 0 )
	{
		check_marquee();
		gtk_widget_queue_draw( drawing_canvas );
	}
	else	alert_box( _("Error"), _("Not enough memory to rotate clipboard"), _("OK"), NULL, NULL );
}

void pressed_rotate_sel_clock( GtkMenuItem *menu_item, gpointer user_data )
{	rot_sel(0);	}

void pressed_rotate_sel_anti( GtkMenuItem *menu_item, gpointer user_data )
{	rot_sel(1);	}

void pressed_rotate_free( GtkMenuItem *menu_item, gpointer user_data )
{	bac_form(4);	}


void mask_ab(int v)
{
	int i;

	if ( mem_clip_mask == NULL )
	{
		i = mem_clip_mask_init(v);
		if ( i != 0 )
		{
			memory_errors(1);	// Not enough memory
			return;
		}
	}
	mem_clip_mask_set(255-v);
	gtk_widget_queue_draw( drawing_canvas );
}

void pressed_clip_unmask( GtkMenuItem *menu_item, gpointer user_data )
{	mask_ab(255);	}

void pressed_clip_mask( GtkMenuItem *menu_item, gpointer user_data )
{	mask_ab(0);	}

void pressed_clip_mask_all( GtkMenuItem *menu_item, gpointer user_data )
{
	int i;

	i = mem_clip_mask_init(255);
	if ( i != 0 )
	{
		memory_errors(1);	// Not enough memory
		return;
	}
	gtk_widget_queue_draw( drawing_canvas );
}

void pressed_clip_mask_clear( GtkMenuItem *menu_item, gpointer user_data )
{
	if ( mem_clip_mask != NULL )
	{
		mem_clip_mask_clear();
		gtk_widget_queue_draw( drawing_canvas );
	}
}

void pressed_flip_image_v( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	mem_flip_v( mem_image, mem_width, mem_height, mem_image_bpp );
	update_all_views();
}

void pressed_flip_image_h( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	mem_flip_h( mem_image, mem_width, mem_height, mem_image_bpp );
	update_all_views();
}

void pressed_flip_sel_v( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_flip_v( mem_clipboard, mem_clip_w, mem_clip_h, mem_image_bpp );
	if ( mem_clip_mask != NULL ) mem_flip_v( mem_clip_mask, mem_clip_w, mem_clip_h, 1 );
	gtk_widget_queue_draw( drawing_canvas );
}

void pressed_flip_sel_h( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_flip_h( mem_clipboard, mem_clip_w, mem_clip_h, mem_image_bpp );
	if ( mem_clip_mask != NULL ) mem_flip_h( mem_clip_mask, mem_clip_w, mem_clip_h, 1 );
	gtk_widget_queue_draw( drawing_canvas );
}

void paste_init()
{
	marq_status = MARQUEE_PASTE;
	cursor_corner = -1;
	update_sel_bar();
	update_menus();
	gtk_widget_queue_draw( drawing_canvas );
}

void pressed_paste( GtkMenuItem *menu_item, gpointer user_data )
{
	paste_prepare();
	marq_x1 = mem_clip_x;
	marq_y1 = mem_clip_y;
	marq_x2 = mem_clip_x + mem_clip_w - 1;
	marq_y2 = mem_clip_y + mem_clip_h - 1;
	paste_init();
}

void pressed_paste_centre( GtkMenuItem *menu_item, gpointer user_data )
{
	int canz = can_zoom;
	GtkAdjustment *hori, *vert;

	if ( canz<1 ) canz = 1;

	hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
	vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

	if ( hori->page_size > mem_width*can_zoom ) mem_icx = 0.5;
	else mem_icx = ( hori->value + hori->page_size/2 ) / (mem_width*can_zoom);

	if ( vert->page_size > mem_height*can_zoom ) mem_icy = 0.5;
	else mem_icy = ( vert->value + vert->page_size/2 ) / (mem_height*can_zoom);

	paste_prepare();
	align_size( can_zoom );
	marq_x1 = mem_width * mem_icx - mem_clip_w/2;
	marq_y1 = mem_height * mem_icy - mem_clip_h/2;
	marq_x2 = marq_x1 + mem_clip_w - 1;
	marq_y2 = marq_y1 + mem_clip_h - 1;
	paste_init();
}

void do_the_copy(int op)
{
	int x1 = marq_x1, y1 = marq_y1;
	int x2 = marq_x2, y2 = marq_y2;
	int x, y, w, h;
	int i, j;

	mtMIN( x, x1, x2 )
	mtMIN( y, y1, y2 )

	w = x1 - x2;
	h = y1 - y2;

	if ( w < 0 ) w = -w;
	if ( h < 0 ) h = -h;

	w++; h++;

	if ( op == 1 )		// COPY
	{
		if ( mem_clipboard != NULL ) free( mem_clipboard );	// Lose old clipboard
		mem_clip_mask_clear();					// Lose old clipboard mask
		mem_clipboard = malloc( w * h * mem_image_bpp );
		text_paste = FALSE;
	
		if ( mem_clipboard == NULL )
		{
			alert_box( _("Error"), _("Not enough memory to create clipboard"),
					_("OK"), NULL, NULL );
		}
		else
		{
			mem_clip_bpp = mem_image_bpp;
			mem_clip_x = x;
			mem_clip_y = y;
			mem_clip_w = w;
			mem_clip_h = h;
			if ( mem_image_bpp == 1 )
				for ( j=0; j<h; j++ )
					for ( i=0; i<w; i++ )
						mem_clipboard[ i + w*j ] =
							mem_image[ x+i + mem_width*(y+j) ];
			if ( mem_image_bpp == 3 )
				for ( j=0; j<h; j++ )
					for ( i=0; i<w; i++ )
					{
						mem_clipboard[ 3*(i + w*j) ] =
							mem_image[ 3*(x+i + mem_width*(y+j)) ];
						mem_clipboard[ 1 + 3*(i + w*j) ] =
							mem_image[ 1 + 3*(x+i + mem_width*(y+j)) ];
						mem_clipboard[ 2 + 3*(i + w*j) ] =
							mem_image[ 2 + 3*(x+i + mem_width*(y+j)) ];
					}
		}
	}
	if ( op == 2 )		// CLEAR area
	{
		f_rectangle( x, y, w, h );
	}
	if ( op == 3 )		// Remember new coords for copy while pasting
	{
		mem_clip_x = x;
		mem_clip_y = y;
	}

	update_menus();
}

void pressed_outline_rectangle( GtkMenuItem *menu_item, gpointer user_data )
{
	int x, y, w, h, x2, y2;

	spot_undo();

	if ( tool_type == TOOL_POLYGON )
	{
		poly_outline();
	}
	else
	{
		mtMIN( x, marq_x1, marq_x2 )
		mtMIN( y, marq_y1, marq_y2 )
		mtMAX( x2, marq_x1, marq_x2 )
		mtMAX( y2, marq_y1, marq_y2 )
		w = abs(marq_x1 - marq_x2) + 1;
		h = abs(marq_y1 - marq_y2) + 1;

		if ( 2*tool_size >= w || 2*tool_size >= h )
			f_rectangle( x, y, w, h );
		else
		{
			f_rectangle( x, y, w, tool_size );				// TOP
			f_rectangle( x, y + tool_size, tool_size, h - 2*tool_size );	// LEFT
			f_rectangle( x, y2 - tool_size + 1, w, tool_size );		// BOTTOM
			f_rectangle( x2 - tool_size + 1,
				y + tool_size, tool_size, h - 2*tool_size );		// RIGHT
		}
	}

	update_all_views();
}

void pressed_fill_ellipse( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	f_ellipse( marq_x1, marq_y1, marq_x2, marq_y2 );
	update_all_views();
}

void pressed_outline_ellipse( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	o_ellipse( marq_x1, marq_y1, marq_x2, marq_y2, tool_size );
	update_all_views();
}

void pressed_fill_rectangle( GtkMenuItem *menu_item, gpointer user_data )
{
	spot_undo();
	if ( tool_type == TOOL_SELECT ) do_the_copy(2);
	if ( tool_type == TOOL_POLYGON ) poly_paint();
	update_all_views();
}

void pressed_cut( GtkMenuItem *menu_item, gpointer user_data )
{			// Copy current selection to clipboard and then fill area with current pattern
	do_the_copy(1);
	spot_undo();
	if ( tool_type == TOOL_SELECT ) do_the_copy(2);
	if ( tool_type == TOOL_POLYGON )
	{
		poly_mask();
		poly_paint();
	}

	update_all_views();
}

void pressed_lasso( GtkMenuItem *menu_item, gpointer user_data )
{
	do_the_copy(1);
	if ( mem_clipboard == NULL ) return;		// No memory so bail out
	poly_mask();
	poly_lasso();
	pressed_paste_centre( NULL, NULL );
}

void pressed_lasso_cut( GtkMenuItem *menu_item, gpointer user_data )
{
	pressed_lasso( menu_item, user_data );
	if ( mem_clipboard == NULL ) return;		// No memory so bail out
	spot_undo();
	poly_lasso_cut();
}

void pressed_copy( GtkMenuItem *menu_item, gpointer user_data )
{			// Copy current selection to clipboard
	if ( tool_type == TOOL_POLYGON )
	{
		do_the_copy(1);
		poly_mask();
	}
	if ( tool_type == TOOL_SELECT )
	{
		if ( marq_status >= MARQUEE_PASTE ) do_the_copy(3);
		else do_the_copy(1);
	}
}

void update_menus()			// Update edit/undo menu
{
	char txt[32];

	sprintf(txt, "%i+%i", mem_undo_done, mem_undo_redo);

	if ( status_on[7] ) gtk_label_set_text( GTK_LABEL(label_bar7), txt );

	if ( mem_image_bpp == 1 )
	{
		men_item_state( menu_only_indexed, TRUE );
		men_item_state( menu_only_24, FALSE );
	}
	if ( mem_image_bpp == 3 )
	{
		men_item_state( menu_only_indexed, FALSE );
		men_item_state( menu_only_24, TRUE );
	}

	if ( marq_status == MARQUEE_NONE )
	{
		men_item_state( menu_need_selection, FALSE );
		men_item_state( menu_crop, FALSE );
		if ( poly_status == POLY_DONE )
		{
			men_item_state( menu_lasso, TRUE );
			men_item_state( menu_need_marquee, TRUE );
		}
		else
		{
			men_item_state( menu_lasso, FALSE );
			men_item_state( menu_need_marquee, FALSE );
		}
	}
	else
	{
		if ( poly_status != POLY_DONE ) men_item_state( menu_lasso, FALSE );

		men_item_state( menu_need_marquee, TRUE );

		if ( marq_status >= MARQUEE_PASTE )	// If we are pasting disallow copy/cut/crop
		{
			men_item_state( menu_need_selection, FALSE );
			men_item_state( menu_crop, FALSE );
		}
		else	men_item_state( menu_need_selection, TRUE );

		if ( marq_status <= MARQUEE_DONE )
		{
			if ( (marq_x1 - marq_x2)*(marq_x1 - marq_x2) < (mem_width-1)*(mem_width-1) ||
				(marq_y1 - marq_y2)*(marq_y1 - marq_y2) < (mem_height-1)*(mem_height-1) )
					men_item_state( menu_crop, TRUE );
				// Only offer the crop option if the user hasn't selected everything
			else men_item_state( menu_crop, FALSE );
		}
		else men_item_state( menu_crop, FALSE );
	}

	if ( mem_clipboard == NULL ) men_item_state( menu_need_clipboard, FALSE );
	else
	{
		if ( mem_clip_bpp == mem_image_bpp ) men_item_state( menu_need_clipboard, TRUE );
		else men_item_state( menu_need_clipboard, FALSE );
			// Only allow pasting if the image is the same type as the clipboard
	}

	if ( mem_undo_done == 0 ) men_item_state( menu_undo, FALSE );
	else men_item_state( menu_undo, TRUE );

	if ( mem_undo_redo == 0 ) men_item_state( menu_redo, FALSE );
	else  men_item_state( menu_redo, TRUE );
}

void canvas_undo_chores()
{
	gtk_widget_set_usize( drawing_canvas, mem_width*can_zoom, mem_height*can_zoom );
	update_all_views();				// redraw canvas widget
	update_menus();
	init_pal();
	gtk_widget_queue_draw( drawing_col_prev );
}

void check_undo_paste_bpp()
{
	if ( marq_status >= MARQUEE_PASTE && mem_image_bpp != mem_clip_bpp)
		pressed_select_none( NULL, NULL );

	if ( tool_type == TOOL_SMUDGE && mem_image_bpp == 1 )
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
		// User is smudging and undo/redo to an indexed image - reset tool
}

void main_undo( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_undo_backward();
	check_undo_paste_bpp();
	canvas_undo_chores();
}

void main_redo( GtkMenuItem *menu_item, gpointer user_data )
{
	mem_undo_forward();
	check_undo_paste_bpp();
	canvas_undo_chores();
}

int save_pal(char *file_name)			// Save the current palette to a file
{
	FILE *fp;
	int i;

	if ((fp = fopen(file_name, "w")) == NULL) return -1;

	fprintf(fp, "%i\n", mem_cols);

	for ( i=0; i<mem_cols; i++ )
		fprintf(fp, "%i,%i,%i\n", mem_pal[i].red, mem_pal[i].green, mem_pal[i].blue );

	fclose( fp );

	return 0;
}

int load_pal(char *file_name)			// Load palette file
{
	FILE *fp;
	char input[32];
	int i, r, g, b;

	int new_mem_cols = -1;
	png_color new_mem_pal[256];

	if ((fp = fopen(file_name, "r")) == NULL) return -1;

	if ( get_next_line(input, 30, fp) != 0 )
	{
		fclose( fp );
		return -1;
	}
	else
	{
		if ( strlen(input) > 4 )			// Only tolerate the correct length
		{
printf("Failed - first line length must be less than 4 chars\n");
			fclose( fp );
			return -1;
		}
		sscanf(input, "%i", &new_mem_cols);
		if ( new_mem_cols > 256 || new_mem_cols < 2 )	// Only tolerate the correct range
		{
printf("Failed - colours must be <=256 or >=2, not %i\n", new_mem_cols);
			fclose( fp );
			return -1;
		}
	}

	for ( i=0; i<new_mem_cols; i++ )
	{
		if ( get_next_line(input, 30, fp) != 0 )
		{
printf("Failed - line %i is > 30 chars\n", i);
			fclose( fp );
			return -1;
		}
		sscanf(input, "%i,%i,%i\n", &r, &g, &b );
		if ( r>=0 && r<=255 && g>=0 && g<=255 && b>=0 && b<=255 )
		{
			new_mem_pal[i].red = r;
			new_mem_pal[i].green = g;
			new_mem_pal[i].blue = b;
		}
		else
		{
printf("Failed - line %i has invalid RGB - %i %i %i\n", i, r, g, b);
			fclose( fp );
			return -1;
		}
	}

	fclose( fp );

	spot_undo();

	for ( i=0; i<new_mem_cols; i++ ) mem_pal[i] = new_mem_pal[i];
	mem_cols = new_mem_cols;

	update_all_views();
	init_pal();
	gtk_widget_queue_draw(drawing_col_prev);

	return 0;
}

void update_status_bar1()
{
	char txt[64], txt2[16];

	if (status_on[0])
	{
		if ( mem_image_bpp == 1 )
			snprintf(txt, 30, "A [%i] = {%i,%i,%i}", mem_col_A, mem_pal[mem_col_A].red,
				mem_pal[mem_col_A].green, mem_pal[mem_col_A].blue );
		if ( mem_image_bpp == 3 )
			snprintf(txt, 30, "A = {%i,%i,%i}", mem_col_A24.red,
				mem_col_A24.green, mem_col_A24.blue );

		gtk_label_set_text( GTK_LABEL(label_A), txt );
		if ( mem_image_bpp == 1 )
			snprintf(txt, 30, "B [%i] = {%i,%i,%i}", mem_col_B, mem_pal[mem_col_B].red,
				mem_pal[mem_col_B].green, mem_pal[mem_col_B].blue );
		if ( mem_image_bpp == 3 )
			snprintf(txt, 30, "B = {%i,%i,%i}", mem_col_B24.red,
				mem_col_B24.green, mem_col_B24.blue );

		gtk_label_set_text( GTK_LABEL(label_B), txt );
	}
	if (status_on[1])
	{
		if ( mem_image_bpp == 1 )
			snprintf(txt, 30, "%i x %i x %i", mem_width, mem_height, mem_cols);
		else
			snprintf(txt, 30, "%i x %i x RGB", mem_width, mem_height);
		if ( layers_total>0 )
		{
			sprintf(txt2, "  (%i/%i)", layer_selected, layers_total);
			strcat(txt, txt2);
		}
		if ( mem_xpm_trans>=0 )
		{
			sprintf(txt2, "  (T=%i)", mem_xpm_trans);
			strcat(txt, txt2);
		}
		strcat(txt, "  ");
		gtk_label_set_text( GTK_LABEL(label_bar1), txt );
	}
}

void update_cols()
{
	if ( mem_image == NULL ) return;	// Only do this if we have an image

	update_status_bar1();
	mem_pat_update();

	if ( marq_status >= MARQUEE_PASTE && text_paste )
	{
		render_text( drawing_pat_prev );
		check_marquee();
		gtk_widget_queue_draw( drawing_canvas );
	}

	gtk_widget_queue_draw( drawing_pat_prev );	// Update widget
	gtk_widget_queue_draw( drawing_col_prev );
}

void init_pal()					// Initialise palette after loading
{
	mem_pal_init();				// Update palette RGB on screen
	gtk_widget_set_usize( drawing_palette, PALETTE_WIDTH, 4+mem_cols*16 );

	update_cols();
	gtk_widget_queue_draw( drawing_palette );
}

#if GTK_MAJOR_VERSION == 2
void cleanse_txt( char *out, char *in )		// Cleans up non ASCII chars for GTK+2
{
	char *c;

	c = g_filename_to_utf8( (gchar *) in, -1, NULL, NULL, NULL );
	if ( c == NULL )
	{
		sprintf(out, "Error in cleanse_txt using g_filename_to_utf8");
	}
	else
	{
		strcpy( out, c );
		g_free(c);
	}
}
#endif

void set_new_filename( char *fname )
{
	strncpy( mem_filename, fname, 250 );
	update_titlebar();
}

int do_a_load( char *fname )
{
	gboolean loading_single = FALSE;
	int res, i;
	char mess[512], real_fname[300];
	float old_zoom = can_zoom;

#if DIR_SEP == '/'
	if ( fname[0] != DIR_SEP )		// GNU/Linux
#endif
#if DIR_SEP == '\\'
	if ( fname[1] != ':' )			// Windows
#endif
	{
		getcwd( real_fname, 256 );
		i = strlen(real_fname);
		real_fname[i] = DIR_SEP;
		real_fname[i+1] = 0;
		strncat( real_fname, fname, 256 );
	}
	else strncpy( real_fname, fname, 256 );

	if ( (res = load_gif( real_fname )) == -1 )
	if ( (res = load_tiff( real_fname )) == -1 )
	if ( (res = load_bmp( real_fname )) == -1 )
	if ( (res = load_jpeg( real_fname )) == -1 )
	if ( (res = load_xpm( real_fname )) == -1 )
	if ( (res = load_xbm( real_fname )) == -1 )
		res = load_png( real_fname, 0 );

	if ( res>0 ) loading_single = TRUE;
	else
	{		// Not a single image file, but is it an mtPaint layers file?
		if ( layers_check_header(real_fname) )
		{
			res = load_layers(real_fname);
		}
	}

	if ( res<=0 )				// Error loading file
	{
		if (res == NOT_INDEXED)
		{
			snprintf(mess, 500, _("Image is not indexed: %s"), fname);
			alert_box( _("Error"), mess, ("OK"), NULL, NULL );
		} else
			if (res == TOO_BIG)
			{
				snprintf(mess, 500, _("File is too big, must be <= to width=%i height=%i : %s"), MAX_WIDTH, MAX_HEIGHT, fname);
				alert_box( _("Error"), mess, _("OK"), NULL, NULL );
			} else
			{
				alert_box( _("Error"), _("Unable to load file"),
					_("OK"), NULL, NULL );
			}
		return 1;
	}

	if ( res == FILE_LIB_ERROR )
		alert_box( _("Error"), _("The file import library had to terminate due to a problem with the file (possibly corrupt image data or a truncated file). I have managed to load some data as the header seemed fine, but I would suggest you save this image to a new file to ensure this does not happen again."), _("OK"), NULL, NULL );

	if ( res == FILE_MEM_ERROR ) memory_errors(1);		// Image was too large for OS

	if ( loading_single )
	{
		mem_mask_setall(0);		// Clear all mask info
		mem_col_A = 1, mem_col_B = 0;
		mem_col_A24 = mem_pal[mem_col_A];
		mem_col_B24 = mem_pal[mem_col_B];
		tool_pat = 0;
		init_pal();

		can_zoom = -1;
		if ( inifile_get_gboolean("zoomToggle", FALSE) )
			align_size(1);			// Always start at 100%
		else
			align_size(old_zoom);

		register_file(real_fname);
		set_new_filename(real_fname);
		notify_unchanged();

		if ( marq_status > MARQUEE_NONE ) marq_status = MARQUEE_NONE;
			// Stops unwanted automatic paste following a file load when enabling
			// "Changing tool commits paste" via preferences

		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON]), TRUE );
			// Set tool to square for new image - easy way to lose a selection marquee
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
		update_menus();
		pressed_opacity( GTK_MENU_ITEM(menu_opac[9]), NULL );	// Set opacity to 100% to start with
		if ( layers_total>0 )
			layers_notify_changed(); // We loaded an image into the layers, so notify change
	}
	else
	{
		register_file(real_fname);		// Update recently used file list
		if ( layers_window == NULL ) pressed_layers( NULL, NULL );
		if ( view_window == NULL ) pressed_view( NULL, NULL );
			// We have just loaded a layers file so display view & layers window if not up
	}

	if ( res>0 )
	{
		update_all_views();					// Show new image

		gtk_adjustment_value_changed( gtk_scrolled_window_get_hadjustment(
			GTK_SCROLLED_WINDOW(scrolledwindow_canvas) ) );
		gtk_adjustment_value_changed( gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(scrolledwindow_canvas) ) );
				// These 2 are needed to synchronize the scrollbars & image view
	}

	return 0;
}



///	FILE SELECTION WINDOW


GtkWidget *filew, *fs_png[2], *fs_jpg[2], *fs_xbm[5], *fs_combo_entry;

static int fs_get_extension()			// Get the extension type from the combo box
{
	char txt[8], *txt2[] = { "", "PNG", "JPEG", "TIFF", "BMP", "GIF", "XPM", "XBM" };
	int i, j=0;

	strncpy( txt, gtk_entry_get_text( GTK_ENTRY(fs_combo_entry) ), 6 );

	for ( i=0; i<8; i++ )
	{
		if ( strcmp( txt, txt2[i] ) == 0 ) j=i;
	}

	return j;		// The order of txt2 matches EXT_NONE, EXT_PNG ...
}

static void fs_set_extension()			// Change the filename extension to match the combo
{
	char *ext[] = { "NONE", ".png", ".jpg", ".tif", ".bmp", ".gif", ".xpm", ".xbm" },
		old_name[260], new_name[260];
	int i, j, k, l;

	strncpy( old_name,
		gtk_entry_get_text( GTK_ENTRY(GTK_FILE_SELECTION(filew)->selection_entry) ),
		256 );
	strncpy( new_name, old_name, 256 );
	i = fs_get_extension();			// New selected extension from combo
	j = file_extension_get( old_name );	// Find out current extension

	if ( i<1 || i>7 ) return;		// Ignore any dodgy numbers

	if ( i != j )				// Only do this if the format is different
	{
		k = strlen( old_name );
		if ( j == 0 )			// No current extension so just append it
		{
			if ( k<250 )
			{
				for ( l=0; l<4; l++ )
				{
					new_name[k++] = ext[i][l];
				}
				new_name[k] = 0;
			}
		}
		else				// Remove old extension, put in new one
		{
			if ( k>3 )
			{
				for ( l=0; l<4; l++ )
				{
					new_name[k-1-l] = ext[i][3-l];
				}
			}
		}
		gtk_entry_set_text( GTK_ENTRY(GTK_FILE_SELECTION(filew)->selection_entry), new_name );
	}
}

static void fs_check_format()			// Check format selected and hide widgets not needed
{
	char txt[8], *txt2[] = { "PNG", "JPEG", "TIFF", "BMP", "GIF", "XPM", "XBM" };
	int i, j = -1;
	gboolean w_show[3];

	strncpy( txt, gtk_entry_get_text( GTK_ENTRY(fs_combo_entry) ), 6 );

	for ( i=0; i<7; i++ )
	{
		if ( strcmp( txt, txt2[i] ) == 0 ) j=i;
	}

	w_show[0] = FALSE;
	w_show[1] = FALSE;
	w_show[2] = FALSE;
	switch ( j )
	{
		case 0:
		case 5:		w_show[0] = TRUE;	// PNG, XPM
				break;
		case 1:		w_show[1] = TRUE;	// JPG
				break;
		case 6:		w_show[2] = TRUE;	// XBM
				break;
	}

	for ( i=0; i<2; i++ )
	{
		if ( w_show[0] ) gtk_widget_show( fs_png[i] );
		else gtk_widget_hide( fs_png[i] );
		if ( w_show[1] ) gtk_widget_show( fs_jpg[i] );
		else gtk_widget_hide( fs_jpg[i] );
	}
	for ( i=0; i<5; i++ )
	{
		if ( w_show[2] ) gtk_widget_show( fs_xbm[i] );
		else gtk_widget_hide( fs_xbm[i] );
	}
}

static void fs_set_format()			// Check format selected and set memory accordingly
{
	char txt[8], *txt2[] = { "PNG", "JPEG", "TIFF", "BMP", "GIF", "XPM", "XBM" };
	int i, j = -1;

	strncpy( txt, gtk_entry_get_text( GTK_ENTRY(fs_combo_entry) ), 6 );

	for ( i=0; i<7; i++ )
	{
		if ( strcmp( txt, txt2[i] ) == 0 ) j=i;
	}

	switch ( j )
	{
		case 0:			// PNG, XPM
		case 5:		if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_png[0])) )
				{
					mem_xpm_trans = gtk_spin_button_get_value_as_int(
							GTK_SPIN_BUTTON(fs_png[1]) );
				}
				else
				{
					mem_xpm_trans = -1;
				}
				break;
		case 1:		gtk_spin_button_update( GTK_SPIN_BUTTON(fs_jpg[1]) );		// JPG
				mem_jpeg_quality = gtk_spin_button_get_value_as_int(
							GTK_SPIN_BUTTON(fs_jpg[1]) );
				inifile_set_gint32( "jpegQuality", mem_jpeg_quality );
				break;
		case 6:		if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_xbm[0])) ) // XBM
				{
					mem_xbm_hot_x = gtk_spin_button_get_value_as_int(
							GTK_SPIN_BUTTON(fs_xbm[2]) );
					mem_xbm_hot_y = gtk_spin_button_get_value_as_int(
							GTK_SPIN_BUTTON(fs_xbm[4]) );
				}
				else
				{
					mem_xbm_hot_x = -1;
					mem_xbm_hot_y = -1;
				}
				break;
	}
}

static void fs_combo_change( GtkWidget *widget, gpointer *user_data )
{
	fs_check_format();
	fs_set_extension();
}

gint fs_destroy( GtkWidget *widget, GtkFileSelection *fs )
{
	int x, y, width, height;

	gdk_window_get_size( filew->window, &width, &height );
	gdk_window_get_root_origin( filew->window, &x, &y );

	inifile_set_gint32("fs_window_x", x );
	inifile_set_gint32("fs_window_y", y );
	inifile_set_gint32("fs_window_w", width );
	inifile_set_gint32("fs_window_h", height );

	fs_type = 0;
	gtk_widget_destroy( filew );

	return FALSE;
}

int check_file( char *fname )		// Does file already exist?  Ask if OK to overwrite
{
	char mess[512];

	if ( valid_file(fname) == 0 )
	{
		snprintf(mess, 500, _("File: %s already exists. Do you want to overwrite it?"), fname);
		if ( alert_box( _("File Found"), mess, _("NO"), _("YES"), NULL ) != 2 ) return 1;
	}

	return 0;
}

gint fs_ok( GtkWidget *widget, GtkFileSelection *fs )
{
	char fname[256], mess[512];
	int res;

	gtk_window_set_modal(GTK_WINDOW(filew),FALSE);
		// Needed to show progress in Windows GTK+2

	if (fs_type == FS_PNG_SAVE) fs_set_extension();		// Ensure extension is correct

	strncpy( fname, gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)), 250 );
	switch ( fs_type )
	{
		case FS_PNG_LOAD:
					if ( do_a_load( fname ) == 1) goto redo;
					break;
		case FS_PNG_SAVE:
					if ( check_file(fname) == 1 ) goto redo;
					fs_set_format();	// Update data in memory from widgets
					if ( gui_save(fname) < 0 ) goto redo;
					if ( layers_total>0 )
					{
						if ( strcmp(fname, mem_filename ) != 0 )
							layers_notify_changed();
			// Filename has changed so layers file needs re-saving to be correct
					}
					set_new_filename( fname );
					update_status_bar1();		// Update transparency info
					break;
		case FS_PALETTE_LOAD:
					if ( load_pal(fname) !=0 )
					{
						snprintf(mess, 500, _("File: %s invalid - palette not updated"), fname);
						res = alert_box( _("Error"), mess, _("OK"), NULL, NULL );
						goto redo;
					}
					else	notify_changed();
					break;
		case FS_PALETTE_SAVE:
					if ( check_file(fname) != 0 ) goto redo;
					res = save_pal( fname );
					if ( res < 0 )
					{
						snprintf(mess, 500, _("Unable to save file: %s"), fname);
						alert_box( _("Error"), mess, _("OK"), NULL, NULL );
					}
					break;
		case FS_CLIP_FILE:
					strncpy( mem_clip_file[1], fname, 250 );
					gtk_entry_set_text( GTK_ENTRY(clipboard_entry),
						mem_clip_file[1] );
					break;
		case FS_EXPORT_UNDO:
		case FS_EXPORT_UNDO2:
					if ( export_undo( fname, fs_type - FS_EXPORT_UNDO ) != 0 )
						alert_box( _("Error"), _("Unable to export undo images"),
							_("OK"), NULL, NULL );
					break;
		case FS_EXPORT_ASCII:
					if ( check_file(fname) != 0 ) goto redo;
					if ( export_ascii( fname ) != 0 )
						alert_box( _("Error"), _("Unable to export ASCII file"),
							_("OK"), NULL, NULL );
					break;
		case FS_LAYER_SAVE:
					if ( check_file(fname) == 1 ) goto redo;
					res = save_layers( fname );
					if ( res != 1 ) goto redo;
					break;
	}

	gtk_window_set_transient_for( GTK_WINDOW(filew), NULL );	// Needed in Windows to stop GTK+ lowering the main window below window underneath
	update_menus();

	fs_destroy( NULL, NULL );
	return FALSE;
redo:
	gtk_window_set_modal(GTK_WINDOW(filew), TRUE);
	return FALSE;
}

void file_selector( int action_type )
{
	GtkWidget *hbox, *label, *combo;
	GList *combo_list = NULL;
	gboolean do_it;
	int i, j, k;
	char *combo_txt[] = { "PNG", "JPEG", "TIFF", "BMP", "PNG", "GIF", "BMP", "XPM", "XBM" };

	char *title = NULL, txt[300], *temp_txt;
	
	fs_type = action_type;

	switch ( fs_type )
	{
		case FS_PNG_LOAD:	title = _("Load Image File");
					if ( layers_total==0 )
					{
						if ( check_for_changes() == 1 ) return;
					}
					else	if ( check_layers_for_changes() == 1 ) return;
					break;
		case FS_PNG_SAVE:	title = _("Save Image File");
					break;
		case FS_PALETTE_LOAD:	title = _("Load Palette File");
					break;
		case FS_PALETTE_SAVE:	title = _("Save Palette File");
					break;
		case FS_CLIP_FILE:	title = _("Select Clipboard File");
					break;
		case FS_EXPORT_UNDO:	title = _("Export Undo Images");
					break;
		case FS_EXPORT_UNDO2:	title = _("Export Undo Images (reversed)");
					break;
		case FS_EXPORT_ASCII:	title = _("Export ASCII Art");
					break;
		case FS_LAYER_SAVE:	title = _("Save Layer Files");
					break;
	}

	filew = gtk_file_selection_new ( title );

	gtk_window_set_modal(GTK_WINDOW(filew),TRUE);
	gtk_window_set_default_size( GTK_WINDOW(filew),
		inifile_get_gint32("fs_window_w", 550 ), inifile_get_gint32("fs_window_h", 500 ) );
	gtk_widget_set_uposition( filew,
		inifile_get_gint32("fs_window_x", 0 ), inifile_get_gint32("fs_window_y", 0 ) );

	if ( fs_type == FS_EXPORT_UNDO )
		gtk_widget_set_sensitive( GTK_WIDGET(GTK_FILE_SELECTION(filew)->file_list), FALSE );

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		"clicked", GTK_SIGNAL_FUNC(fs_ok), (gpointer) filew);

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC( fs_destroy ), GTK_OBJECT (filew) );

	gtk_signal_connect_object (GTK_OBJECT(filew),
		"delete_event", GTK_SIGNAL_FUNC( fs_destroy ), GTK_OBJECT (filew) );

	if ( fs_type == FS_PNG_SAVE && strcmp( mem_filename, _("Untitled") ) != 0 )
		gtk_file_selection_set_filename( GTK_FILE_SELECTION(filew), mem_filename );
	else
	{
		if ( fs_type == FS_LAYER_SAVE && strcmp( layers_filename, _("Untitled") ) != 0 )
			gtk_file_selection_set_filename( GTK_FILE_SELECTION(filew), layers_filename );
		else
		{
			if ( fs_type == FS_LAYER_SAVE )
			{
				strncpy( txt, inifile_get("last_dir", "/"), 256 );
				strncat( txt, "layers.txt", 256 );
				gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), txt );
			}
			else
				gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew),
					inifile_get("last_dir", "/") );
		}
	}

	if ( fs_type == FS_PNG_SAVE )
	{
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->main_vbox), hbox, FALSE, TRUE, 0);

		label = gtk_label_new( _("File Format") );
		gtk_widget_show( label );
		gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 10 );

		combo = gtk_combo_new ();
		gtk_widget_set_usize(combo, 100, -2);
		gtk_widget_show (combo);
		gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 10);
		fs_combo_entry = GTK_COMBO (combo)->entry;
		gtk_widget_show (fs_combo_entry);
		gtk_entry_set_editable( GTK_ENTRY(fs_combo_entry), FALSE );

		temp_txt = "PNG";
		i = file_extension_get( mem_filename );
		k = 4;
		if ( i == EXT_BMP ) temp_txt = "BMP";
		if ( mem_image_bpp == 3 )
		{
			j = 0;
			if ( i == EXT_JPEG ) temp_txt = "JPEG";
			if ( i == EXT_TIFF ) temp_txt = "TIFF";
		}
		else
		{
			if ( i == EXT_GIF ) temp_txt = "GIF";
			if ( i == EXT_XPM ) temp_txt = "XPM";
			j = 4;
			if ( mem_cols == 2 )
			{
				k++;			// Allow XBM if indexed with 2 colours
				if ( i == EXT_XBM ) temp_txt = "XBM";
			}
		}

		for ( i=j; i<(j+k); i++ )
		{
			combo_list = g_list_append( combo_list, combo_txt[i] );
		}
		gtk_combo_set_popdown_strings( GTK_COMBO(combo), combo_list );
		gtk_entry_set_text( GTK_ENTRY(fs_combo_entry), temp_txt );

		gtk_signal_connect_object (GTK_OBJECT (fs_combo_entry), "changed",
			GTK_SIGNAL_FUNC (fs_combo_change), GTK_OBJECT (fs_combo_entry));

		if ( mem_xpm_trans >=0 ) do_it = TRUE; else do_it = FALSE;
		fs_png[0] = add_a_toggle( _("Set transparency index"), hbox, do_it );

		i = mem_xpm_trans;
		if ( i<0 ) i=0;
		fs_png[1] = add_a_spin( i, 0, mem_cols-1 );
		gtk_box_pack_start (GTK_BOX (hbox), fs_png[1], FALSE, FALSE, 10);

		fs_jpg[0] = gtk_label_new( _("JPEG Save Quality (100=High)") );
		gtk_widget_show( fs_jpg[0] );
		gtk_box_pack_start( GTK_BOX(hbox), fs_jpg[0], FALSE, FALSE, 10 );
		fs_jpg[1] = add_a_spin( mem_jpeg_quality, 0, 100 );
		gtk_box_pack_start (GTK_BOX (hbox), fs_jpg[1], FALSE, FALSE, 10);

		if ( mem_xbm_hot_x >=0 ) do_it = TRUE; else do_it = FALSE;
		fs_xbm[0] = add_a_toggle( _("Set hotspot"), hbox, do_it );
		fs_xbm[1] = gtk_label_new( _("X =") );
		gtk_widget_show( fs_xbm[1] );
		gtk_box_pack_start( GTK_BOX(hbox), fs_xbm[1], FALSE, FALSE, 10 );
		i = mem_xbm_hot_x;
		if ( i<0 ) i = 0;
		fs_xbm[2] = add_a_spin( i, 0, mem_width-1 );
		gtk_box_pack_start (GTK_BOX (hbox), fs_xbm[2], FALSE, FALSE, 10);
		fs_xbm[3] = gtk_label_new( _("Y =") );
		gtk_widget_show( fs_xbm[3] );
		gtk_box_pack_start( GTK_BOX(hbox), fs_xbm[3], FALSE, FALSE, 10 );
		i = mem_xbm_hot_y;
		if ( i<0 ) i = 0;
		fs_xbm[4] = add_a_spin( i, 0, mem_height-1 );
		gtk_box_pack_start (GTK_BOX (hbox), fs_xbm[4], FALSE, FALSE, 10);

		fs_check_format();
	}

	gtk_widget_show (filew);
	gtk_window_set_transient_for( GTK_WINDOW(filew), GTK_WINDOW(main_window) );
}

void align_size( float new_zoom )		// Set new zoom level
{
	GtkAdjustment *hori, *vert;
	int nv_h = 0, nv_v = 0;			// New positions of scrollbar

	if ( zoom_flag != 0 ) return;		// Needed as we could be called twice per iteration

	if ( new_zoom<MIN_ZOOM ) new_zoom = MIN_ZOOM;
	if ( new_zoom>MAX_ZOOM ) new_zoom = MAX_ZOOM;

	if ( new_zoom != can_zoom )
	{
		zoom_flag = 1;
		hori = gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
		vert = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

		if ( mem_ics == 0 )
		{
			if ( hori->page_size > mem_width*can_zoom ) mem_icx = 0.5;
			else mem_icx = ( hori->value + hori->page_size/2 ) / (mem_width*can_zoom);

			if ( vert->page_size > mem_height*can_zoom ) mem_icy = 0.5;
			else mem_icy = ( vert->value + vert->page_size/2 ) / (mem_height*can_zoom);
		}
		mem_ics = 0;

		can_zoom = new_zoom;

		if ( hori->page_size < mem_width*can_zoom )
			nv_h = mem_width*can_zoom*mem_icx - hori->page_size/2;
		else	nv_h = 0;

		if ( vert->page_size < mem_height*can_zoom )
			nv_v = mem_height*can_zoom*mem_icy - vert->page_size/2;
		else	nv_v = 0;

		hori->value = nv_h;
		hori->upper = mem_width*can_zoom;
		vert->value = nv_v;
		vert->upper = mem_height*can_zoom;

#if GTK_MAJOR_VERSION == 1
		gtk_adjustment_value_changed( hori );
		gtk_adjustment_value_changed( vert );
#endif
		gtk_widget_set_usize( drawing_canvas, mem_width*can_zoom, mem_height*can_zoom );

		if (status_on[4]) init_status_bar();
		zoom_flag = 0;
		vw_focus_view();		// View window position may need updating
	}
}

void square_continuous( int nx, int ny, int *minx, int *miny, int *xw, int *yh )
{
	if ( tool_size == 1 )
	{
		if ( mem_image_bpp == 1 ) PUT_PIXEL( nx, ny )
		if ( mem_image_bpp == 3 ) PUT_PIXEL24( nx, ny )
	}
	else
	{
		if ( tablet_working )	// Needed to fill in possible gap when size changes
		{
			f_rectangle( tool_ox - tool_size/2, tool_oy - tool_size/2,
					tool_size, tool_size );
		}
		if ( ny > tool_oy )		// Down
		{
			h_para( tool_ox - tool_size/2,
				tool_oy - tool_size/2 + tool_size - 1,
				nx - tool_size/2,
				ny - tool_size/2 + tool_size - 1,
				tool_size );
		}
		if ( nx > tool_ox )		// Right
		{
			v_para( tool_ox - tool_size/2 + tool_size - 1,
				tool_oy - tool_size/2,
				nx - tool_size/2 + tool_size -1,
				ny - tool_size/2,
				tool_size );
		}
		if ( ny < tool_oy )		// Up
		{
			h_para( tool_ox - tool_size/2,
				tool_oy - tool_size/2,
				nx - tool_size/2,
				ny - tool_size/2,
				tool_size );
		}
		if ( nx < tool_ox )		// Left
		{
			v_para( tool_ox - tool_size/2,
				tool_oy - tool_size/2,
				nx - tool_size/2,
				ny - tool_size/2,
				tool_size );
		}
	}
}

void vertical_continuous( int nx, int ny, int *minx, int *miny, int *xw, int *yh )
{
	int	ax = tool_ox, ay = tool_oy - tool_size/2,
		bx = nx, by = ny - tool_size/2, vlen = tool_size;

	int mny, may;

	if ( ax == bx )		// Simple vertical line required
	{
		mtMIN( ay, tool_oy - tool_size/2, ny - tool_size/2 )
		mtMAX( by, tool_oy - tool_size/2 + tool_size - 1, ny - tool_size/2 + tool_size - 1 )
		vlen = by - ay + 1;
		if ( ay < 0 )
		{
			vlen = vlen + ay;
			ay = 0;
		}
		if ( by >= mem_height )
		{
			vlen = vlen - ( mem_height - by + 1 );
			by = mem_height - 1;
		}

		if ( vlen <= 1 )
		{
			ax = bx; ay = by;
			if ( mem_image_bpp == 1 ) PUT_PIXEL( bx, by )
			if ( mem_image_bpp == 3 ) PUT_PIXEL24( bx, by )
		}
		else
		{
			sline( ax, ay, bx, by );

			mtMIN( *minx, ax, bx )
			mtMIN( *miny, ay, by )
			*xw = abs( ax - bx ) + 1;
			*yh = abs( ay - by ) + 1;
		}
	}
	else			// Parallelogram with vertical left and right sides required
	{
		v_para( ax, ay, bx, by, tool_size );

		mtMIN( *minx, ax, bx )
		*xw = abs( ax - bx ) + 1;

		mtMIN( mny, ay, by )
		mtMAX( may, ay + tool_size - 1, by + tool_size - 1 )

		mtMAX( mny, mny, 0 )
		mtMIN( may, may, mem_height )

		*miny = mny;
		*yh = may - mny + 1;
	}
}

void horizontal_continuous( int nx, int ny, int *minx, int *miny, int *xw, int *yh )
{
	int ax = tool_ox - tool_size/2, ay = tool_oy,
		bx = nx - tool_size/2, by = ny, hlen = tool_size;

	int mnx, max;

	if ( ay == by )		// Simple horizontal line required
	{
		mtMIN( ax, tool_ox - tool_size/2, nx - tool_size/2 )
		mtMAX( bx, tool_ox - tool_size/2 + tool_size - 1, nx - tool_size/2 + tool_size - 1 )
		hlen = bx - ax + 1;
		if ( ax < 0 )
		{
			hlen = hlen + ax;
			ax = 0;
		}
		if ( bx >= mem_width )
		{
			hlen = hlen - ( mem_width - bx + 1 );
			bx = mem_width - 1;
		}

		if ( hlen <= 1 )
		{
			ax = bx; ay = by;
			if ( mem_image_bpp == 1 ) PUT_PIXEL( bx, by )
			if ( mem_image_bpp == 3 ) PUT_PIXEL24( bx, by )
		}
		else
		{
			sline( ax, ay, bx, by );

			mtMIN( *minx, ax, bx )
			mtMIN( *miny, ay, by )
			*xw = abs( ax - bx ) + 1;
			*yh = abs( ay - by ) + 1;
		}
	}
	else			// Parallelogram with horizontal top and bottom sides required
	{
		h_para( ax, ay, bx, by, tool_size );

		mtMIN( *miny, ay, by )
		*yh = abs( ay - by ) + 1;

		mtMIN( mnx, ax, bx )
		mtMAX( max, ax + tool_size - 1, bx + tool_size - 1 )

		mtMAX( mnx, mnx, 0 )
		mtMIN( max, max, mem_width )

		*minx = mnx;
		*xw = max - mnx + 1;
	}
}

unsigned char *pattern_fill_allocate()		// Allocate memory for pattern fill
{
	int err = 0, i, j = mem_width * mem_height;
	unsigned char *mem = NULL;

	if ( j < mem_undo_limit * 1024 * 1024 )
	{
		mem = malloc( j );
		if ( mem == NULL ) err = 1;
		else for ( i=0; i<j; i++ ) mem[i] = 0;		// Flush all to zero
	}
	else err = 2;

	if ( err>0 ) memory_errors(err);	// Not enough system/allocated memory so tell the user

	return mem;
}

void update_all_views()				// Update whole canvas on all views
{
	if ( !allow_view ) gtk_widget_queue_draw( vw_drawing );
	gtk_widget_queue_draw( drawing_canvas );
}


void stretch_poly_line(int x, int y)			// Clear old temp line, draw next temp line
{
	if ( poly_points>0 && poly_points<MAX_POLY )
	{
		if ( line_x1 != x || line_y1 != y )	// This check reduces flicker
		{
			repaint_line(0);
			paint_poly_marquee();

			line_x1 = x;
			line_y1 = y;
			line_x2 = poly_mem[poly_points-1][0];
			line_y2 = poly_mem[poly_points-1][1];

			repaint_line(2);
		}
	}
}

static void poly_conclude()
{
	repaint_line(0);
	if ( poly_points < 3 )
	{
		poly_status = POLY_NONE;
		poly_points = 0;
		update_all_views();
		update_sel_bar();
	}
	else
	{
		poly_status = POLY_DONE;
		poly_init();			// Set up polgon stats
		marq_x1 = poly_min_x;
		marq_y1 = poly_min_y;
		marq_x2 = poly_max_x;
		marq_y2 = poly_max_y;
		update_menus();			// Update menu/icons

		paint_poly_marquee();
		update_sel_bar();
	}
}

static void poly_add_po( int x, int y )
{
	repaint_line(0);
	poly_add(x, y);
	if ( poly_points >= MAX_POLY ) poly_conclude();
	paint_poly_marquee();
	update_sel_bar();
}

void tool_action( int x, int y, int button, gdouble pressure )
{
	int minx = -1, miny = -1, xw = -1, yh = -1;
	int i, j, k, rx, ry, sx, sy;
	int ox, oy, off1, off2, o_size = tool_size, o_flow = tool_flow, o_opac = tool_opacity, n_vs[3];
	int xdo, ydo, px, py, todo, oox, ooy;	// Continuous smudge stuff
	float rat;
	unsigned char rpix = 0, spix, *pat_fill;
	gboolean first_point = FALSE, paint_action = FALSE;
	png_color fstart = {0,0,0};

	if ( tool_fixx > -1 ) x = tool_fixx;
	if ( tool_fixy > -1 ) y = tool_fixy;

	if ( (button == 1 || button == 3) && (tool_type <= TOOL_SPRAY) )
		paint_action = TRUE;

	if ( pen_down == 0 )
	{
		first_point = TRUE;
		if ( button == 3 && paint_action && !col_reverse )
		{
			col_reverse = TRUE;
			mem_swap_cols();
		}
	}
	else if ( tool_ox == x && tool_oy == y ) return;	// Only do something with a new point

	if ( (button == 1 || paint_action) && tool_type != TOOL_FLOOD &&
		tool_type != TOOL_SELECT && tool_type != TOOL_POLYGON )
	{
		if ( !(tool_type == TOOL_LINE && line_status == LINE_NONE) )
		{
			mem_undo_next();		// Do memory stuff for undo
		}
	}

	if ( tablet_working )
	{
		pressure = (pressure - 0.2)/0.8;
		mtMIN( pressure, pressure, 1)
		mtMAX( pressure, pressure, 0)

		n_vs[0] = tool_size;
		n_vs[1] = tool_flow;
		n_vs[2] = tool_opacity;
		for ( i=0; i<3; i++ )
		{
			if ( tablet_tool_use[i] )
			{
				if ( tablet_tool_factor[i] > 0 )
					n_vs[i] *= (1 + tablet_tool_factor[i] * (pressure - 1));
				else
					n_vs[i] *= (0 - tablet_tool_factor[i] * (1 - pressure));
				mtMAX( n_vs[i], n_vs[i], 1 )
			}
		}
		tool_size = n_vs[0];
		tool_flow = n_vs[1];
		tool_opacity = n_vs[2];
	}

	minx = x - tool_size/2;
	miny = y - tool_size/2;
	xw = tool_size;
	yh = tool_size;

	if ( paint_action && !first_point && mem_continuous && tool_size == 1 &&
		tool_type < TOOL_SPRAY && ( abs(x - tool_ox) > 1 || abs(y - tool_oy ) > 1 ) )
	{		// Single point continuity
		sline( tool_ox, tool_oy, x, y );

		mtMIN( minx, tool_ox, x )
		mtMIN( miny, tool_oy, y )
		xw = abs( tool_ox - x ) + 1;
		yh = abs( tool_oy - y ) + 1;
	}
	else
	{
		if ( mem_continuous && !first_point && (button == 1 || button == 3) )
		{
			mtMIN( minx, tool_ox, x )
			mtMAX( xw, tool_ox, x )
			xw = xw - minx + tool_size;
			minx = minx - tool_size/2;

			mtMIN( miny, tool_oy, y )
			mtMAX( yh, tool_oy, y )
			yh = yh - miny + tool_size;
			miny = miny - tool_size/2;

			mem_boundary( &minx, &miny, &xw, &yh );
		}
		if ( tool_type == TOOL_SQUARE && paint_action )
		{
			if ( !mem_continuous || first_point )
				f_rectangle( minx, miny, xw, yh );
			else
			{
				square_continuous(x, y, &minx, &miny, &xw, &yh);
			}
		}
		if ( tool_type == TOOL_CIRCLE  && paint_action )
		{
			if ( mem_continuous && !first_point )
			{
				tline( tool_ox, tool_oy, x, y, tool_size );
			}
			f_circle( x, y, tool_size );
		}
		if ( tool_type == TOOL_HORIZONTAL && paint_action )
		{
			if ( !mem_continuous || first_point || tool_size == 1 )
			{
				for ( j=0; j<tool_size; j++ )
				{
					rx = x - tool_size/2 + j;
					ry = y;
					if ( mem_image_bpp == 1 )
						IF_IN_RANGE( rx, ry ) PUT_PIXEL( rx, ry )
					if ( mem_image_bpp == 3 )
						IF_IN_RANGE( rx, ry ) PUT_PIXEL24( rx, ry )
				}
			}
			else	horizontal_continuous(x, y, &minx, &miny, &xw, &yh);
		}
		if ( tool_type == TOOL_VERTICAL && paint_action )
		{
			if ( !mem_continuous || first_point || tool_size == 1 )
			{
				for ( j=0; j<tool_size; j++ )
				{
					rx = x;
					ry = y - tool_size/2 + j;
					if ( mem_image_bpp == 1 )
						IF_IN_RANGE( rx, ry ) PUT_PIXEL( rx, ry )
					if ( mem_image_bpp == 3 )
						IF_IN_RANGE( rx, ry ) PUT_PIXEL24( rx, ry )
				}
			}
			else	vertical_continuous(x, y, &minx, &miny, &xw, &yh);
		}
		if ( tool_type == TOOL_SLASH && paint_action )
		{
			if ( mem_continuous && !first_point && tool_size > 1 )
				g_para( x + (tool_size-1)/2, y - tool_size/2,
					x + (tool_size-1)/2 - (tool_size - 1),
					y - tool_size/2 + tool_size - 1,
					tool_ox - x, tool_oy - y );
			else for ( j=0; j<tool_size; j++ )
			{
				rx = x + (tool_size-1)/2 - j;
				ry = y - tool_size/2 + j;
				if ( mem_image_bpp == 1 )
					IF_IN_RANGE( rx, ry ) PUT_PIXEL( rx, ry )
				if ( mem_image_bpp == 3 )
					IF_IN_RANGE( rx, ry ) PUT_PIXEL24( rx, ry )
			}
		}
		if ( tool_type == TOOL_BACKSLASH && paint_action )
		{
			if ( mem_continuous && !first_point && tool_size > 1 )
				g_para( x - tool_size/2, y - tool_size/2,
					x - tool_size/2 + tool_size - 1,
					y - tool_size/2 + tool_size - 1,
					tool_ox - x, tool_oy - y );
			else for ( j=0; j<tool_size; j++ )
			{
				rx = x - tool_size/2 + j;
				ry = y - tool_size/2 + j;
				if ( mem_image_bpp == 1 )
					IF_IN_RANGE( rx, ry ) PUT_PIXEL( rx, ry )
				if ( mem_image_bpp == 3 )
					IF_IN_RANGE( rx, ry ) PUT_PIXEL24( rx, ry )
			}
		}
		if ( tool_type == TOOL_SPRAY && paint_action )
		{
			if ( mem_image_bpp == 1 )
				for ( j=0; j<tool_flow; j++ )
				{
					rx = x - tool_size/2 + rand() % tool_size;
					ry = y - tool_size/2 + rand() % tool_size;
					IF_IN_RANGE( rx, ry ) PUT_PIXEL( rx, ry )
				}
			if ( mem_image_bpp == 3 )
				for ( j=0; j<tool_flow; j++ )
				{
					rx = x - tool_size/2 + rand() % tool_size;
					ry = y - tool_size/2 + rand() % tool_size;
					IF_IN_RANGE( rx, ry ) PUT_PIXEL24( rx, ry )
				}
		}
		if ( tool_type == TOOL_SHUFFLE && button == 1 )
		{
			for ( j=0; j<tool_flow; j++ )
			{
				rx = x - tool_size/2 + rand() % tool_size;
				ry = y - tool_size/2 + rand() % tool_size;
				sx = x - tool_size/2 + rand() % tool_size;
				sy = y - tool_size/2 + rand() % tool_size;
				IF_IN_RANGE( rx, ry ) IF_IN_RANGE( sx, sy )
				{
					if ( mem_image_bpp == 1 )
					{
						rpix = mem_image[ rx + ry*mem_width ];
						spix = mem_image[ sx + sy*mem_width ];
						if ( mem_prot_mask[rpix] == 0 &&
							mem_prot_mask[spix] == 0 )
						{
							mem_image[ rx + ry*mem_width ] = spix;
							mem_image[ sx + sy*mem_width ] = rpix;
						}
					}
					if ( mem_image_bpp == 3 )
					{
						off1 = 3*(rx + ry*mem_width);
						off2 = 3*(sx + sy*mem_width);

						px = MEM_2_INT(mem_image, off1);
						py = MEM_2_INT(mem_image, off2);

						if ( !mem_protected_RGB(px) && !mem_protected_RGB(py) )
						{
							for ( i=0; i<3; i++ )
							{
								rpix = mem_image[ i + off1 ];
								spix = mem_image[ i + off2 ];
								mem_image[ i + off1 ] = spix;
								mem_image[ i + off2 ] = rpix;
							}
						}
					}
				}
			}
		}
		if ( tool_type == TOOL_FLOOD && button == 1 )
		{
			if ( mem_image_bpp == 1 )
			{
				rpix = GET_PIXEL(x,y);
				if ( mem_prot_mask[rpix] == 0 )
				{
					if ( tool_pat == 0 && rpix != mem_col_A ) // Pure colour fill
					{
					 spot_undo();
					 flood_fill( x, y, rpix );
					 update_all_views();
					}
					if ( tool_pat !=0 )		// Patteren fill
					{
					  pat_fill = pattern_fill_allocate();
					  if ( pat_fill != NULL )
					  {
						spot_undo();
						flood_fill_pat( x, y, rpix, pat_fill );
						mem_paint_mask( pat_fill );
						update_all_views();
						free( pat_fill );
					  }
					}
				}
			}
			if ( mem_image_bpp == 3 )
			{
				fstart = get_pixel24(x, y);
				j = PNG_2_INT(fstart);

				if ( mem_protected_RGB(j) == 0 )
				{
					k = PNG_2_INT(mem_col_A24);
					i = tool_opacity;
					tool_opacity = 100;
						// 100% pure colour for flood filling

					if ( tool_pat == 0 && j != k)	// Pure colour fill
					{
						spot_undo();
						flood_fill24( x, y, fstart );
						update_all_views();
					}
					if ( tool_pat !=0 )		// Patteren fill
					{
					  pat_fill = pattern_fill_allocate();
					  if ( pat_fill != NULL )
					  {
						spot_undo();
						flood_fill24_pat( x, y, fstart, pat_fill );
						mem_paint_mask( pat_fill );
						update_all_views();
						free( pat_fill );
					  }
					}
					tool_opacity = i;
				}
			}
		}
		if ( tool_type == TOOL_SMUDGE && button == 1 )
		{
			if ( !first_point && (tool_ox!=x || tool_oy!=y) )
			{
				if ( mem_continuous )
				{
					xdo = tool_ox - x;
					ydo = tool_oy - y;
					mtMAX( todo, abs(xdo), abs(ydo) )
					oox = tool_ox;
					ooy = tool_oy;

					for ( i=1; i<=todo; i++ )
					{
						rat = ((float) i ) / todo;
						px = mt_round(tool_ox + (x - tool_ox) * rat);
						py = mt_round(tool_oy + (y - tool_oy) * rat);
						mem_smudge(oox, ooy, px, py);
						oox = px;
						ooy = py;
					}
				}
				else mem_smudge(tool_ox, tool_oy, x, y);
			}
		}
		if ( tool_type == TOOL_CLONE && button == 1 )
		{
			if ( first_point || (!first_point && (tool_ox!=x || tool_oy!=y)) )
			{
				mem_clone( x+clone_x, y+clone_y, x, y );
			}
		}
	}

	if ( tool_type == TOOL_LINE )
	{
		if ( button == 1 )
		{
			line_x1 = x;
			line_y1 = y;
			if ( line_status == LINE_NONE )
			{
				line_x2 = x;
				line_y2 = y;
			}

			// Draw circle at x, y
			if ( line_status == LINE_LINE )
			{
				if ( tool_size > 1 )
				{
					f_circle( line_x1, line_y1, tool_size );
					f_circle( line_x2, line_y2, tool_size );
					// Draw tool_size thickness line from 1-2
					tline( line_x1, line_y1, line_x2, line_y2, tool_size );
				}
				else sline( line_x1, line_y1, line_x2, line_y2 );

				mtMIN( minx, line_x1, line_x2 )
				mtMIN( miny, line_y1, line_y2 )
				minx = minx - tool_size/2;
				miny = miny - tool_size/2;
				xw = abs( line_x2 - line_x1 ) + 1 + tool_size;
				yh = abs( line_y2 - line_y1 ) + 1 + tool_size;

				line_x2 = line_x1;
				line_y2 = line_y1;
				line_status = LINE_START;
			}
			if ( line_status == LINE_NONE ) line_status = LINE_START;
		}
		else stop_line();	// Right button pressed so stop line process
	}

	if ( tool_type == TOOL_SELECT || tool_type == TOOL_POLYGON )
	{
		if ( marq_status == MARQUEE_PASTE )		// User wants to drag the paste box
		{
			if ( x>=marq_x1 && x<=marq_x2 && y>=marq_y1 && y<=marq_y2 )
			{
				marq_status = MARQUEE_PASTE_DRAG;
				marq_drag_x = x - marq_x1;
				marq_drag_y = y - marq_y1;
			}
		}
		if ( marq_status == MARQUEE_PASTE_DRAG && ( button == 1 || button == 13 || button == 2 ) )
		{	// User wants to drag the paste box
			ox = marq_x1;
			oy = marq_y1;
			paint_marquee(0, x - marq_drag_x, y - marq_drag_y);
			marq_x1 = x - marq_drag_x;
			marq_y1 = y - marq_drag_y;
			marq_x2 = marq_x1 + mem_clip_w - 1;
			marq_y2 = marq_y1 + mem_clip_h - 1;
			paint_marquee(1, ox, oy);
		}
		if ( (marq_status == MARQUEE_PASTE_DRAG || marq_status == MARQUEE_PASTE ) &&
			(button == 13 || button == 3) )
		{	// User wants to commit the paste
			commit_paste(TRUE);
		}
		if ( tool_type == TOOL_SELECT && button == 3 && (marq_status == MARQUEE_DONE ) )
		{
			pressed_select_none(NULL, NULL);
			set_cursor();
		}
		if ( tool_type == TOOL_SELECT && button == 1 && (marq_status == MARQUEE_NONE ||
			marq_status == MARQUEE_DONE) )		// Starting a selection
		{
			if ( marq_status == MARQUEE_DONE )
			{
				paint_marquee(0, marq_x1-mem_width, marq_y1-mem_height);
				i = close_to(x, y);
				if ( (i%2) == 0 )
				{	mtMAX(marq_x1, marq_x1, marq_x2)	}
				else
				{	mtMIN(marq_x1, marq_x1, marq_x2)	}
				if ( (i/2) == 0 )
				{	mtMAX(marq_y1, marq_y1, marq_y2)	}
				else
				{	mtMIN(marq_y1, marq_y1, marq_y2)	}
				set_cursor();
			}
			else
			{
				marq_x1 = x;
				marq_y1 = y;
			}
			marq_x2 = x;
			marq_y2 = y;
			marq_status = MARQUEE_SELECTING;
			paint_marquee(1, marq_x1-mem_width, marq_y1-mem_height);
		}
		else
		{
			if ( marq_status == MARQUEE_SELECTING )		// Continuing to make a selection
			{
				paint_marquee(0, marq_x1-mem_width, marq_y1-mem_height);
				marq_x2 = x;
				marq_y2 = y;
				paint_marquee(1, marq_x1-mem_width, marq_y1-mem_height);
			}
		}
	}

	if ( tool_type == TOOL_POLYGON )
	{
		if ( poly_status == POLY_NONE && marq_status == MARQUEE_NONE )
		{
			if ( button != 0 )		// Start doing something
			{
				if ( button == 1 )
					poly_status = POLY_SELECTING;
				else
					poly_status = POLY_DRAGGING;
			}
		}
		if ( poly_status == POLY_SELECTING )
		{
			if ( button == 1 ) poly_add_po(x, y);		// Add another point to polygon
			else
			{
				if ( button == 3 ) poly_conclude();	// Stop adding points
			}
		}
		if ( poly_status == POLY_DRAGGING )
		{
			if ( button == 0 ) poly_conclude();		// Stop forming polygon
			else poly_add_po(x, y);				// Add another point to polygon
		}
	}

	if ( tool_type != TOOL_SELECT && tool_type != TOOL_POLYGON )
	{
		if ( minx<0 )
		{
			xw = xw + minx;
			minx = 0;
		}

		if ( miny<0 )
		{
			yh = yh + miny;
			miny = 0;
		}
		if ( can_zoom<1 )
		{
			xw = xw + mt_round(1/can_zoom) + 1;
			yh = yh + mt_round(1/can_zoom) + 1;
		}
		if ( (minx+xw) > mem_width ) xw = mem_width - minx;
		if ( (miny+yh) > mem_height ) yh = mem_height - miny;
		if ( tool_type != TOOL_FLOOD && (button == 1 || paint_action) &&
			minx>-1 && miny>-1 && xw>-1 && yh>-1)
		{
			gtk_widget_queue_draw_area( drawing_canvas,
				minx*can_zoom, miny*can_zoom,
				mt_round(xw*can_zoom), mt_round(yh*can_zoom) );
			vw_update_area( minx, miny, xw+1, yh+1 );
		}
	}
	tool_ox = x;	// Remember the coords just used as they are needed in continuous mode
	tool_oy = y;

	if ( tablet_working )
	{
		tool_size = o_size;
		tool_flow = o_flow;
		tool_opacity = o_opac;
	}
}

void check_marquee()		// Check marquee boundaries are OK - may be outside limits via arrow keys
{
	int i;

	if ( marq_status >= MARQUEE_PASTE )
	{
		mtMAX( marq_x1, marq_x1, 1-mem_clip_w )
		mtMAX( marq_y1, marq_y1, 1-mem_clip_h )
		mtMIN( marq_x1, marq_x1, mem_width-1 )
		mtMIN( marq_y1, marq_y1, mem_height-1 )
		marq_x2 = marq_x1 + mem_clip_w - 1;
		marq_y2 = marq_y1 + mem_clip_h - 1;
	}
	else			// Selection mode in operation
	{
		mtMAX( marq_x1, marq_x1, 0 )
		mtMAX( marq_y1, marq_y1, 0 )
		mtMAX( marq_x2, marq_x2, 0 )
		mtMAX( marq_y2, marq_y2, 0 )
		mtMIN( marq_x1, marq_x1, mem_width-1 )
		mtMIN( marq_y1, marq_y1, mem_height-1 )
		mtMIN( marq_x2, marq_x2, mem_width-1 )
		mtMIN( marq_y2, marq_y2, mem_height-1 )
		if ( tool_type == TOOL_POLYGON && poly_points > 0 )
		{
			for ( i=0; i<poly_points; i++ )
			{
				mtMIN( poly_mem[i][0], poly_mem[i][0], mem_width-1 )
				mtMIN( poly_mem[i][1], poly_mem[i][1], mem_height-1 )
			}
		}
	}
}

int vc_x1, vc_y1, vc_x2, vc_y2;			// Visible canvas
GtkAdjustment *hori, *vert;

void get_visible()
{
	GtkAdjustment *hori, *vert;

	hori = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );
	vert = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolledwindow_canvas) );

	vc_x1 = hori->value;
	vc_y1 = vert->value;
	vc_x2 = hori->value + hori->page_size - 1;
	vc_y2 = vert->value + vert->page_size - 1;
}

void clip_area( int *rx, int *ry, int *rw, int *rh )		// Clip area to visible canvas
{
	if ( *rx<vc_x1 )
	{
		*rw = *rw + (*rx - vc_x1);
		*rx = vc_x1;
	}
	if ( *ry<vc_y1 )
	{
		*rh = *rh + (*ry - vc_y1);
		*ry = vc_y1;
	}
	if ( *rx + *rw > vc_x2 ) *rw = vc_x2 - *rx + 1;
	if ( *ry + *rh > vc_y2 ) *rh = vc_y2 - *ry + 1;
}

void update_paste_chunk( int x1, int y1, int x2, int y2 )
{
	int ux1, uy1, ux2, uy2;

	get_visible();

	mtMAX( ux1, vc_x1, x1 )
	mtMAX( uy1, vc_y1, y1 )
	mtMIN( ux2, vc_x2, x2 )
	mtMIN( uy2, vc_y2, y2 )

	mtMIN( ux2, ux2, mem_width*can_zoom - 1 )
	mtMIN( uy2, uy2, mem_height*can_zoom - 1 )

	if ( ux1 <= ux2 && uy1 <= uy2 )		// Only repaint if on visible canvas
		repaint_paste( ux1, uy1, ux2, uy2 );
}

void paint_poly_marquee()			// Paint polygon marquee
{
	int i, j, last = poly_points-1, co[2];
	
	GdkPoint xy[MAX_POLY+1];

	check_marquee();

	if ( tool_type == TOOL_POLYGON && poly_points > 1 )
	{
		if ( poly_status == POLY_DONE ) last++;		// Join 1st & last point if finished
		for ( i=0; i<=last; i++ )
		{
			for ( j=0; j<2; j++ )
			{
				co[j] = poly_mem[ i % (poly_points) ][j];
				co[j] = mt_round(co[j] * can_zoom + can_zoom/2);
						// Adjust for zoom
			}
			xy[i].x = co[0];
			xy[i].y = co[1];
		}
		gdk_draw_lines( drawing_canvas->window, dash_gc, xy, last+1 );
	}
}

void paint_marquee(int action, int new_x, int new_y)
{
	int x1, y1, x2, y2;
	int x, y, w, h, offx = 0, offy = 0;
	int rx, ry, rw, rh, canz = can_zoom, zerror = 0;
	int i, j, new_x2 = new_x + (marq_x2-marq_x1), new_y2 = new_y + (marq_y2-marq_y1);
	char *rgb;

	if ( canz<1 )
	{
		canz = 1;
		zerror = 2;
	}

	check_marquee();
	x1 = marq_x1*can_zoom; y1 = marq_y1*can_zoom;
	x2 = marq_x2*can_zoom; y2 = marq_y2*can_zoom;

	mtMIN( x, x1, x2 )
	mtMIN( y, y1, y2 )
	w = x1 - x2;
	h = y1 - y2;

	if ( w < 0 ) w = -w;
	if ( h < 0 ) h = -h;

	w = w + canz;
	h = h + canz;

	get_visible();

	if ( action == 0 )		// Clear marquee
	{
		j = marq_status;
		marq_status = 0;
		if ( j >= MARQUEE_PASTE && show_paste )
		{
			if ( new_x != marq_x1 || new_y != marq_y1 )
			{	// Only do something if there is a change
				if (	new_x2 < marq_x1 || new_x > marq_x2 ||
					new_y2 < marq_y1 || new_y > marq_y2	)
						repaint_canvas( x, y, w, h );	// Remove completely
				else
				{
					if ( new_x != marq_x1 )
					{	// Horizontal shift
						if ( new_x < marq_x1 )	// LEFT
						{
							ry = y; rh = h + zerror;
							rx = (new_x2 + 1) * can_zoom;
							rw = (marq_x2 - new_x2) * can_zoom + zerror;
						}
						else			// RIGHT
						{
							ry = y; rx = x; rh = h + zerror;
							rw = (new_x - marq_x1) * can_zoom + zerror;
						}
						clip_area( &rx, &ry, &rw, &rh );
						repaint_canvas( rx, ry, rw, rh );
					}
					if ( new_y != marq_y1 )
					{	// Vertical shift
						if ( new_y < marq_y1 )	// UP
						{
							rx = x; rw = w + zerror;
							ry = (new_y2 + 1) * can_zoom;
							rh = (marq_y2 - new_y2) * can_zoom + zerror;
						}
						else			// DOWN
						{
							rx = x; ry = y; rw = w + zerror;
							rh = (new_y - marq_y1) * can_zoom + zerror;
						}
						clip_area( &rx, &ry, &rw, &rh );
						repaint_canvas( rx, ry, rw, rh );
					}
				}
			}
		}
		else
		{
			repaint_canvas( x, y, 1, h );
			repaint_canvas(	x+w-1-zerror/2, y, 1+zerror, h );
			repaint_canvas(	x, y, w, 1 );
			repaint_canvas(	x, y+h-1-zerror/2, w, 1+zerror );
				// zerror required here to stop artifacts being left behind while dragging
				// a selection at the right/bottom edges
		}
		marq_status = j;
	}
	if ( action == 1 || action == 11 )		// Draw marquee
	{
		mtMAX( j, w, h )
		rgb = grab_memory( j*3, "marquee RGB redraw", 255 );

		if ( marq_status >= MARQUEE_PASTE )
		{
			if ( action == 1 && show_paste )
			{	// Display paste RGB, only if not being called from repaint_canvas
				if ( new_x != marq_x1 || new_y != marq_y1 )
				{	// Only do something if there is a change in position
					update_paste_chunk( x1+1, y1+1,
						x2 + canz-2, y2 + canz-2 );
				}
			}
			for ( i=0; i<j; i++ )
			{
				rgb[ 0 + 3*i ] = 255 * ((i/3) % 2);
				rgb[ 1 + 3*i ] = 255 * ((i/3) % 2);
				rgb[ 2 + 3*i ] = 255;
			}
		}
		else
		{
			for ( i=0; i<j; i++ )
			{
				rgb[ 0 + 3*i ] = 255;
				rgb[ 1 + 3*i ] = 255 * ((i/3) % 2);
				rgb[ 2 + 3*i ] = 255 * ((i/3) % 2);
			}
		}

		rx = x; ry = y; rw = w; rh = h;
		clip_area( &rx, &ry, &rw, &rh );

		if ( rx != x ) offx = 3*( abs(rx - x) );
		if ( ry != y ) offy = 3*( abs(ry - y) );

		if ( (rx + rw) >= mem_width*can_zoom ) rw = mem_width*can_zoom - rx;
		if ( (ry + rh) >= mem_height*can_zoom ) rh = mem_height*can_zoom - ry;

		if ( x >= vc_x1 )
		{
			gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
				rx, ry, 1, rh, GDK_RGB_DITHER_NONE, rgb + offy, 3 );
		}

		if ( (x+w-1) <= vc_x2 && (x+w-1) < mem_width*can_zoom )
		{
			gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
				rx+rw-1, ry, 1, rh, GDK_RGB_DITHER_NONE, rgb + offy, 3 );
		}

		if ( y >= vc_y1 )
		{
			gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
				rx, ry, rw, 1, GDK_RGB_DITHER_NONE, rgb + offx, 3*j );
		}

		if ( (y+h-1) <= vc_y2 && (y+h-1) < mem_height*can_zoom )
		{
			gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
				rx, ry+rh-1, rw, 1, GDK_RGB_DITHER_NONE, rgb + offx, 3*j );
		}

		free(rgb);
	}
}

int close_to( int x1, int y1 )		// Which corner of selection is coordinate closest to?
{
	int distance, xx[2], yy[2], i, closest[2];

	mtMIN( xx[0], marq_x1, marq_x2 )
	mtMAX( xx[1], marq_x1, marq_x2 )
	mtMIN( yy[0], marq_y1, marq_y2 )
	mtMAX( yy[1], marq_y1, marq_y2 )

	closest[0] = 0;
	closest[1] = (x1 - xx[0]) * (x1 - xx[0]) + (y1 - yy[0]) * (y1 - yy[0]);
	for ( i=1; i<4; i++ )
	{
		distance =	( x1 - xx[i % 2] ) * ( x1 - xx[i % 2] ) + 
				( y1 - yy[i / 2] ) * ( y1 - yy[i / 2] );
		if ( distance < closest[1] )
		{
			closest[0] = i;
			closest[1] = distance;
		}
	}

	return closest[0];
}

void update_sel_bar()			// Update selection stats on status bar
{
	char txt[64];
	int x1, y1, x2, y2;
	float lang = 0, llen = 1;

	if ( (tool_type == TOOL_SELECT || tool_type == TOOL_POLYGON) )
	{
		if ( marq_status > MARQUEE_NONE )
		{
			mtMIN( x1, marq_x1, marq_x2 )
			mtMIN( y1, marq_y1, marq_y2 )
			mtMAX( x2, marq_x1, marq_x2 )
			mtMAX( y2, marq_y1, marq_y2 )
			if ( status_on[5] )
			{
				if ( x1==x2 )
				{
					if ( marq_y1 < marq_y2 ) lang = 180;
				}
				else
				{
					lang = 90 + 180*atan( ((float) marq_y1 - marq_y2) /
						(marq_x1 - marq_x2) ) / M_PI;
					if ( marq_x1 > marq_x2 )
						lang = lang - 180;
				}

				llen = sqrt( (x2-x1+1)*(x2-x1+1) + (y2-y1+1)*(y2-y1+1) );

				snprintf(txt, 60, "  %i,%i : %i x %i   %.1f' %.1f\"",
					x1, y1, x2-x1+1, y2-y1+1, lang, llen);
				gtk_label_set_text( GTK_LABEL(label_bar5), txt );
			}
		}
		else
		{
			if ( tool_type == TOOL_POLYGON )
			{
				snprintf(txt, 60, "  (%i)", poly_points);
				if ( poly_status != POLY_DONE ) strcat(txt, "+");
				if ( status_on[5] ) gtk_label_set_text( GTK_LABEL(label_bar5), txt );
			}
			else if ( status_on[5] ) gtk_label_set_text( GTK_LABEL(label_bar5), "" );
		}
	}
	else if ( status_on[5] ) gtk_label_set_text( GTK_LABEL(label_bar5), "" );
}


void repaint_line(int mode)			// Repaint or clear line on canvas
{
	png_color pcol;
	int i, j, pixy = 1, xdo, ydo, px, py, todo, todor;
	int minx, miny, xw, yh, canz = can_zoom, canz2 = 1;
	int lx1, ly1, lx2, ly2,
		ax=-1, ay=-1, bx, by, aw, ah;
	float rat;
	char *rgb;
	gboolean do_redraw = FALSE;

	if ( canz<1 )
	{
		canz = 1;
		canz2 = mt_round(1/can_zoom);
	}
	pixy = canz*canz;
	lx1 = line_x1;
	ly1 = line_y1;
	lx2 = line_x2;
	ly2 = line_y2;

	xdo = abs(lx2 - lx1);
	ydo = abs(ly2 - ly1);
	mtMAX( todo, xdo, ydo )

	mtMIN( minx, lx1, lx2 )
	mtMIN( miny, ly1, ly2 )
	minx = minx * canz;
	miny = miny * canz;
	xw = (xdo + 1)*canz;
	yh = (ydo + 1)*canz;
	get_visible();
	clip_area( &minx, &miny, &xw, &yh );

	mtMIN( lx1, lx1 / canz2, mem_width / canz2 - 1 )
	mtMIN( ly1, ly1 / canz2, mem_height / canz2 - 1 )
	mtMIN( lx2, lx2 / canz2, mem_width / canz2 - 1 )
	mtMIN( ly2, ly2 / canz2, mem_height / canz2 - 1 )
	todo = todo / canz2;

	if ( todo == 0 ) todor = 1; else todor = todo;
	rgb = grab_memory( pixy*3, "repaint_line", 255 );

	for ( i=0; i<=todo; i++ )
	{
		rat = ((float) i ) / todor;
		px = mt_round(lx1 + (lx2 - lx1) * rat);
		py = mt_round(ly1 + (ly2 - ly1) * rat);

		if ( (px+1)*canz > vc_x1 && (py+1)*canz > vc_y1 &&
			px*canz <= vc_x2 && py*canz <= vc_y2 )
		{
			if ( mode == 2 )
			{
				pcol.red   = 255*( (todo-i)/4 % 2 );
				pcol.green = pcol.red;
				pcol.blue  = pcol.red;
			}
			if ( mode == 1 )
			{
				pcol.red   = mem_col_pat24[     3*((px % 8) + 8*(py % 8)) ];
				pcol.green = mem_col_pat24[ 1 + 3*((px % 8) + 8*(py % 8)) ];
				pcol.blue  = mem_col_pat24[ 2 + 3*((px % 8) + 8*(py % 8)) ];
			}

			if ( mode == 0 )
			{
				if ( ax<0 )	// 1st corner of repaint rectangle
				{
					ax = px;
					ay = py;
				}
				do_redraw = TRUE;
			}
			else
			{
				for ( j=0; j<pixy; j++ )
				{
					rgb[ 3*j ] = pcol.red;
					rgb[ 1 + 3*j ] = pcol.green;
					rgb[ 2 + 3*j ] = pcol.blue;
				}
				gdk_draw_rgb_image (the_canvas, drawing_canvas->style->black_gc,
					px*canz, py*canz, canz, canz,
					GDK_RGB_DITHER_NONE, rgb, 3*canz );
			}
		}
		else
		{
			if ( ax>=0 && mode==0 ) do_redraw = TRUE;
		}
		if ( do_redraw )
		{
			do_redraw = FALSE;
			bx = px;	// End corner
			by = py;
			aw = canz * (1 + abs(bx-ax));	// Width of rectangle on canvas
			ah = canz * (1 + abs(by-ay));
			if ( aw>16 || ah>16 || i==todo )
			{ // Commit canvas clear if >16 pixels or final pixel of this line
				mtMIN( ax, ax, bx )
				mtMIN( ay, ay, by )
				repaint_canvas( ax*canz, ay*canz, aw, ah );
				ax = -1;
			}
		}
	}
	free(rgb);
}

void init_status_bar()
{
	char txt[64];

	if ( status_on[0] || status_on[1] ) update_cols();
	if ( !status_on[0] )
	{
		gtk_label_set_text( GTK_LABEL(label_A), "" );
		gtk_label_set_text( GTK_LABEL(label_B), "" );
	}
	if ( !status_on[1] ) gtk_label_set_text( GTK_LABEL(label_bar1), "" );

	if ( status_on[2] ) gtk_widget_set_usize(label_bar2, 90, -2);
	else
	{
		gtk_widget_set_usize(label_bar2, 0, -2);
		gtk_label_set_text( GTK_LABEL(label_bar2), "" );
	}

	if ( status_on[3] ) gtk_widget_set_usize(label_bar3, 160, -2);
	else
	{
		gtk_widget_set_usize(label_bar3, 0, -2);
		gtk_label_set_text( GTK_LABEL(label_bar3), "" );
	}

	if (status_on[4])
	{
		sprintf(txt, "  %.0f%%", can_zoom*100);
		gtk_label_set_text( GTK_LABEL(label_bar4), txt );
	}
	else gtk_label_set_text( GTK_LABEL(label_bar4), "" );

	if ( !status_on[5] ) gtk_label_set_text( GTK_LABEL(label_bar5), "" );

	if ( mem_continuous && status_on[6] )
		gtk_label_set_text( GTK_LABEL(label_bar6), "  CON" );
	else
		gtk_label_set_text( GTK_LABEL(label_bar6), "" );

	if ( status_on[7] )
	{
		sprintf(txt, "%i+%i", mem_undo_done, mem_undo_redo);
		gtk_label_set_text( GTK_LABEL(label_bar7), txt );
		gtk_widget_set_usize(label_bar7, 50, -2);
	}
	else
	{
		gtk_widget_set_usize(label_bar7, 0, -2);
		gtk_label_set_text( GTK_LABEL(label_bar7), "" );
	}

	if ( status_on[8] )
	{
		sprintf(txt, "  %i%%", tool_opacity);
		gtk_label_set_text( GTK_LABEL(label_bar8), txt );
	}
	else gtk_label_set_text( GTK_LABEL(label_bar8), "" );

	if ( mem_undo_opacity && status_on[9] )
		gtk_label_set_text( GTK_LABEL(label_bar9), "  OP" );
	else
		gtk_label_set_text( GTK_LABEL(label_bar9), "" );
}


void men_item_visible( GtkWidget *menu_items[], gboolean state )
{	// Show or hide menu items
	int i = 0;
	while ( menu_items[i] != NULL )
	{
		if ( state )
			gtk_widget_show( menu_items[i] );
		else
			gtk_widget_hide( menu_items[i] );
		i++;
	}
}

void update_recent_files()			// Update the menu items
{
	char txt[64], *t, txt2[600];
	int i, count;

	if ( recent_files == 0 ) men_item_visible( menu_recent, FALSE );
	else
	{
		for ( i=0; i<=MAX_RECENT; i++ )			// Show or hide items
		{
			if ( i <= recent_files )
				gtk_widget_show( menu_recent[i] );
			else
				gtk_widget_hide( menu_recent[i] );
		}
		count = 0;
		for ( i=1; i<=recent_files; i++ )		// Display recent filenames
		{
			sprintf( txt, "file%i", i );

			t = inifile_get( txt, "." );
			if ( strlen(t) < 2 )
				gtk_widget_hide( menu_recent[i] );	// Hide if empty
			else
			{
#if GTK_MAJOR_VERSION == 2
				cleanse_txt( txt2, t );		// Clean up non ASCII chars
#else
				strcpy( txt2, t );
#endif
				gtk_label_set_text( GTK_LABEL( GTK_MENU_ITEM(
					menu_recent[i] )->item.bin.child ) , txt2 );
				count++;
			}
		}
		if ( count == 0 ) gtk_widget_hide( menu_recent[0] );	// Hide separator if not needed
	}
}

void register_file( char *filename )		// Called after successful load/save
{
	char txt[280], *c;
	int i, f;

	c = strrchr( filename, DIR_SEP );
	if (c != NULL)
	{
		txt[0] = c[1];
		c[1] = 0;
	}
	inifile_set("last_dir", filename);		// Strip off filename
	if (c != NULL) c[1] = txt[0];

	// Is it already in used file list?  If so shift relevant filenames down and put at top.
	i = 1;
	f = 0;
	while ( i<MAX_RECENT && f==0 )
	{
		sprintf( txt, "file%i", i );
		c = inifile_get( txt, "." );
		if ( strcmp( filename, c ) == 0 ) f = 1;	// Filename found in list
		else i++;
	}
	if ( i>1 )			// If file is already most recent, do nothing
	{
		while ( i>1 )
		{
			sprintf( txt, "file%i", i-1 );
			sprintf( txt+100, "file%i", i );
			inifile_set( txt+100,
				inifile_get( txt, "" )
				);

			i--;
		}
		inifile_set("file1", filename);		// Strip off filename
	}

	update_recent_files();
}

void scroll_wheel( int x, int y, int d )		// Scroll wheel action from mouse
{
	if ( d == 1 ) zoom_in( NULL, NULL ); else zoom_out( NULL, NULL );
}
