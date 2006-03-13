/*	layer.c
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

#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "memory.h"
#include "layer.h"
#include "mainwindow.h"
#include "otherwindow.h"
#include "canvas.h"
#include "mygtk.h"
#include "inifile.h"
#include "global.h"
#include "viewer.h"
#include "png.h"


int	layers_total = 0,		// Layers currently being used
	layer_selected = 0,		// Layer currently selected in the layers window
	layers_changed = 0,		// 0=Unchanged
	layer_w, layer_h;		// Layer geometry for saving

char	layers_filename[256];		// Current filename for layers file
unsigned char *layer_rgb;		// Memory containing composite layers image
gboolean show_layers_main = FALSE;	// Show all layers in main window

layer_node layer_table[MAX_LAYERS+1];	// Table of layer info


void layers_init()
{
	sprintf( layers_filename, _("Untitled") );
	sprintf( layer_table[0].name, _("Background") );
	layer_table[0].visible = TRUE;
	layer_table[0].use_trans = TRUE;
	layer_table[0].x = 0;
	layer_table[0].y = 0;
	layer_table[0].trans = 0;
	layer_table[0].opacity = 100;

	show_layers_main = inifile_get_gboolean( "layermainToggle", FALSE );
}

void repaint_layer(int l)			// Repaint layer in view/main window
{
	int lx = vw_zoom * layer_table[l].x, ly = vw_zoom * layer_table[l].y,
		lw = vw_zoom * mem_width, lh = vw_zoom * mem_height;

	if ( l != layer_selected )
	{
		lw = vw_zoom * layer_table[l].image->mem_width + 1;
		lh = vw_zoom * layer_table[l].image->mem_height + 1;
	}
	if ( view_window != NULL ) gtk_widget_queue_draw_area( vw_drawing, lx, ly, lw+1, lh+1 );

	if ( show_layers_main )
	{
		if ( layer_selected == 0 )
		{
			lx = 0;
			ly = 0;
		}
		else
		{
			lx = layer_table[layer_selected].x;
			ly = layer_table[layer_selected].y;
		}

		if ( l == 0 )
		{
			lx = -lx;
			ly = -ly;
		}
		else
		{
			lx = -lx + layer_table[l].x;
			ly = -ly + layer_table[l].y;
		}

		if ( l == layer_selected )
		{
			lw = can_zoom * mem_width;
			lh = can_zoom * mem_height;
		}
		else
		{
			lw = can_zoom * layer_table[l].image->mem_width + 1;
			lh = can_zoom * layer_table[l].image->mem_height + 1;
		}
		lx *= can_zoom;
		ly *= can_zoom;
		gtk_widget_queue_draw_area( drawing_canvas, lx, ly, lw, lh );
	}
}


///	LAYERS WINDOW

GtkWidget *layers_window = NULL;

static GtkWidget *layer_list, *entry_layer_name,
	*layer_buttons[10], *layer_spin, *layer_slider, *layer_list_data[MAX_LAYERS+1][3],
	*layer_label_position, *layer_trans_toggle, *layer_show_toggle;

gboolean layers_initialized;		// Indicates if initializing is complete



static void layers_update_titlebar()		// Update filename in titlebar
{
	char txt[300], txt2[600], *extra = "-";

	if ( layers_window == NULL ) return;		// Don't bother if window is not showing

#if GTK_MAJOR_VERSION == 2
	cleanse_txt( txt2, layers_filename );		// Clean up non ASCII chars
#else
	strcpy( txt2, layers_filename );
#endif

	if ( layers_changed == 1 ) extra = _("(Modified)");

	snprintf( txt, 290, "%s %s %s", _("Layers"), extra, txt2 );

	gtk_window_set_title (GTK_WINDOW (layers_window), txt );
}

void layers_notify_changed()			// Layers have just changed - update vars as needed
{
	if ( layers_changed != 1 )
	{
		layers_changed = 1;
		layers_update_titlebar();
	}
}

static void layers_notify_unchanged()		// Layers have just been unchanged (saved) - update vars as needed
{
	if ( layers_changed != 0 )
	{
		layers_changed = 0;
		layers_update_titlebar();
	}
}


static void layer_copy_from_main( int l )	// Copy info from main image to layer
{
	int i;

	for ( i=0; i<256; i++ )
	{
		layer_table[l].image->mem_filename[i]	= mem_filename[i];
		layer_table[l].image->mem_pal[i]	= mem_pal[i];
		layer_table[l].image->mem_prot_RGB[i]	= mem_prot_RGB[i];
		layer_table[l].image->mem_prot_mask[i]	= mem_prot_mask[i];
	}
	for ( i=0; i<MAX_UNDO; i++ )
	{
		layer_table[l].image->mem_undo_im[i]	= mem_undo_im[i];
	}

	layer_table[l].image->mem_image		= mem_image;
	layer_table[l].image->mem_image_bpp	= mem_image_bpp;
	layer_table[l].image->mem_changed	= mem_changed;
	layer_table[l].image->mem_width		= mem_width;
	layer_table[l].image->mem_height	= mem_height;
	layer_table[l].image->mem_ics		= mem_ics;
	layer_table[l].image->mem_icx		= mem_icx;
	layer_table[l].image->mem_icy		= mem_icy;

	layer_table[l].image->mem_undo_pointer	= mem_undo_pointer;
	layer_table[l].image->mem_undo_done	= mem_undo_done;
	layer_table[l].image->mem_undo_redo	= mem_undo_redo;

	layer_table[l].image->mem_cols		= mem_cols;
	layer_table[l].image->tool_pat		= tool_pat;
	layer_table[l].image->mem_col_A		= mem_col_A;
	layer_table[l].image->mem_col_B		= mem_col_B;
	layer_table[l].image->mem_col_A24	= mem_col_A24;
	layer_table[l].image->mem_col_B24	= mem_col_B24;

	layer_table[l].image->mem_xpm_trans	= mem_xpm_trans;
	layer_table[l].image->mem_xbm_hot_x	= mem_xbm_hot_x;
	layer_table[l].image->mem_xbm_hot_y	= mem_xbm_hot_y;

	layer_table[l].image->mem_prot		= mem_prot;
}

static void layer_copy_to_main( int l )		// Copy info from layer to main image
{
	int i;

	for ( i=0; i<256; i++ )
	{
		mem_filename[i] = layer_table[l].image->mem_filename[i];
		mem_pal[i] = layer_table[l].image->mem_pal[i];
		mem_prot_RGB[i] = layer_table[l].image->mem_prot_RGB[i];
		mem_prot_mask[i] = layer_table[l].image->mem_prot_mask[i];
	}
	for ( i=0; i<MAX_UNDO; i++ )
	{
		mem_undo_im[i] = layer_table[l].image->mem_undo_im[i];
	}

	mem_image	= layer_table[l].image->mem_image;
	mem_image_bpp	= layer_table[l].image->mem_image_bpp;
	mem_changed	= layer_table[l].image->mem_changed;
	mem_width	= layer_table[l].image->mem_width;
	mem_height	= layer_table[l].image->mem_height;
	mem_ics 	= layer_table[l].image->mem_ics;
	mem_icx 	= layer_table[l].image->mem_icx;
	mem_icy 	= layer_table[l].image->mem_icy;

	mem_undo_pointer = layer_table[l].image->mem_undo_pointer;
	mem_undo_done	= layer_table[l].image->mem_undo_done;
	mem_undo_redo	= layer_table[l].image->mem_undo_redo;

	mem_cols	= layer_table[l].image->mem_cols;
	tool_pat	= layer_table[l].image->tool_pat;
	mem_col_A	= layer_table[l].image->mem_col_A;
	mem_col_B	= layer_table[l].image->mem_col_B;
	mem_col_A24	= layer_table[l].image->mem_col_A24;
	mem_col_B24	= layer_table[l].image->mem_col_B24;

	mem_xpm_trans	= layer_table[l].image->mem_xpm_trans;
	mem_xbm_hot_x	= layer_table[l].image->mem_xbm_hot_x;
	mem_xbm_hot_y	= layer_table[l].image->mem_xbm_hot_y;

	mem_prot	= layer_table[l].image->mem_prot;
}

static void shift_layer(int val)
{
	layer_node temp = layer_table[layer_selected];
	int i, j, k;

	layer_table[layer_selected] = layer_table[layer_selected+val];
	layer_table[layer_selected+val] = temp;

	layer_selected += val;

		// Updated 2 list items - Text name + visible toggle
	mtMIN(j, layer_selected, layer_selected-val)
	mtMAX(k, layer_selected, layer_selected-val)
	for ( i=j; i<=k; i++ )
	{
		gtk_label_set_text( GTK_LABEL(layer_list_data[i][1]), layer_table[i].name );
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( layer_list_data[i][2] ),
				layer_table[i].visible);
	}

	gtk_list_select_child( GTK_LIST(layer_list), layer_list_data[layer_selected][0] );
	layers_notify_changed();
	if ( layer_selected == layers_total )
		gtk_widget_set_sensitive( layer_buttons[1], FALSE );	// Raise button
	if ( layer_selected == 0 )
		gtk_widget_set_sensitive( layer_buttons[2], FALSE );	// Lower button

	if ( val==1 ) gtk_widget_set_sensitive( layer_buttons[2], TRUE );	// Lower button
	if ( val==-1 ) gtk_widget_set_sensitive( layer_buttons[1], TRUE );	// Raise button

	update_cols();				// Update status bar info

	if ( layer_selected == 0 || (layer_selected-val) == 0 )
	{
		if ( view_window != NULL ) gtk_widget_queue_draw( vw_drawing );	// Update Whole window
		if ( show_layers_main ) gtk_widget_queue_draw(drawing_canvas);
	}
	else
	{
		repaint_layer(layer_selected);			// Update View window
	}
}

static gint layer_press_raise()
{
	shift_layer(1);
	return FALSE;
}

static gint layer_press_lower()
{
	shift_layer(-1);
	return FALSE;
}


void layer_new_chores( int l, int w, int h, int type, int cols,
			unsigned char *rgb, layer_image *lim )
{
	int bpp = type, i, j, k;
	png_color temp_pal[256];

	if ( marq_status > MARQUEE_NONE )	// If we are selecting or pasting - lose it!
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON]), TRUE );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
	}

	if ( type == 2 ) bpp = 1;	// Type 2 = greyscale indexed

	layer_table[l].image = lim;			// Image info memory pointer

	layer_table[l].name[0] = 0;
	layer_table[l].x = 0;
	layer_table[l].y = 0;
	layer_table[l].trans = 0;
	layer_table[l].opacity = 100;
	layer_table[l].visible = TRUE;
	layer_table[l].use_trans = TRUE;
	layer_table[l].image = lim;

	j = w * h * bpp;
	i = 0;
#ifdef U_GUADALINEX
	if ( bpp == 3 ) i = 255;
#endif
	for ( k=0; k<j; k++ ) rgb[k] = i;

	if ( type == 2 )	// Greyscale
	{
		mem_pal_copy( temp_pal, mem_pal );		// Save current
#ifdef U_GUADALINEX
		mem_scale_pal( 0, 255,255,255, cols-1, 0,0,0 );
#else
		mem_scale_pal( 0, 0,0,0, cols-1, 255,255,255 );
#endif
		mem_pal_copy( layer_table[l].image->mem_pal, mem_pal );
		mem_pal_copy( temp_pal, mem_pal );		// Restore
	}
	else mem_pal_copy( layer_table[l].image->mem_pal, mem_pal_def );

	sprintf( layer_table[l].image->mem_filename, _("Untitled") );

	layer_table[l].image->mem_image = rgb;
	layer_table[l].image->mem_image_bpp = bpp;
	layer_table[l].image->mem_changed = 0;
	layer_table[l].image->mem_width = w;
	layer_table[l].image->mem_height = h;
	layer_table[l].image->mem_ics = 0;
	layer_table[l].image->mem_icx = 0;
	layer_table[l].image->mem_icy = 0;

	layer_table[l].image->mem_undo_pointer = 0;
	layer_table[l].image->mem_undo_done = 0;
	layer_table[l].image->mem_undo_redo = 0;

	for ( i=1; i<MAX_UNDO; i++ )
		layer_table[l].image->mem_undo_im[i].image = NULL;

	layer_table[l].image->mem_undo_im[0].image = rgb;
	layer_table[l].image->mem_undo_im[0].cols = cols;
	layer_table[l].image->mem_undo_im[0].bpp = bpp;
	layer_table[l].image->mem_undo_im[0].width = w;
	layer_table[l].image->mem_undo_im[0].height = h;
	mem_pal_copy( layer_table[l].image->mem_undo_im[0].pal, layer_table[l].image->mem_pal );

	layer_table[l].image->mem_cols = cols;
	layer_table[l].image->tool_pat = 0;
	layer_table[l].image->mem_col_A = 1;
	layer_table[l].image->mem_col_B = 0;
	layer_table[l].image->mem_col_A24 = layer_table[l].image->mem_pal[1];
	layer_table[l].image->mem_col_B24 = layer_table[l].image->mem_pal[0];
	for ( i=0; i<256; i++ )
	{
		layer_table[l].image->mem_prot_RGB[i] = 0;
		layer_table[l].image->mem_prot_mask[i] = 0;
	}
	layer_table[l].image->mem_prot = 0;

	layer_table[l].image->mem_xpm_trans = -1;
	layer_table[l].image->mem_xbm_hot_x = -1;
	layer_table[l].image->mem_xbm_hot_y = -1;
}

void layer_new_chores2( int l )
{
	if ( layers_window != NULL )
	{
		gtk_widget_show( layer_list_data[l][0] );
		gtk_widget_set_sensitive( layer_list_data[l][0], TRUE );	// Enable list item

		gtk_label_set_text( GTK_LABEL(layer_list_data[l][1]), layer_table[l].name );
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( layer_list_data[l][2] ),
			layer_table[l].visible);

		gtk_list_select_child( GTK_LIST(layer_list), layer_list_data[l][0] );
		gtk_widget_set_sensitive( layer_buttons[1], FALSE );	// Raise button
		if ( l == 1 )
			gtk_widget_set_sensitive( layer_buttons[2], FALSE );	// Lower button
		else
			gtk_widget_set_sensitive( layer_buttons[2], TRUE );	// Lower button

		if ( l == MAX_LAYERS )			// Hide new/duplicate if we have max layers
		{
			gtk_widget_set_sensitive( layer_buttons[3], FALSE );
			gtk_widget_set_sensitive( layer_buttons[4], FALSE );
		}
		gtk_widget_set_sensitive( layer_buttons[7], TRUE );		// Remove all
	}

	layers_notify_changed();
}


void layer_new( int w, int h, int type, int cols )	// Types 1=indexed, 2=grey, 3=RGB
{
	int bpp=type;
	layer_image *lim;
	unsigned char *rgb;

	if ( layers_total>=MAX_LAYERS ) return;

	if ( layers_total == 0 ) layer_table[0].image = malloc( sizeof(layer_image) );
	layer_copy_from_main( layer_selected );

	if ( type == 2 ) bpp = 1;	// Type 2 = greyscale indexed

	rgb = malloc( w*h*bpp );

	if ( rgb == NULL )
	{
		memory_errors(1);
		return;
	}
	lim = malloc( sizeof(layer_image) );
	if ( lim == NULL )
	{
		free(rgb);
		memory_errors(1);
		return;
	}
	layers_total++;

	layer_new_chores( layers_total, w, h, type, cols, rgb, lim );
	layer_new_chores2( layers_total );
	layer_selected = layers_total;
}

static gint layer_press_new()
{
	generic_new_window(1);	// Call image new routine which will in turn call layer_new if needed

	return FALSE;
}

static gint layer_press_duplicate()
{
	layer_image *lim;
	unsigned char *rgb, *rgb_o = mem_image;
	int w = mem_width, h = mem_height, bpp = mem_image_bpp, cols = mem_cols, i, j;

	if ( layers_total>=MAX_LAYERS ) return FALSE;

	gtk_widget_set_sensitive( main_window, FALSE);		// Stop any user input
	if ( layers_window ) gtk_widget_set_sensitive( layers_window, FALSE);
			// This stops a nasty segfault if users does 2 quick duplicates

	if ( layers_total == 0 ) layer_table[0].image = malloc( sizeof(layer_image) );
	layer_copy_from_main( layer_selected );

	rgb = malloc( w*h*bpp );
	if ( rgb == NULL )
	{
		memory_errors(1);
		goto end;
	}
	lim = malloc( sizeof(layer_image) );
	if ( lim == NULL )
	{
		free(rgb);
		memory_errors(1);
		goto end;
	}
	layers_total++;

	layer_new_chores( layers_total, w, h, bpp, cols, rgb, lim);

	layer_table[layers_total] = layer_table[layer_selected];		// Copy layer info
	layer_table[layers_total].image = lim;

	for ( i=0; i<256; i++ )
	{
		layer_table[layers_total].image->mem_pal[i] =
			layer_table[layer_selected].image->mem_pal[i];
		layer_table[layers_total].image->mem_prot_RGB[i] =
			layer_table[layer_selected].image->mem_prot_RGB[i];
		layer_table[layers_total].image->mem_prot_mask[i] =
			layer_table[layer_selected].image->mem_prot_mask[i];
	}

	j = w*h*bpp;
	for ( i=0; i<j; i++ ) rgb[i] = rgb_o[i];		// Copy image pixels
	sprintf( layer_table[layers_total].image->mem_filename,
		layer_table[layer_selected].image->mem_filename );

	layer_new_chores2( layers_total );
	layer_selected = layers_total;

end:
	if ( layers_window ) gtk_widget_set_sensitive( layers_window, TRUE);
	gtk_widget_set_sensitive( main_window, TRUE);		// Restart user input
	gtk_list_select_child( GTK_LIST(layer_list), layer_list_data[layer_selected][0] );

	return FALSE;
}

static void layer_delete(int item)
{
	int i;

	for (i=0; i<MAX_UNDO; i++)		// Release old UNDO images
		if ( layer_table[item].image->mem_undo_im[i].image!=NULL )
		{
			free( layer_table[item].image->mem_undo_im[i].image );
		}

	free( layer_table[item].image );

	if ( item<layers_total )		// If deleted item is not at the end shuffle rest down
		for ( i=item; i<layers_total; i++ )
			layer_table[i] = layer_table[i+1];

	layers_total--;
	if ( layers_total == 0 )
	{
		free( layer_table[0].image );
	}

	layers_notify_changed();
	update_all_views();
}


static void layer_refresh_list()
{
	int i;

	if ( layers_window == NULL ) return;

	for ( i=0; i<=MAX_LAYERS; i++ )
	{
		if ( layers_total<i )		// Disable item
		{
			gtk_widget_hide( layer_list_data[i][0] );
			gtk_widget_set_sensitive( layer_list_data[i][0], FALSE );
		}
		else
		{
			gtk_widget_show( layer_list_data[i][0] );
			gtk_widget_set_sensitive( layer_list_data[i][0], TRUE );
			gtk_label_set_text( GTK_LABEL(layer_list_data[i][1]), layer_table[i].name );
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( layer_list_data[i][2] ),
					layer_table[i].visible);
		}
	}
	if ( layer_selected == layers_total )
		gtk_widget_set_sensitive( layer_buttons[1], FALSE );	// Raise button
	gtk_widget_set_sensitive( layer_buttons[3], TRUE );		// New
	gtk_widget_set_sensitive( layer_buttons[4], TRUE );		// Duplicate
	if ( layers_total < 1 )
		gtk_widget_set_sensitive( layer_buttons[7], FALSE );	// Remove all
	else
		gtk_widget_set_sensitive( layer_buttons[7], TRUE );	// Remove all
}

static gint layer_press_delete()
{
	char txt[128];
	int i, to_go = layer_selected;

	sprintf(txt, _("Do you really want to delete layer %i (%s) ?"), layer_selected, layer_table[layer_selected].name );

	i = alert_box( _("Warning"), txt, _("No"), _("Yes"), NULL );

	if ( i==2 )
	{
		i = check_for_changes();
		if ( i==2 || i==10 || i<10 )
		{
			gtk_list_select_child( GTK_LIST(layer_list),
				layer_list_data[layer_selected-1][0] );
			while (gtk_events_pending()) gtk_main_iteration();

			layer_delete( to_go );
			layer_refresh_list();
		}
	}

	return FALSE;
}

static void layer_show_position()
{
	char txt[35];

	if ( layers_window == NULL ) return;

	sprintf(txt, "%i , %i", layer_table[layer_selected].x, layer_table[layer_selected].y);
	gtk_label_set_text( GTK_LABEL(layer_label_position), txt );
}

static gint layer_press_centre()
{
	layer_table[layer_selected].x = layer_table[0].image->mem_width / 2 - mem_width / 2;
	layer_table[layer_selected].y = layer_table[0].image->mem_height / 2 - mem_height / 2;
	layer_show_position();
	layers_notify_changed();
	update_all_views();

	return FALSE;
}

int layers_unsaved_tot()			// Return number of layers with no filenames
{
	int j = 0, k;
	char *t;

	for ( k=0; k<=layers_total; k++ )	// Check each layer for proper filename
	{
		if ( k == layer_selected ) t = mem_filename;
		else t = layer_table[k].image->mem_filename;

		if ( strcmp( t, _("Untitled") ) == 0 ) j++;
	}

	return j;
}

int layers_changed_tot()			// Return number of layers with changes
{
	int j = 0, k;

	for ( k=0; k<=layers_total; k++ )	// Check each layer for mem_changed
	{
		if ( k == layer_selected )
		{
			j = j + mem_changed;
			if ( strcmp( mem_filename, _("Untitled") ) == 0 ) j++;
		}
		else
		{
			j = j + layer_table[k].image->mem_changed;
			if ( strcmp( layer_table[k].image->mem_filename, _("Untitled") ) == 0 ) j++;
		}
	}

	return j;
}

int check_layers_for_changes()			// 1=STOP, 2=IGNORE, 10=ESCAPE, -10=NOT CHECKED
{
	int i = -10, j = 0;
	char *warning = _("One or more of the layers contains changes that have not been saved.  Do you really want to lose these changes?");


	j = j + layers_changed_tot() + layers_changed;
	if ( j>0 )
		i = alert_box( _("Warning"), warning, _("Cancel Operation"), _("Lose Changes"), NULL );

	return i;
}

void layer_update_filename( char *name )
{
	strncpy(layers_filename, name, 250);
	layers_changed = 1;		// Forces update of titlebar
	layers_notify_unchanged();
}

void string_chop( char *txt )
{
	if ( strlen(txt) > 0 )		// Chop off unwanted non ASCII characters at end
	{
		while ( strlen(txt)>0 && txt[strlen(txt)-1]<32 ) txt[strlen(txt)-1] = 0;
	}
}

int read_file_num(FILE *fp, char *txt)
{
	int i = get_next_line(txt, 32, fp);

	if ( i<0 || i>30 ) return -987654321;
	sscanf(txt, "%i", &i);

	return i;
}

gboolean layers_check_header( char *file_name )
{
	char tin[64];
	int i;
	FILE *fp;


	if ((fp = fopen(file_name, "r")) == NULL) goto fail;

	i = get_next_line(tin, 32, fp);
	if ( i<0 || i>30 ) goto fail2;

	string_chop( tin );
	if ( strcmp( tin, LAYERS_HEADER ) != 0 ) goto fail2;		// Bad header

	i = read_file_num(fp, tin);
	if ( i==-987654321 ) goto fail2;
	if ( i>LAYERS_VERSION ) goto fail2;		// Version number must be compatible

	fclose(fp);
	return TRUE;
fail2:
	fclose(fp);
fail:
	return FALSE;
}

int load_layers( char *file_name )
{
	char tin[300], load_prefix[300], load_name[300], *c;
	unsigned char *lim;
	layer_image *lim2;
	int i, j, k=0, layers_to_read = -1, layer_file_version = -1;
	FILE *fp;

	strncpy( load_prefix, file_name, 256 );
	c = strrchr( load_prefix, DIR_SEP );
	if ( c!=NULL ) c[1]=0;
	else load_prefix[0]=0;

		// Try to save text file, return -1 if failure
	if ((fp = fopen(file_name, "r")) == NULL) goto fail;

	gtk_widget_hide( drawing_canvas );
		// This stops a nasty segfault caused by a redraw during the loading process

	i = get_next_line(tin, 32, fp);
	if ( i<0 || i>30 ) goto fail2;

	string_chop( tin );
	if ( strcmp( tin, LAYERS_HEADER ) != 0 ) goto fail2;		// Bad header

	i = read_file_num(fp, tin);
	if ( i==-987654321 ) goto fail2;
	layer_file_version = i;
	if ( i>LAYERS_VERSION ) goto fail2;		// Version number must be compatible

	i = read_file_num(fp, tin);
	if ( i==-987654321 ) goto fail2;
	layers_to_read = i;
	mtMIN( layers_to_read, layers_to_read, MAX_LAYERS )

	if ( layers_total>0 ) layers_remove_all();	// Remove all current layers if any
	for ( i=0; i<=layers_to_read; i++ )
	{
		// Read filename, strip end chars & try to load (if name length > 0)
		j = get_next_line(tin, 256, fp);
		string_chop(tin);
		snprintf(load_name, 260, "%s%s", load_prefix, tin);
		k = do_a_load(load_name);
		if ( k==0 )				// Load was successful
		{
			lim2 = malloc( sizeof(layer_image) );
			if ( lim2 == NULL )
			{
				memory_errors(1);	// We have run out of memory!
				layers_remove_all();	// Remove all layers - total meltdown!
				goto fail2;
			}
			else
			{
				layer_table[layers_total].image = lim2;
				layer_copy_from_main( layers_total );

				j = get_next_line(tin, 256, fp);
				string_chop(tin);
				strncpy(layer_table[layers_total].name, tin, 32); // Layer text name

				k = read_file_num(fp, tin);
				mtMAX(k, k, 0)
				mtMIN(k, k, 1)
				layer_table[layers_total].visible = k;

				layer_table[layers_total].x = read_file_num(fp, tin);
				layer_table[layers_total].y = read_file_num(fp, tin);

				k = read_file_num(fp, tin);
				mtMAX(k, k, 0)
				mtMIN(k, k, 1)
				layer_table[layers_total].use_trans = k;

				k = read_file_num(fp, tin);
				mtMAX(k, k, 0)
				mtMIN(k, k, 255)
				layer_table[layers_total].trans = k;

				k = read_file_num(fp, tin);
				mtMAX(k, k, 1)
				mtMIN(k, k, 100)
				layer_table[layers_total].opacity = k;

				layers_total++;

				mem_width = 1;
				mem_height = 1;
				mem_image = malloc(3);
				mem_undo_im[0].image = mem_image;	// Bogus 1x1 image used
			}
		}
		else		// Failed to load file
		{
			for ( j=0; j<7; j++ ) read_file_num(fp, tin);		// Skip this layer
		}
	}
	if ( layers_total>0 )
	{
		layers_total--;

		lim = mem_image;
		layer_copy_to_main(layers_total);
		free( lim );				// Lose excess bogus memory
		if ( layers_total == 0 )
		{	// You will need to free the memory holding first layer if just 1 was loaded
			free( layer_table[0].image );
		}
		layer_selected = layers_total;
		layer_refresh_list();
		if ( layers_window != NULL )
			gtk_list_select_child( GTK_LIST(layer_list),
				layer_list_data[layer_selected][0] );
	}
	else layer_refresh_list();

	fclose(fp);
	layer_update_filename( file_name );

	update_cols();		// Update status bar info

	gtk_widget_show( drawing_canvas );

	return 1;		// Success
fail:
	return -1;
fail2:
	gtk_widget_show( drawing_canvas );

	fclose(fp);
	return -1;
}

void parse_filename( char *dest, char *prefix, char *file )
{	// Convert absolute filename 'file' into one relative to prefix
	int i = 0, j = strlen(prefix), k;

	while ( i<j && prefix[i] == file[i] ) i++;	// # of chars that match at start

	if ( i>0 )
	{
		if ( i==j )				// File is at prefix level or below
			strncpy( dest, file+i, 256 );
		else					// File is below prefix level
		{
			dest[0]=0;
			k = i;
			while ( k<j )
			{
				if ( prefix[k] == DIR_SEP ) strncat( dest, "../", 256 );
				k++;
			} // Count number of DIR_SEP encountered on and after point i in 'prefix', add a '../' for each found
			k = i;
			while ( k>-1 && file[k]!=DIR_SEP )
			{
				k--;
			} // nip backwards on 'file' from i to previous DIR_SEP or beginning and ..

			strncat( dest, file+k+1, 256 );	// ... add rest of 'file'
		}
	}
	else	strncpy( dest, file, 256 );		// Prefix not in file at all, so copy all
}

int save_layers( char *file_name )
{
	char mess[300], comp_name[300], save_prefix[300], *c;
	int i;
	FILE *fp;

	strncpy( save_prefix, file_name, 256 );
	c = strrchr( save_prefix, DIR_SEP );
	if ( c!=NULL ) c[1]=0;
	else save_prefix[0]=0;

		// Try to save text file, return -1 if failure
	if ((fp = fopen(file_name, "w")) == NULL) goto fail;

	fprintf( fp, "%s\n%i\n%i\n", LAYERS_HEADER, LAYERS_VERSION, layers_total );
	for ( i=0; i<=layers_total; i++ )
	{
		if ( layer_selected == i )
		{
			parse_filename( comp_name, save_prefix, mem_filename );
		}
		else
		{
			parse_filename( comp_name, save_prefix, layer_table[i].image->mem_filename );
		}
		fprintf( fp, "%s\n", comp_name );

		fprintf( fp, "%s\n%i\n%i\n%i\n%i\n%i\n%i\n",
			layer_table[i].name, layer_table[i].visible, layer_table[i].x,
			layer_table[i].y, layer_table[i].use_trans, layer_table[i].trans,
			layer_table[i].opacity);
	}

	fclose(fp);
	layer_update_filename( file_name );
	register_file( file_name );		// Recently used file list / last directory

	if ( layer_selected != 0 )
	{
		layer_w = layer_table[0].image->mem_width;
		layer_h = layer_table[0].image->mem_height;
	}
	else
	{
		layer_w = mem_width;
		layer_h = mem_height;
	}

		// Create, save, free the composite image
	layer_rgb = malloc( layer_w * layer_h * 3);
	if ( layer_rgb != NULL )
	{
		sprintf(comp_name, "%s.png", file_name);			// Add .png to end
		view_render_rgb( layer_rgb, 0, 0, layer_w, layer_h, 1 );	// Render layer
		if ( save_png( comp_name, 3 ) < 0 )				// Save to PNG
			alert_box( _("Error"), _("Unable to save image"), _("OK"), NULL, NULL );
		free( layer_rgb );
	}
	else memory_errors(1);


	return 1;		// Success
fail:
	snprintf(mess, 260, _("Unable to save file: %s"), layers_filename);
	alert_box( _("Error"), mess, _("OK"), NULL, NULL );

	return -1;
}


int check_layers_all_saved()
{
	if ( layers_unsaved_tot() > 0 )
	{
		alert_box( _("Warning"), "One or more of the image layers has not been saved.  You must save each image individually before saving the layers text file in order to load this composite image in the future.", _("OK"), NULL, NULL );
		return 1;
	}

	return 0;
}

static gint layer_press_save_as()
{
	check_layers_all_saved();
	file_selector( FS_LAYER_SAVE );
// Use standard file_selector which in turn calls save_layers( char *file_name );

	return FALSE;
}

static gint layer_press_save()
{
	if ( strcmp( layers_filename, _("Untitled") ) == 0 )
	{
		layer_press_save_as();
	}
	else
	{
		check_layers_all_saved();
		save_layers( layers_filename );
	}

	return FALSE;
}

static void update_main_with_new_layer()
{
	gtk_widget_set_usize( drawing_canvas, mem_width*can_zoom, mem_height*can_zoom );
	update_all_views();

	init_pal();		// Update Palette, pattern & mask area + widgets
	gtk_widget_queue_draw(drawing_col_prev);

	update_titlebar();
	update_menus();
	if ( tool_type == TOOL_SMUDGE && mem_image_bpp == 1 )
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(icon_buttons[DEFAULT_TOOL_ICON]), TRUE );
}

void layers_remove_all()
{
	int i;

	gtk_widget_set_sensitive( main_window, FALSE);		// Stop any user input
	if ( layers_window ) gtk_widget_set_sensitive( layers_window, FALSE);

	if ( layers_window !=0 )
	{
		gtk_list_select_child( GTK_LIST(layer_list), layer_list_data[0][0] );
		while (gtk_events_pending()) gtk_main_iteration();
	}
	else
	{
		if ( layers_total>0 && layer_selected!=0 )	// Copy over layer 0
		{
			layer_copy_to_main(0);
			update_main_with_new_layer();
		}
	}

	for ( i=layers_total; i>0; i-- )
	{
		layer_delete(i);
	}
	layer_refresh_list();
	sprintf(layers_filename, _("Untitled"));
	layers_notify_unchanged();

	if ( layers_window ) gtk_widget_set_sensitive( layers_window, TRUE);
	gtk_widget_set_sensitive( main_window, TRUE);		// Restart user input
}

static gint layer_press_remove()
{
	int i;

	i = check_layers_for_changes();
	if ( i == 2 || i < 0 )
	{
		if ( i<0 ) i = alert_box( _("Warning"), _("Do you really want to delete all of the layers?"), _("No"), _("Yes"), NULL );
		if ( i==2 )
		{
			layers_remove_all();
		}
	}

	return FALSE;
}


static gint layer_tog_visible( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	int i, j=-1;
	gboolean k=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	if ( !layers_initialized ) return TRUE;

	for ( i=1; i<=layers_total; i++ ) if ( widget == layer_list_data[i][2] ) j = i;

	if ( j>0 )
	{
		layer_table[j].visible = k;
		layers_notify_changed();
		repaint_layer(j);
	}

	return FALSE;
}

static gint layer_main_toggled( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	show_layers_main = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(layer_show_toggle) );
	inifile_set_gboolean( "layermainToggle", show_layers_main );
	gtk_widget_queue_draw(drawing_canvas);

	return FALSE;
}

static gint layer_inputs_changed( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	gboolean txt_changed = FALSE;

	if ( !layers_initialized ) return FALSE;

	layers_notify_changed();

	layer_table[layer_selected].trans =
			gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(layer_spin) );
	layer_table[layer_selected].opacity =
			GTK_HSCALE(layer_slider)->scale.range.adjustment->value;

	if ( strncmp( layer_table[layer_selected].name,
		gtk_entry_get_text( GTK_ENTRY(entry_layer_name) ), 35) ) txt_changed = TRUE;

	strncpy( layer_table[layer_selected].name,
		gtk_entry_get_text( GTK_ENTRY(entry_layer_name) ), 35);
	gtk_label_set_text( GTK_LABEL(layer_list_data[layer_selected][1]),
		layer_table[layer_selected].name );
	layer_table[layer_selected].use_trans = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(layer_trans_toggle));

	if ( !txt_changed ) repaint_layer( layer_selected );
		// Update layer image if not just changing text

	return FALSE;
}

void layer_choose( int l )				// Select a new layer from the list
{
	if ( l<=layers_total && l>=0 && l != layer_selected )	// Change selected layer if different
	{
		if ( layers_window == NULL )
		{
			layer_copy_from_main( layer_selected );
					// Copy image info to layer table before we change
			layer_selected = l;
			if ( tool_type == TOOL_SELECT && marq_status >= MARQUEE_PASTE )
				pressed_select_none( NULL, NULL );

			layer_copy_to_main( layer_selected );
			update_main_with_new_layer();
		}
		else
		{
			gtk_list_select_child( GTK_LIST(layer_list),
				layer_list_data[l][0] );
		}
	}
}

static gint layer_select( GtkList *list, GtkWidget *widget, gpointer user_data )
{
	gboolean dont_update = FALSE;
	int i, j=-1;

	if ( !layers_initialized ) return TRUE;

	layers_initialized = FALSE;

	for ( i=0; i<(MAX_LAYERS+1); i++ )
	{
		if ( widget == layer_list_data[i][0] ) j=i;
	}

	if ( j==layer_selected )
		dont_update=TRUE;	// Already selected e.g. raise, lower, startup

	if ( j>-1 && entry_layer_name != NULL && j<=layers_total )
	{
		if ( !dont_update ) layer_copy_from_main( layer_selected );
					// Copy image info to layer table before we change

		gtk_entry_set_text( GTK_ENTRY(entry_layer_name), layer_table[j].name );
		layer_selected = j;
		if ( j==0 )		// Background layer selected
		{
			if ( layers_total>0 )
			 gtk_widget_set_sensitive( layer_buttons[1], TRUE );	// Raise button
			else
			 gtk_widget_set_sensitive( layer_buttons[1], FALSE );	// Raise button
			gtk_widget_set_sensitive( layer_buttons[2], FALSE );	// Lower button
			gtk_widget_set_sensitive( layer_buttons[5], FALSE );	// Delete button
			gtk_widget_set_sensitive( layer_buttons[6], FALSE );	// Centre button
			gtk_widget_set_sensitive( layer_trans_toggle, FALSE );
			gtk_widget_set_sensitive( layer_spin, FALSE );
			gtk_label_set_text( GTK_LABEL(layer_label_position), _("Background") );

			gtk_widget_set_sensitive( layer_slider, FALSE );
		}
		else
		{
			gtk_widget_set_sensitive( layer_slider, TRUE );
			gtk_adjustment_set_value( GTK_HSCALE(layer_slider)->scale.range.adjustment,
				layer_table[layer_selected].opacity );
			layer_show_position();
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( layer_trans_toggle ),
				layer_table[layer_selected].use_trans);

			gtk_widget_set_sensitive( layer_buttons[1], TRUE );	// Raise button
			gtk_widget_set_sensitive( layer_buttons[2], TRUE );	// Lower button
			gtk_widget_set_sensitive( layer_buttons[5], TRUE );	// Delete button
			gtk_widget_set_sensitive( layer_buttons[6], TRUE );	// Centre button
			gtk_widget_set_sensitive( layer_trans_toggle, TRUE );

			gtk_widget_set_sensitive( layer_spin, TRUE );
			gtk_spin_button_set_value( GTK_SPIN_BUTTON(layer_spin), layer_table[j].trans);

			if ( j==layers_total )		// Top layer
			{
				gtk_widget_set_sensitive( layer_buttons[1], FALSE );	// Raise button
			}
		}
		
	}

	while (gtk_events_pending()) gtk_main_iteration();
	layers_initialized = TRUE;

	if ( !dont_update )
	{
		if ( tool_type == TOOL_SELECT && marq_status >= MARQUEE_PASTE )
			pressed_select_none( NULL, NULL );

		layer_copy_to_main( layer_selected );
		update_main_with_new_layer();
	}

	return FALSE;
}

gint delete_layers_window( GtkWidget *widget, GdkEvent *event, gpointer data )
{
	int x, y, width, height;

	gdk_window_get_size( layers_window->window, &width, &height );
	gdk_window_get_root_origin( layers_window->window, &x, &y );
	
	inifile_set_gint32("layers_x", x );
	inifile_set_gint32("layers_y", y );
	inifile_set_gint32("layers_w", width );
	inifile_set_gint32("layers_h", height );

	gtk_widget_destroy(layers_window);
	men_item_state(menu_layer, TRUE);
	layers_window = NULL;

	return FALSE;
}

void pressed_paste_layer( GtkMenuItem *menu_item, gpointer user_data )
{
	int ol = layer_selected;

	if ( layers_total<MAX_LAYERS )
	{
		gtk_widget_set_sensitive( main_window, FALSE);		// Stop any user input
		if ( layers_window ) gtk_widget_set_sensitive( layers_window, FALSE);
				// This stops a nasty segfault if users does 2 quick paste layers

		layer_new( mem_clip_w, mem_clip_h, mem_clip_bpp, mem_cols );
		if ( layer_selected != ol )	// If == then new layer wasn't created
		{
			layer_table[layer_selected].x = mem_clip_x;
			layer_table[layer_selected].y = mem_clip_y;
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(icon_buttons[PAINT_TOOL_ICON]),
				TRUE );			// Change tool

			mem_pal_copy(layer_table[layer_selected].image->mem_pal,
				layer_table[ol].image->mem_pal);		// Copy palette

			layer_table[layer_selected].image->tool_pat =
				layer_table[ol].image->tool_pat;
			layer_table[layer_selected].image->mem_col_A =
				layer_table[ol].image->mem_col_A;
			layer_table[layer_selected].image->mem_col_B =
				layer_table[ol].image->mem_col_B;
			layer_table[layer_selected].image->mem_col_A24 =
				layer_table[ol].image->mem_col_A24;
			layer_table[layer_selected].image->mem_col_B24 =
				layer_table[ol].image->mem_col_B24;

			layer_copy_to_main( layer_selected );
			update_main_with_new_layer();
			f_rectangle( 0, 0, mem_width, mem_height );
			marq_x1=0;
			marq_y1=0;
			commit_paste(FALSE);					// Paste
			if ( layers_window == NULL ) pressed_layers( NULL, NULL );
			if ( view_window == NULL ) pressed_view( NULL, NULL );

		}
		if ( layers_window ) gtk_widget_set_sensitive( layers_window, TRUE);
		gtk_widget_set_sensitive( main_window, TRUE);		// Restart user input
	}
	else alert_box( _("Error"), _("You cannot add any more layers."), _("OK"), NULL, NULL );
}

void move_layer_relative(int l, int change_x, int change_y)	// Move a layer & update window labels
{
	int lx = layer_table[l].x, ly = layer_table[l].y, lw = mem_width, lh = mem_height;

	layer_table[l].x += change_x;
	layer_table[l].y += change_y;
	if ( l==layer_selected ) layer_show_position();
	else
	{
		lw = layer_table[l].image->mem_width;
		lh = layer_table[l].image->mem_height;
	}
	layers_notify_changed();

	if ( view_window != NULL )
	{
		mtMIN(lx, lx, lx+change_x)
		mtMIN(ly, ly, ly+change_y)
		gtk_widget_queue_draw_area( vw_drawing,
			lx*vw_zoom, ly*vw_zoom,
			(lw + abs(change_x))*vw_zoom+1, (lh + abs(change_y) )*vw_zoom+1 );
	}
	if ( show_layers_main ) gtk_widget_queue_draw(drawing_canvas);
}

void pressed_layers( GtkMenuItem *menu_item, gpointer user_data )
{
	GtkWidget *vbox, *hbox, *table, *label, *tog, *scrolledwindow;
	GtkAccelGroup* ag = gtk_accel_group_new();
	char txt[256];
	int i;

	men_item_state(menu_layer, FALSE);

	entry_layer_name = NULL;
	layers_initialized = FALSE;


	layers_window = add_a_window( GTK_WINDOW_TOPLEVEL, "", GTK_WIN_POS_NONE, FALSE );
	gtk_window_set_default_size( GTK_WINDOW(layers_window),
		inifile_get_gint32("layers_w", 400 ), inifile_get_gint32("layers_h", 400 ) );
	gtk_widget_set_uposition( layers_window,
		inifile_get_gint32("layers_x", 0 ), inifile_get_gint32("layers_y", 0 ) );

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (layers_window), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow);
	gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	layer_list = gtk_list_new ();
	gtk_signal_connect( GTK_OBJECT(layer_list), "select_child",
			GTK_SIGNAL_FUNC(layer_select), NULL );
	gtk_widget_show (layer_list);

	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(scrolledwindow), layer_list);

	for ( i=MAX_LAYERS; i>=0; i-- )
	{
		hbox = gtk_hbox_new( FALSE, 3 );
		gtk_widget_show( hbox );

		layer_list_data[i][0] = gtk_list_item_new();
		gtk_container_add( GTK_CONTAINER(layer_list), layer_list_data[i][0] );
		gtk_container_add( GTK_CONTAINER(layer_list_data[i][0]), hbox );
		gtk_widget_show( layer_list_data[i][0] );

		sprintf(txt, "%s %i   ", _("Layer"), i);
		label = gtk_label_new( txt );
		gtk_widget_show( label );
		gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );
		gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 0 );

		label = gtk_label_new( "" );
		gtk_widget_show( label );
		gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );
		gtk_box_pack_start( GTK_BOX(hbox), label, TRUE, TRUE, 0 );
		layer_list_data[i][1] = label;

		tog = gtk_check_button_new_with_label("");
		gtk_widget_show( tog );
		gtk_box_pack_start (GTK_BOX(hbox), tog, FALSE, FALSE, 0);
		layer_list_data[i][2] = tog;
		if ( i == 0 ) gtk_widget_hide(tog);
		else gtk_signal_connect(GTK_OBJECT(tog), "clicked",
			GTK_SIGNAL_FUNC(layer_tog_visible), NULL);
	}

	for ( i=0; i<=MAX_LAYERS; i++ )
	{
		if ( i<=layers_total )
		{
			gtk_label_set_text( GTK_LABEL(layer_list_data[i][1]), layer_table[i].name );
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( layer_list_data[i][2] ),
				layer_table[i].visible);
		}
		else
		{
			gtk_widget_hide( layer_list_data[i][0] );
			gtk_widget_set_sensitive( layer_list_data[i][0], FALSE );
			layer_table[i].image = NULL;		// Needed for checks later
		}
	}

	layer_iconbar(layers_window, vbox, layer_buttons);

	if ( layers_total == MAX_LAYERS )	// Hide new/duplicate if we have max layers
	{
		gtk_widget_set_sensitive( layer_buttons[3], FALSE );
		gtk_widget_set_sensitive( layer_buttons[4], FALSE );
	}

	table = add_a_table( 3, 2, 5, vbox );
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);

	add_to_table( _("Layer Name"), table, 0, 0, 0, GTK_JUSTIFY_LEFT, 0, 0.5 );
	add_to_table( _("Position"), table, 1, 0, 0, GTK_JUSTIFY_LEFT, 0, 0.5 );
	add_to_table( _("Opacity"), table, 2, 0, 0, GTK_JUSTIFY_LEFT, 0, 0.5 );

	entry_layer_name = gtk_entry_new_with_max_length (32);
	gtk_widget_set_usize(entry_layer_name, 100, -2);
	gtk_widget_show (entry_layer_name);
	gtk_table_attach (GTK_TABLE (table), entry_layer_name, 1, 2, 0, 1,
		(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_signal_connect( GTK_OBJECT(entry_layer_name),
			"changed", GTK_SIGNAL_FUNC(layer_inputs_changed), NULL);

	layer_label_position = add_to_table( "-320, 200", table, 1, 1, 1,
			GTK_JUSTIFY_LEFT, 0, 0.5 );

	layer_slider = add_slider2table( 100, 1, 100, table, 2, 1, -2, -2 );
	gtk_signal_connect( GTK_OBJECT(GTK_HSCALE(layer_slider)->scale.range.adjustment),
		"value_changed", GTK_SIGNAL_FUNC(layer_inputs_changed), NULL);
	gtk_scale_set_draw_value(GTK_SCALE (layer_slider), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE (layer_slider), GTK_POS_RIGHT);
	gtk_adjustment_set_value( GTK_HSCALE(layer_slider)->scale.range.adjustment,
		layer_table[layer_selected].opacity );

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	layer_trans_toggle = add_a_toggle( _("Transparent Colour"), hbox, TRUE );
	gtk_signal_connect(GTK_OBJECT(layer_trans_toggle), "clicked",
			GTK_SIGNAL_FUNC(layer_inputs_changed), NULL);

	layer_spin = add_a_spin(0, 0, 255);
#if GTK_MAJOR_VERSION == 2
	gtk_signal_connect( GTK_OBJECT( &GTK_SPIN_BUTTON(layer_spin)->entry ),
			"value_changed", GTK_SIGNAL_FUNC(layer_inputs_changed), NULL);
#endif
#if GTK_MAJOR_VERSION == 1
	gtk_signal_connect( GTK_OBJECT( &GTK_SPIN_BUTTON(layer_spin)->entry ),
			"changed", GTK_SIGNAL_FUNC(layer_inputs_changed), NULL);
#endif
	gtk_box_pack_start (GTK_BOX (hbox), layer_spin, FALSE, FALSE, 0);

	layer_show_toggle = add_a_toggle( _("Show all layers in main window"), vbox, show_layers_main );
	gtk_signal_connect(GTK_OBJECT(layer_show_toggle), "clicked",
			GTK_SIGNAL_FUNC(layer_main_toggled), NULL);

	if ( layers_total == 0 ) gtk_widget_set_sensitive( layer_buttons[7], FALSE );
		// Delete all layers button
	gtk_widget_add_accelerator (layer_buttons[8], "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);

	gtk_signal_connect_object (GTK_OBJECT (layers_window), "delete_event",
		GTK_SIGNAL_FUNC (delete_layers_window), NULL);

	gtk_window_set_transient_for( GTK_WINDOW(layers_window), GTK_WINDOW(main_window) );
	gtk_widget_show(layers_window);
	gtk_window_add_accel_group(GTK_WINDOW (layers_window), ag);

	layers_initialized = TRUE;
	gtk_list_select_child( GTK_LIST(layer_list), layer_list_data[layer_selected][0] );

	layers_update_titlebar();
}


void layer_iconbar_click(GtkWidget *widget, gpointer data)
{
	gint j = (gint) data;

	switch (j)
	{
		case 0:	layer_press_new(); break;
		case 1:	layer_press_raise(); break;
		case 2:	layer_press_lower(); break;
		case 3:	layer_press_duplicate(); break;
		case 4:	layer_press_centre(); break;
		case 5:	layer_press_delete(); break;
		case 6:	layer_press_save(); break;
		case 7:	layer_press_save_as(); break;
		case 8:	layer_press_remove(); break;
		case 9:	delete_layers_window(NULL,NULL,NULL); break;
	}
}
