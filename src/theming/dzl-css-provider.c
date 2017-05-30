/* dzl-css-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "dzl-css-provider"

#include <glib/gi18n.h>

#include "theming/dzl-css-provider.h"

struct _DzlCssProvider
{
  GtkCssProvider  parent_instance;
  gchar          *base_path;
  guint           queued_update;
};

G_DEFINE_TYPE (DzlCssProvider, dzl_css_provider, GTK_TYPE_CSS_PROVIDER)

enum {
  PROP_0,
  PROP_BASE_PATH,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkCssProvider *
dzl_css_provider_new (const gchar *base_path)
{
  return g_object_new (DZL_TYPE_CSS_PROVIDER,
                       "base-path", base_path,
                       NULL);
}

static void
dzl_css_provider_update (DzlCssProvider *self)
{
  g_autofree gchar *theme_name = NULL;
  g_autofree gchar *resource_path = NULL;
  GtkSettings *settings = gtk_settings_get_default ();
  gboolean prefer_dark_theme = FALSE;
  gsize len = 0;
  guint32 flags = 0;

  g_assert (DZL_IS_CSS_PROVIDER (self));

  if ((theme_name = g_strdup(g_getenv ("GTK_THEME"))))
    {
      char *p;

      /* Theme variants are specified with the syntax
       * "<theme>:<variant>" e.g. "Adwaita:dark" */
      if ((p = strrchr (theme_name, ':')))
        {
          *p = '\0';
          p++;
          prefer_dark_theme = g_strcmp0 (p, "dark") == 0;
        }
    }
  else
    {
      g_object_get (settings,
                    "gtk-theme-name", &theme_name,
                    "gtk-application-prefer-dark-theme", &prefer_dark_theme,
                    NULL);
    }

  resource_path = g_strdup_printf ("%s/theme/%s%s.css",
                                   self->base_path,
                                   theme_name, prefer_dark_theme ? "-dark" : "");

  if (!g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &len, &flags, NULL))
    {
      g_free (resource_path);
      resource_path = g_strdup_printf ("%s/theme/shared.css", self->base_path);
    }

  /* Nothing to load */
  if (!g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &len, &flags, NULL))
    return;

  g_debug ("Loading css overrides \"%s\"", resource_path);

  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (self), resource_path);
}

static gboolean
dzl_css_provider_do_update (gpointer user_data)
{
  DzlCssProvider *self = user_data;

  g_assert (DZL_IS_CSS_PROVIDER (self));

  self->queued_update = 0;
  dzl_css_provider_update (self);

  return G_SOURCE_REMOVE;
}

static void
dzl_css_provider_queue_update (DzlCssProvider *self)
{
  g_assert (DZL_IS_CSS_PROVIDER (self));

  if (self->queued_update == 0)
    self->queued_update = g_timeout_add (0, dzl_css_provider_do_update, self);
}

static void
dzl_css_provider__settings_notify_gtk_theme_name (DzlCssProvider *self,
                                                 GParamSpec    *pspec,
                                                 GtkSettings   *settings)
{
  g_assert (DZL_IS_CSS_PROVIDER (self));

  dzl_css_provider_queue_update (self);
}

static void
dzl_css_provider__settings_notify_gtk_application_prefer_dark_theme (DzlCssProvider *self,
                                                                    GParamSpec    *pspec,
                                                                    GtkSettings   *settings)
{
  g_assert (DZL_IS_CSS_PROVIDER (self));

  dzl_css_provider_queue_update (self);
}

static void
dzl_css_provider_parsing_error (GtkCssProvider *provider,
                               GtkCssSection  *section,
                               const GError   *error)
{
  g_autofree gchar *uri = NULL;
  GFile *file;
  guint line = 0;
  guint line_offset = 0;

  g_assert (DZL_IS_CSS_PROVIDER (provider));
  g_assert (error != NULL);

  if (section != NULL)
    {
      file = gtk_css_section_get_file (section);
      uri = g_file_get_uri (file);
      line = gtk_css_section_get_start_line (section);
      line_offset = gtk_css_section_get_start_position (section);
      g_warning ("Parsing Error: %s @ %u:%u: %s", uri, line, line_offset, error->message);
    }
  else
    {
      g_warning ("%s", error->message);
    }
}

static void
dzl_css_provider_constructed (GObject *object)
{
  DzlCssProvider *self = (DzlCssProvider *)object;
  GtkSettings *settings;

  G_OBJECT_CLASS (dzl_css_provider_parent_class)->constructed (object);

  settings = gtk_settings_get_default ();

  g_signal_connect_object (settings,
                           "notify::gtk-theme-name",
                           G_CALLBACK (dzl_css_provider__settings_notify_gtk_theme_name),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (settings,
                           "notify::gtk-application-prefer-dark-theme",
                           G_CALLBACK (dzl_css_provider__settings_notify_gtk_application_prefer_dark_theme),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_css_provider_update (self);
}

static void
dzl_css_provider_finalize (GObject *object)
{
  DzlCssProvider *self = (DzlCssProvider *)object;

  g_clear_pointer (&self->base_path, g_free);

  G_OBJECT_CLASS (dzl_css_provider_parent_class)->finalize (object);
}

static void
dzl_css_provider_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  DzlCssProvider *self = DZL_CSS_PROVIDER(object);

  switch (prop_id)
    {
    case PROP_BASE_PATH:
      g_value_set_string (value, self->base_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
dzl_css_provider_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  DzlCssProvider *self = DZL_CSS_PROVIDER(object);

  switch (prop_id)
    {
    case PROP_BASE_PATH:
      self->base_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
dzl_css_provider_class_init (DzlCssProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssProviderClass *provider_class = GTK_CSS_PROVIDER_CLASS (klass);

  object_class->constructed = dzl_css_provider_constructed;
  object_class->finalize = dzl_css_provider_finalize;
  object_class->get_property = dzl_css_provider_get_property;
  object_class->set_property = dzl_css_provider_set_property;

  provider_class->parsing_error = dzl_css_provider_parsing_error;

  properties [PROP_BASE_PATH] =
    g_param_spec_string ("base-path",
                         "Base Path",
                         "The base resource path to discover themes",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
dzl_css_provider_init (DzlCssProvider *self)
{
}
