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

GladeXML *app;
GtkWidget *win, *main_vbox, *help_tv, *adm_img, *comp_img, *soft_img,
    *ok_button;
GtkWidget *event_box, *title_label;
GtkWidget *name_entry, *passwd_entry, *passwd2_entry;
GtkWidget *cname_entry;
GtkWidget *main_repo_check, *sec_repo_check, *extra_repo_check;
GdkColor color;
GdkScreen *screen;
GtkTextBuffer *buffer;

gchar *name = NULL;
gchar *passwd = NULL;
gchar *passwd2 = NULL;
gchar *cname = NULL;

void validate_info()
{
    if (name == NULL || passwd == NULL || passwd2 == NULL || cname == NULL) {
	gtk_widget_set_sensitive(ok_button, FALSE);
	return;
    }
    if (strlen(name) == 0) {
	gtk_widget_set_sensitive(ok_button, FALSE);
	return;
    }

    if (strcmp(name, "root") == 0) {
	gtk_widget_set_sensitive(ok_button, FALSE);
	return;
    }

    if (strcmp(passwd, passwd2) != 0) {
	gtk_widget_set_sensitive(ok_button, FALSE);
	return;
    }
    if (strlen(cname) == 0) {
	gtk_widget_set_sensitive(ok_button, FALSE);
	return;
    }
    gtk_widget_set_sensitive(ok_button, TRUE);
}

void name_entry_changed(GtkEditable * editable)
{
    if (name != NULL)
	g_free(name);
    name = gtk_editable_get_chars(editable, 0, -1);
    validate_info();
}

void
names_entry_insert(GtkEditable * editable,
		   gchar * new_text,
		   gint new_text_length,
		   gint * position, gpointer user_data)
{
    gint pos = *position;
    if (new_text_length > 1) {
	g_signal_handlers_block_by_func(editable,
					(gpointer) names_entry_insert,
					user_data);
	gtk_editable_delete_text(editable, pos, pos + strlen(new_text));
	g_signal_handlers_unblock_by_func(editable,
					  (gpointer) names_entry_insert,
					  user_data);
	g_signal_stop_emission_by_name(editable, "insert_text");
    } else {
	if (strcmp(new_text, " ") == 0) {
	    g_signal_handlers_block_by_func(editable,
					    (gpointer) names_entry_insert,
					    user_data);
	    gtk_editable_delete_text(editable, pos, pos + 1);
	    g_signal_handlers_unblock_by_func(editable, (gpointer)
					      names_entry_insert,
					      user_data);
	    g_signal_stop_emission_by_name(editable, "insert_text");
	}
    }
}

void passwd_entry_changed(GtkEditable * editable)
{
    if (passwd != NULL)
	g_free(passwd);
    passwd = gtk_editable_get_chars(editable, 0, -1);
    validate_info();
}

void passwd2_entry_changed(GtkEditable * editable)
{
    if (passwd2 != NULL)
	g_free(passwd2);
    passwd2 = gtk_editable_get_chars(editable, 0, -1);
    validate_info();
}

void cname_entry_changed(GtkEditable * editable)
{
    if (cname != NULL)
	g_free(cname);
    cname = gtk_editable_get_chars(editable, 0, -1);
    validate_info();
}

ok_button_cb(GtkButton * button, gpointer user_data)
{

    printf("USERNAME=\"%s\"\nPASSWD=\"%s\"\nCOMPUTER_NAME=\"%s\"\n",
	   name, passwd, cname);
    g_free(name);
    g_free(passwd);
    g_free(passwd2);
    g_free(cname);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_repo_check)))
	printf("MAIN_REPO=\"yes\"\n");
    else
	printf("MAIN_REPO=\"no\"\n");

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sec_repo_check)))
	printf("SEC_REPO=\"yes\"\n");
    else
	printf("SEC_REPO=\"no\"\n");

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(extra_repo_check)))
	printf("EXTRA_REPO=\"yes\"\n");
    else
	printf("EXTRA_REPO=\"no\"\n");
    gtk_main_quit();
}

void set_help_text(void)
{
    GtkTextIter iter;

    gtk_text_buffer_create_tag(buffer, "heading",
			       "weight", PANGO_WEIGHT_BOLD,
			       "size", 15 * PANGO_SCALE, NULL);
    gtk_text_buffer_create_tag(buffer, "x-large",
			       "scale", PANGO_SCALE_X_LARGE, NULL);
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "AYUDA\n\n",
					     -1, "heading", NULL);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
					     "Usuario administrador\n", -1,
					     "x-large", NULL);
    gtk_text_buffer_insert(buffer, &iter,
			   "El usuario administrador, es un usuario que "
			   "tiene capacidades especiales con respecto al resto de los "
			   "usuarios. Desde este usuario se tiene la posibilidad de "
			   "acceder a las herramientas de administración (instalacion de "
			   "software, herramientas de configuracion ...).\n\n",
			   -1);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "Mi equipo\n",
					     -1, "x-large", NULL);
    gtk_text_buffer_insert(buffer, &iter,
			   "En sistemas como este es habitual ponerle un "
			   "nombre al equipo para poder identificarlo. "
			   "Puede poner como nombre \"casa\", \"oficina\" o cualquier otro "
			   "nombre que se le ocurra , siempre y cuando sea una sola palabra"
			   ".\n\n", -1);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
					     "Repositorios en internet\n",
					     -1, "x-large", NULL);
    gtk_text_buffer_insert(buffer, &iter,
			   "Si usted dispone de conexión a Internet podra "
			   "utilizar nuestros repositorios de manera libre y gratuita. "
			   "Un repositorio es un lugar en Internet desde el cual usted "
			   "podra actualizar sus programas e instalar programas nuevos. "
			   "Esto se realiza a través de la herramienta de gestión de "
			   "software que dispone su sistema.", -1);
}

int main(int argc, char *argv[])
{

    gtk_init(&argc, &argv);

    app = glade_xml_new(UIDIR "uda-postconfig.glade", NULL, NULL);
    win = glade_xml_get_widget(app, "uda_postconfig_win");
    ok_button = glade_xml_get_widget(app, "ok_button");
    gtk_widget_set_sensitive(ok_button, FALSE);
    main_vbox = glade_xml_get_widget(app, "main_vbox");
    event_box = gtk_event_box_new();
    title_label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(title_label),
			 "<span size=\"x-large\" weight=\"bold\" foreground=\"white\">INFORMACIÓN DEL SISTEMA</span>");
    gtk_container_add(GTK_CONTAINER(event_box), title_label);
    gtk_box_pack_start(GTK_BOX(main_vbox), event_box, FALSE, TRUE, 0);
    gtk_box_reorder_child(GTK_BOX(main_vbox), event_box, 0);
    gdk_color_parse("#7b96ad", &color);
    gtk_widget_modify_bg(event_box, GTK_STATE_NORMAL, &color);
    gtk_widget_set_size_request(GTK_WIDGET(title_label), -1, 40);
    help_tv = glade_xml_get_widget(app, "help_text_view");
    adm_img = glade_xml_get_widget(app, "image_superuser");
    gtk_image_set_from_file(GTK_IMAGE(adm_img), UIDIR "admin.png");
    comp_img = glade_xml_get_widget(app, "image_computer");
    gtk_image_set_from_file(GTK_IMAGE(comp_img), UIDIR "computer.png");
    soft_img = glade_xml_get_widget(app, "image_repo");
    gtk_image_set_from_file(GTK_IMAGE(soft_img), UIDIR "software.png");
    name_entry = glade_xml_get_widget(app, "root_name_entry");

    passwd_entry = glade_xml_get_widget(app, "root_passwd_entry");
    passwd2_entry = glade_xml_get_widget(app, "root_passwd2_entry");
    cname_entry = glade_xml_get_widget(app, "computer_name_entry");
    main_repo_check = glade_xml_get_widget(app, "main_repo_check");
    sec_repo_check = glade_xml_get_widget(app, "security_repo_check");
    extra_repo_check = glade_xml_get_widget(app, "extra_repo_check");
    screen = gdk_screen_get_default();
    gtk_widget_set_size_request(help_tv,
				gdk_screen_get_width(screen) * 30 / 100,
				-1);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(help_tv));
    set_help_text();

    /* Signals */
    g_signal_connect(G_OBJECT(GTK_EDITABLE(name_entry)), "changed",
		     G_CALLBACK(name_entry_changed), NULL);
    g_signal_connect(G_OBJECT(GTK_EDITABLE(name_entry)), "insert_text",
		     G_CALLBACK(names_entry_insert), NULL);
    g_signal_connect(G_OBJECT(GTK_EDITABLE(passwd_entry)), "changed",
		     G_CALLBACK(passwd_entry_changed), NULL);
    g_signal_connect(G_OBJECT(GTK_EDITABLE(passwd2_entry)), "changed",
		     G_CALLBACK(passwd2_entry_changed), NULL);
    g_signal_connect(G_OBJECT(GTK_EDITABLE(cname_entry)), "changed",
		     G_CALLBACK(cname_entry_changed), NULL);
    g_signal_connect(G_OBJECT(GTK_EDITABLE(cname_entry)), "insert_text",
		     G_CALLBACK(names_entry_insert), NULL);
    g_signal_connect(G_OBJECT(ok_button), "clicked",
		     G_CALLBACK(ok_button_cb), NULL);

    gtk_window_fullscreen(GTK_WINDOW(win));
    gtk_widget_show_all(win);
    gtk_main();

    return 0;
}
