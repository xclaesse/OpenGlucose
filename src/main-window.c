#include "config.h"

#include "main-window.h"

#include <webkit2/webkit2.h>

G_DEFINE_TYPE (OgMainWindow, og_main_window, GTK_TYPE_APPLICATION_WINDOW)

struct _OgMainWindowPrivate
{
  /* Owned OgBaseDevice */
  GList *devices;
  WebKitWebView *view;
  gboolean loaded;
};

static gchar *
dup_basedir (void)
{
  const gchar *basedir;

  basedir = g_getenv ("OPENGLUCOSE_SRCDIR");
  if (basedir == NULL)
    basedir = DATA_DIR;

  return g_strdup_printf ("file://%s/", basedir);
}

static void
run_javascript_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgMainWindow *self = user_data;
  WebKitJavascriptResult *js_result;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_finish (self->priv->view, result,
      &error);
  if (js_result == NULL)
    {
      g_warning ("Error running javascript: %s", error->message);
      g_clear_error (&error);
      return;
    }

  webkit_javascript_result_unref (js_result);
}

static void
run_javascript_literal (OgMainWindow *self,
    const gchar *script)
{
  g_debug ("Run script:\n%s", script);

  webkit_web_view_run_javascript (self->priv->view, script, NULL,
      run_javascript_cb, self);
}

static void run_javascript (OgMainWindow *self,
    const gchar *format,
    ...) G_GNUC_PRINTF (2, 3);

static void
run_javascript (OgMainWindow *self,
    const gchar *format,
    ...)
{
  gchar *script;
  va_list args;

  g_assert (self->priv->loaded);

  va_start (args, format);
  script = g_strdup_vprintf (format, args);
  va_end (args);

  run_javascript_literal (self, script);

  g_free (script);
}

static void
update_device_js (OgMainWindow *self,
    OgBaseDevice *device)
{
  GString *string;
  const OgRecord * const *records;
  guint i;

  string = g_string_new ("og.updateDevice({");

  /* Fill always available info */
  g_string_append_printf (string,
      "id:'%p',"
      "name:'%s',",
      device,
      og_base_device_get_name (device));

  switch (og_base_device_get_status (device))
    {
      case OG_BASE_DEVICE_STATUS_NONE:
        g_assert_not_reached ();
      case OG_BASE_DEVICE_STATUS_REFRESHING:
        g_string_append (string, "refreshing:true,");
        goto out;
      case OG_BASE_DEVICE_STATUS_FAILED:
        g_string_append (string, "failed:true,");
        goto out;
      case OG_BASE_DEVICE_STATUS_READY:
        /* continue */
        break;
    }

  g_string_append_printf (string, "sn:'%s',",
    og_base_device_get_serial_number (device));

  g_string_append (string, "data:[");
  records = og_base_device_get_records (device);
  for (i = 0; records[i] != NULL; i++)
    {
      if (i > 0)
        g_string_append_c (string, ',');

      g_string_append_printf (string, "[new Date(%"G_GINT64_FORMAT"),%u]",
          g_date_time_to_unix (records[i]->datetime) * 1000,
          records[i]->glycemia);
    }
  g_string_append (string, "],");

out:
  /* Run the script */
  g_string_append (string, "});");
  run_javascript_literal (self, string->str);
  g_string_free (string, TRUE);
}

static void
remove_device_js (OgMainWindow *self,
    OgBaseDevice *device)
{
  run_javascript (self, "og.removeDevice('%p');",
      device);
}

static void
load_changed_cb (WebKitWebView *view,
    WebKitLoadEvent load_event,
    OgMainWindow *self)
{
  GList *l;

  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  self->priv->loaded = TRUE;
  for (l = self->priv->devices; l != NULL; l = l->next)
    update_device_js (self, l->data);
}

static void
og_main_window_init (OgMainWindow *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_MAIN_WINDOW, OgMainWindowPrivate);
}

static void
constructed (GObject *object)
{
  OgMainWindow *self = (OgMainWindow *) object;
  GtkWidget *w;
  GBytes *bytes;
  gchar *basedir;
  GError *error = NULL;

  G_OBJECT_CLASS (og_main_window_parent_class)->constructed (object);

  /* FIXME: We should remember last time's window geometry */
  gtk_window_maximize (GTK_WINDOW (self));

  w = webkit_web_view_new ();
  gtk_container_add (GTK_CONTAINER (self), w);
  gtk_widget_show (w);
  self->priv->view = (WebKitWebView *) w;

  g_object_set (webkit_web_view_get_settings (self->priv->view),
      "enable-developer-extras", TRUE, NULL);

  basedir = dup_basedir ();
  bytes = g_resources_lookup_data (
      "/org/freedesktop/OpenGlucose/src/main-window.html",
      G_RESOURCE_LOOKUP_FLAGS_NONE,
      &error);
  if (bytes == NULL)
    {
      g_error ("Failed to load resource: %s", error->message);
      g_clear_error (&error);
      return;
    }

  webkit_web_view_load_html (self->priv->view,
      g_bytes_get_data (bytes, NULL),
      basedir);
  g_signal_connect (self->priv->view, "load-changed",
      G_CALLBACK (load_changed_cb), self);

  g_bytes_unref (bytes);
  g_free (basedir);
}

static void
dispose (GObject *object)
{
  OgMainWindow *self = (OgMainWindow *) object;

  g_list_free_full (self->priv->devices, g_object_unref);
  self->priv->devices = NULL;

  G_OBJECT_CLASS (og_main_window_parent_class)->dispose (object);
}

static void
og_main_window_class_init (OgMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

  g_type_class_add_private (object_class, sizeof (OgMainWindowPrivate));
}

GtkWidget *
og_main_window_new (GtkApplication *application)
{
  return g_object_new (OG_TYPE_MAIN_WINDOW,
      "application", application,
      NULL);
}

static void
refresh_device_info_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgMainWindow *self = user_data;
  OgBaseDevice *device = (OgBaseDevice *) source;
  GError *error = NULL;

  if (!og_base_device_refresh_device_info_finish (device, result, &error))
    {
      g_warning ("Error refreshing device: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (self->priv->loaded)
    update_device_js (self, device);
}

void
og_main_window_add_device (OgMainWindow *self,
    OgBaseDevice *device)
{
  g_return_if_fail (OG_IS_MAIN_WINDOW (self));
  g_return_if_fail (OG_IS_BASE_DEVICE (device));
  g_return_if_fail (g_list_find (self->priv->devices, device) == NULL);

  self->priv->devices = g_list_prepend (self->priv->devices,
      g_object_ref (device));

  og_base_device_refresh_device_info_async (device, NULL,
      refresh_device_info_cb, self);

  if (self->priv->loaded)
    update_device_js (self, device);
}

void
og_main_window_remove_device (OgMainWindow *self,
    OgBaseDevice *device)
{
  GList *l;

  g_return_if_fail (OG_IS_MAIN_WINDOW (self));
  g_return_if_fail (OG_IS_BASE_DEVICE (device));

  l = g_list_find (self->priv->devices, device);
  g_return_if_fail (l != NULL);

  self->priv->devices = g_list_delete_link (self->priv->devices, l);

  if (self->priv->loaded)
    remove_device_js (self, device);

  g_object_unref (device);
}
