#include <dazzle.h>

gint
main (gint argc,
      gchar *argv[])
{
  GtkWidget *window;
  GtkWidget *button;

  gtk_init (&argc, &argv);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "border-width", 24,
                         "title", "Progress Button Test",
                         "visible", TRUE,
                         NULL);

  button = g_object_new (DZL_TYPE_PROGRESS_BUTTON,
                         "label", "Downloading…",
                         "progress", 0,
                         "show-progress", TRUE,
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (window), button);

  dzl_object_animate (button, DZL_ANIMATION_LINEAR, 5000, NULL,
                      "progress", 100,
                      NULL);

  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  gtk_window_present (GTK_WINDOW (window));
  gtk_main ();

  return 0;
}
