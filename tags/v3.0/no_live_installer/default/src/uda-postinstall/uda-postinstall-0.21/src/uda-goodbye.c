#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glade/glade.h>

GladeXML *app;
GtkWidget *win;
GtkWidget *im;
GdkScreen *screen;

gchar *get_branding_path(void)
{
    return g_strdup_printf(BRANDINGDIR "/%s/%s/",
			   g_getenv("UDA_DISTRO_NAME"),
			   g_getenv("UDA_DISTRO_LANG"));
}

int main(int argc, char *argv[])
{

    gchar *pix_file;
    gtk_init(&argc, &argv);

    app = glade_xml_new(UIDIR "uda-goodbye.glade", NULL, NULL);
    win = glade_xml_get_widget(app, "win");
    im = glade_xml_get_widget(app, "image");
    gtk_widget_hide(win);

    screen = gdk_screen_get_default();
    if (gdk_screen_get_width(screen) >= 1024) {
	pix_file =
	    g_strdup_printf("%s%s", get_branding_path(),
			    "endinst_big.jpg");
    } else {
	pix_file =
	    g_strdup_printf("%s%s", get_branding_path(), "endinst.jpg");
    }
    gtk_image_set_from_file(GTK_IMAGE(im), pix_file);
    gtk_widget_show_all(win);
    gtk_window_fullscreen(GTK_WINDOW(win));
    g_timeout_add (10000, (GSourceFunc) gtk_main_quit, NULL);
    gtk_main();
    g_free(pix_file);
    return 0;
}
