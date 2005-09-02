/*
 * Copyright (C) 2005 JCCM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more av.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author:  Roberto Majadas <roberto@molinux.info>
 */


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glade/glade.h>


FILE *f;
GIOChannel *io;
GladeXML *app;
GtkWidget *win;
GtkWidget *im;
GtkWidget *pb;
GtkWidget *label;
GtkWidget *hbox1;
gfloat x;
gfloat y;
gint z;
gchar *cmd;
gboolean installation_ok;
gint win_width;
gint win_height;
GdkScreen *screen;
gchar *branding_path;
gchar *last_package;

gboolean
apt_action_cb(GIOChannel * io, GIOCondition condition, gpointer data)
{
    GString *str;
    gchar *package;
    gchar *package_info;

    str = g_string_new("");
    
    g_io_channel_read_line_string(io, str, NULL, NULL);
    if (strncmp(str->str, "Unpacking", strlen("Unpacking")) == 0) {
	gchar **split_str;
	gchar *msg;
	split_str = g_strsplit(str->str, " ", 3);
	printf("Desempaquetando ---> %s\n", split_str[1]);
	msg = g_strdup_printf("Desempaquetando %s ...", split_str[1]);
	gtk_label_set_text(GTK_LABEL(label), msg);
	g_strfreev(split_str);
	g_free(msg);

    } else {
	if (strncmp(str->str, "Setting up", strlen("Setting up")) == 0) {
	    gchar **split_str;
	    gchar *msg;
	    split_str = g_strsplit(str->str, " ", 4);
	    printf("Configurando ---> %s\n", split_str[2]);

	    if (strcmp(split_str[2], last_package) == 0){
		gtk_widget_hide_all (hbox1);
		if (gdk_screen_get_width(screen) >= 1024){
		    gchar *big_pic;
		    big_pic = g_strdup_printf ("%s/reg_doc_big.jpg", branding_path);
		    gtk_image_set_from_file(GTK_IMAGE(im), big_pic);
		    g_free (big_pic);
		}else{
		    gchar *normal_pic;
		    normal_pic = g_strdup_printf ("%s/reg_doc.jpg", branding_path);
	            gtk_image_set_from_file(GTK_IMAGE(im), normal_pic);
		    g_free (normal_pic);
		}
	    }
	    else
	    {
		msg = g_strdup_printf("Configurando %s ...", split_str[2]);
	    	gtk_label_set_text(GTK_LABEL(label), msg);
	    	g_free(msg);
	    }
	    g_strfreev(split_str);
	    
	} else {
	    if (strncmp(str->str, "--end install ok--",
			strlen("--end install ok--")) == 0) {
		g_string_free(str, TRUE);
		installation_ok = TRUE;
		return FALSE;
	    }
	    if (strncmp(str->str, "--end install fail--",
			strlen("--end install fail--")) == 0) {
		g_string_free(str, TRUE);
		installation_ok = FALSE;
		return FALSE;
	    }
	    g_string_free(str, TRUE);
	    return TRUE;
	}
    }
    y = y + x;
    gtk_progress_set_value(GTK_PROGRESS(pb), y);
    if (y > z) {
	char *normal_pic ;
	char *big_pic ;
	
	switch (z) {
	case 20:
	case 40:
	case 60:
	case 80:
	    normal_pic = g_strdup_printf ("%s/corp_pic%i.jpg", branding_path, (z/20)+1) ;
	    big_pic = g_strdup_printf ("%s/corp_pic_big%i.jpg", branding_path, (z/20)+1) ;
	    printf ("-> XXL = %s\n->N = %s\n", big_pic, normal_pic);
	    if (gdk_screen_get_width(screen) >= 1024)
		gtk_image_set_from_file(GTK_IMAGE(im), big_pic);
	    else
		gtk_image_set_from_file(GTK_IMAGE(im), normal_pic);
	    
	    g_free (normal_pic);
	    g_free (big_pic);
	    break;
	}
	z = z + 20;
    }
    gtk_widget_show(pb);
    gtk_widget_show(label);
    g_string_free(str, TRUE);
    return TRUE;
}

void close_apt(gpointer data)
{
    g_print("Closing apt\n");
    pclose(f);
    if (installation_ok == TRUE)
	exit(0);
    else
	exit(1);
}

gchar * get_branding_path (void)
{
	return g_strdup (BRANDINGDIR "/Molinux/es/") ;
}

int main(int argc, char *argv[])
{

    gtk_init(&argc, &argv);

    if (argc != 4) {
	g_print
	    ("Usage uda-postinstall <num_of_total_packages> <packages> <last_package>\n\n"
	     "\t<num_of_total_packages> : This is the number of all packages that will be installated\n"
	     "\t<packages> : name of packages or meta-packages that install all software.\n "
	     "\tyou MUST write the names of this packages between double quotes. Example \"uda-desktop uda-base\"\n"
	     "\t<last_package> : The last package that will be installed in the installation process\n\n" );
	return;
    }

    branding_path = get_branding_path();
    last_package = g_strdup (argv[3]);

    app = glade_xml_new(UIDIR "uda-postinstall.glade", NULL, NULL);
    win = glade_xml_get_widget(app, "window_apt");
    pb = glade_xml_get_widget(app, "progressbar");
    im = glade_xml_get_widget(app, "image");
    label = glade_xml_get_widget(app, "label");
    hbox1 = glade_xml_get_widget(app, "hbox1");
    
    x = (100.0 / (atoi(argv[1]) * 2.0));
    y = 0;

    gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(pb),
				   GTK_PROGRESS_CONTINUOUS);
    gtk_progress_set_format_string(GTK_PROGRESS(pb), "%p%%");
    gtk_progress_set_show_text(GTK_PROGRESS(pb), TRUE);
    gtk_progress_set_value(GTK_PROGRESS(pb), y);

    z = 20;			/* Next percent for show the next corporative image */
    cmd =
	g_strdup_printf("LANG=C /usr/bin/apt-get -y --force-yes install %s\
						&& echo \"--end install ok--\"  \
						|| echo \"--end install fail--\"", argv[2]);
    f = popen(cmd, "r");
    io = g_io_channel_unix_new(fileno(f));
    g_io_add_watch_full(io, G_PRIORITY_LOW,
			G_IO_IN, apt_action_cb, NULL, close_apt);

    screen = gdk_screen_get_default();
    if (gdk_screen_get_width(screen) >= 1024){
	gchar *big_pic;
	big_pic = g_strdup_printf ("%s/corp_pic_big1.jpg", branding_path); 
	gtk_image_set_from_file(GTK_IMAGE(im), big_pic);
	g_free (big_pic);
    }
    else {
        gchar *normal_pic;
        normal_pic = g_strdup_printf ("%s/corp_pic1.jpg", branding_path);
	gtk_image_set_from_file(GTK_IMAGE(im), normal_pic);
	g_free (normal_pic);
    }
    gtk_widget_set_size_request(pb,
				gdk_screen_get_width(screen) * 80 / 100,
				-1);
    gtk_widget_show_all(win);
    gtk_window_fullscreen(GTK_WINDOW(win));
	
    gtk_main();
    
    g_free (branding_path);
    g_free (last_package);
    return 0;
}
