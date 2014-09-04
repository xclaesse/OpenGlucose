#include "config.h"

#include "device-widget.h"

#include <webkit2/webkit2.h>
#include <glib/gi18n.h>

#define DEBUG g_debug

/* FIXME: This should be user-defined, or even stored on device */
/* FIXME: It is in mg/dl unit */
#define HYPOGLYCEMIA 60
#define HYPERGLYCEMIA 170

G_DEFINE_TYPE (OgDeviceWidget, og_device_widget, GTK_TYPE_BIN)

struct _OgDeviceWidgetPrivate
{
  OgBaseDevice *device;

  GtkWidget *main_vbox;
  GtkWidget *info_bar;
  GtkWidget *info_bar_label;
  GtkWidget *spinner;

  WebKitWebView *modal_day_view;
  WebKitWebView *average_view;
  guint n_loading_views;

  gint64 time_span;
};

enum
{
  PROP_0,
  PROP_DEVICE,
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
  WebKitWebView *view = (WebKitWebView *) source;
  WebKitJavascriptResult *js_result;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_finish (view, result, &error);
  if (js_result == NULL)
    {
      g_warning ("Error running javascript: %s", error->message);
      g_clear_error (&error);
      return;
    }

  webkit_javascript_result_unref (js_result);
}

static void
run_javascript_literal (OgDeviceWidget *self,
    WebKitWebView *view,
    const gchar *script)
{
  g_assert (!webkit_web_view_is_loading (view));
  DEBUG ("Run script on view %p:\n%s", view, script);

  webkit_web_view_run_javascript (view, script, NULL,
      run_javascript_cb, self);
}

static void run_javascript (OgDeviceWidget *self,
    WebKitWebView *view,
    const gchar *format,
    ...) G_GNUC_PRINTF (3, 4);

static void
run_javascript (OgDeviceWidget *self,
    WebKitWebView *view,
    const gchar *format,
    ...)
{
  gchar *script;
  va_list args;

  va_start (args, format);
  script = g_strdup_vprintf (format, args);
  va_end (args);

  run_javascript_literal (self, view, script);

  g_free (script);
}

static gboolean
in_range (OgDeviceWidget *self,
    GDateTime *now,
    GDateTime *dt)
{
  if (self->priv->time_span < 0)
    return TRUE;

  return (g_date_time_difference (now, dt) <= self->priv->time_span);
}

static gchar *
dup_modal_day_data (OgDeviceWidget *self)
{
  GDateTime *now;
  GString *string;
  const OgRecord * const *records;
  struct { guint sum; guint n_values; } averages[12] = {};
  guint i;

  now = g_date_time_new_now_local ();
  records = og_base_device_get_records (self->priv->device);

  string = g_string_new ("[[");
  for (i = 0; records[i] != NULL; i++)
    {
      guint p;

      if (!in_range (self, now, records[i]->datetime))
        continue;

      p = g_date_time_get_hour (records[i]->datetime) / 2;
      averages[p].sum += records[i]->glycemia;
      averages[p].n_values++;

      g_string_append_printf (string, "[new Date(0,0,0,%u,%u,0,0),%u],",
          g_date_time_get_hour (records[i]->datetime),
          g_date_time_get_minute (records[i]->datetime),
          records[i]->glycemia);
    }
  g_string_append (string, "],[");
  for (i = 0; i < 12; i++)
    {
      if (averages[i].n_values == 0)
        continue;

      g_string_append_printf (string, "[new Date(0,0,0,%u,0,0,0),%u],",
          i * 2 + 1, averages[i].sum / averages[i].n_values);
    }
  g_string_append (string, "]]");

  g_date_time_unref (now);

  return g_string_free (string, FALSE);
}

static void
modal_day_chart_run_js_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgDeviceWidget *self = user_data;
  WebKitWebView *view = (WebKitWebView *) source;
  WebKitJavascriptResult *js_result;
  gchar *data;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_from_gresource_finish (view,
      result, &error);
  if (js_result == NULL)
    {
      g_warning ("Error running javascript: %s", error->message);
      g_clear_error (&error);
      return;
    }

  data = dup_modal_day_data (self);
  run_javascript (self, self->priv->modal_day_view,
      "OgChartPlot('%s',%u,%u,%s);",
      _("Modal Day Report"),
      HYPOGLYCEMIA, HYPERGLYCEMIA, data);

  g_free (data);
  webkit_javascript_result_unref (js_result);
}

static gchar *
dup_average_data (OgDeviceWidget *self)
{
  GDateTime *now;
  const OgRecord * const *records;
  guint n_hypo = 0;
  guint n_good = 0;
  guint n_hyper = 0;
  gchar *ret;
  guint i;

  now = g_date_time_new_now_local ();
  records = og_base_device_get_records (self->priv->device);
  for (i = 0; records[i] != NULL; i++)
    {
      if (!in_range (self, now, records[i]->datetime))
        continue;

      if (records[i]->glycemia < HYPOGLYCEMIA)
        n_hypo++;
      else if (records[i]->glycemia < HYPERGLYCEMIA)
        n_good++;
      else
        n_hyper++;
    }

  ret = g_strdup_printf ("[['%s',%u],['%s',%u],['%s',%u]]",
      _("Hypoglycemia"), n_hypo,
      _("Good"), n_good,
      _("Hyperglycemia"), n_hyper);

  g_date_time_unref (now);

  return ret;
}

static void
average_chart_run_js_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgDeviceWidget *self = user_data;
  WebKitWebView *view = (WebKitWebView *) source;
  WebKitJavascriptResult *js_result;
  gchar *data;
  GError *error = NULL;

  js_result = webkit_web_view_run_javascript_from_gresource_finish (view,
      result, &error);
  if (js_result == NULL)
    {
      g_warning ("Error running javascript: %s", error->message);
      g_clear_error (&error);
      return;
    }

  data = dup_average_data (self);
  run_javascript (self, self->priv->average_view,
      "OgChartPlot('%s',%s);",
      _("Average"), data);

  g_free (data);
  webkit_javascript_result_unref (js_result);
}

static void
view_is_loading_notify_cb (WebKitWebView *view,
    GParamSpec *param_spec,
    OgDeviceWidget *self)
{
  if (webkit_web_view_is_loading (view))
    return;

  g_signal_handlers_disconnect_by_func (view, view_is_loading_notify_cb, self);

  /* For some reason we have to wait for all views within the same process to be
   * loaded before running scripts, otherwise there are race conditions. Could
   * be a webkit bug? */
  self->priv->n_loading_views--;
  if (self->priv->n_loading_views > 0)
    return;

  webkit_web_view_run_javascript_from_gresource (self->priv->modal_day_view,
      "/org/freedesktop/OpenGlucose/src/modal-day-chart.js",
      NULL, modal_day_chart_run_js_cb, self);
  webkit_web_view_run_javascript_from_gresource (self->priv->average_view,
      "/org/freedesktop/OpenGlucose/src/average-chart.js",
      NULL, average_chart_run_js_cb, self);
}

static GtkWidget *
chart_view_new (OgDeviceWidget *self)
{
  WebKitWebView *view;
  gchar *basedir;
  GBytes *bytes;
  GError *error = NULL;

  view = (WebKitWebView *) webkit_web_view_new ();

  g_object_set (webkit_web_view_get_settings (view),
      "enable-developer-extras", TRUE, NULL);

  basedir = dup_basedir ();
  bytes = g_resources_lookup_data (
      "/org/freedesktop/OpenGlucose/src/chart.html",
      G_RESOURCE_LOOKUP_FLAGS_NONE,
      &error);
  g_assert_no_error (error);
  g_assert (bytes != NULL);

  webkit_web_view_load_html (view,
      g_bytes_get_data (bytes, NULL),
      basedir);

  self->priv->n_loading_views++;
  g_signal_connect (view, "notify::is-loading",
      G_CALLBACK (view_is_loading_notify_cb), self);

  g_free (basedir);
  g_bytes_unref (bytes);

  return (GtkWidget *) view;
}

static void
update_status (OgDeviceWidget *self)
{
  switch (og_base_device_get_status (self->priv->device))
    {
      case OG_BASE_DEVICE_STATUS_NONE:
        g_assert_not_reached ();
        break;
      case OG_BASE_DEVICE_STATUS_READY:
        gtk_widget_hide (self->priv->info_bar);
        break;
      case OG_BASE_DEVICE_STATUS_BUZY:
        gtk_info_bar_set_message_type (GTK_INFO_BAR (self->priv->info_bar),
            GTK_MESSAGE_INFO);
        gtk_label_set_text (GTK_LABEL (self->priv->info_bar_label),
            _("Fetching device information…"));
        gtk_widget_show (self->priv->info_bar);
        break;
      case OG_BASE_DEVICE_STATUS_ERROR:
        /* FIXME: The device should provide an error message */
        gtk_info_bar_set_message_type (GTK_INFO_BAR (self->priv->info_bar),
            GTK_MESSAGE_ERROR);
        gtk_label_set_text (GTK_LABEL (self->priv->info_bar_label),
            _("An error occured"));
        gtk_widget_show (self->priv->info_bar);
        break;
    }
}

static void
add_info_widget (OgDeviceWidget *self,
    GtkGrid *grid,
    GtkWidget *widget)
{
  gtk_grid_attach_next_to (grid, widget, NULL, GTK_POS_BOTTOM, 2, 1);
  gtk_widget_show (widget);
}

static void
add_info_widget_with_title (OgDeviceWidget *self,
    GtkGrid *grid,
    const gchar *title,
    GtkWidget *value_widget)
{
  GtkWidget *title_widget;
  GtkStyleContext *style;

  /* Title label */
  title_widget = gtk_label_new (title);
  style = gtk_widget_get_style_context (title_widget);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_set_halign (title_widget, GTK_ALIGN_END);
  gtk_grid_attach_next_to (grid, title_widget, NULL,
      GTK_POS_BOTTOM, 1, 1);
  gtk_widget_show (title_widget);

  /* Value widget */
  gtk_widget_set_halign (value_widget, GTK_ALIGN_START);
  gtk_grid_attach_next_to (grid, value_widget, title_widget,
      GTK_POS_RIGHT, 1, 1);
  gtk_widget_show (value_widget);
}

static void
add_info (OgDeviceWidget *self,
    GtkGrid *grid,
    const gchar *title,
    const gchar *value)
{
  add_info_widget_with_title (self, grid, title,
      gtk_label_new (value));
}

static void
add_info_entry (OgDeviceWidget *self,
    GtkGrid *grid,
    const gchar *title,
    const gchar *value)
{
  GtkWidget *entry;

  entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), title);
  gtk_entry_set_text (GTK_ENTRY (entry), value);

  add_info_widget_with_title (self, grid, title, entry);
}

static void
time_span_button_clicked_cb (GtkWidget *button,
    OgDeviceWidget *self)
{
  gint64 *span;
  gchar *data;

  span = g_object_get_data (G_OBJECT (button), "og-time-span");
  self->priv->time_span = *span;

  data = dup_modal_day_data (self);
  run_javascript (self, self->priv->modal_day_view,
      "OgChartRePlot(%s);", data);
  g_free (data);

  data = dup_average_data (self);
  run_javascript (self, self->priv->average_view,
      "OgChartRePlot(%s);", data);
  g_free (data);
}

static void
add_time_span_button (OgDeviceWidget *self,
    GtkBox *box,
    const gchar *text,
    gint64 span)
{
  GtkWidget *button;
  gint64 *span_ptr;

  button = gtk_button_new_with_label (text);
  gtk_box_pack_start (box, button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* Cannot stuff the gint64 into a gpointer on 32bits systems */
  span_ptr = g_new (gint64, 1);
  *span_ptr = span;
  g_object_set_data_full (G_OBJECT (button), "og-time-span", span_ptr, g_free);

  g_signal_connect (button, "clicked",
      G_CALLBACK (time_span_button_clicked_cb),
      self);
}

static void
sync_clock_clicked_cb (GtkWidget *button,
    OgDeviceWidget *self)
{
  og_base_device_sync_clock_async (self->priv->device, NULL, NULL, NULL);
}

static void
prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgDeviceWidget *self = user_data;
  OgBaseDevice *device = (OgBaseDevice *) source;
  GDateTime *device_clock;
  GDateTime *system_clock;
  gchar *device_clock_str;
  gchar *system_clock_str;
  GtkWidget *w;
  GtkGrid *info_grid;
  GtkBox *top_box;
  GError *error = NULL;

  if (!og_base_device_prepare_finish (device, result, &error))
    {
      g_warning ("Error preparing device: %s", error->message);
      g_clear_error (&error);
      return;
    }

  gtk_widget_destroy (self->priv->spinner);
  self->priv->spinner = NULL;

  /* top hbox */
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), w, FALSE, FALSE, 0);
  gtk_widget_show (w);
  top_box = (GtkBox *) w;

  /* top-left info grid */
  w = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (w), 6);
  gtk_grid_set_row_spacing (GTK_GRID (w), 6);
  gtk_box_pack_start (top_box, w, FALSE, FALSE, 0);
  gtk_widget_show (w);
  info_grid = (GtkGrid *) w;

  /* Info: name */
  add_info_entry (self, info_grid, _("First name"),
      og_base_device_get_first_name (self->priv->device));
  add_info_entry (self, info_grid, _("Last name"),
      og_base_device_get_last_name (self->priv->device));

  /* Info: time span selector */
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  add_time_span_button (self, GTK_BOX (w), _("1W"),   7*G_TIME_SPAN_DAY);
  add_time_span_button (self, GTK_BOX (w), _("2W"),  14*G_TIME_SPAN_DAY);
  add_time_span_button (self, GTK_BOX (w), _("1M"),  30*G_TIME_SPAN_DAY);
  add_time_span_button (self, GTK_BOX (w), _("2M"),  60*G_TIME_SPAN_DAY);
  add_time_span_button (self, GTK_BOX (w), _("1Y"), 365*G_TIME_SPAN_DAY);
  add_time_span_button (self, GTK_BOX (w), _("All"), -1);
  add_info_widget_with_title (self, info_grid, _("Time span"), w);

  /* Info: serial number */
  add_info (self, info_grid, _("Serial number"),
      og_base_device_get_serial_number (self->priv->device));

  /* Info: clock */
  device_clock = og_base_device_get_clock (self->priv->device, &system_clock);
  device_clock_str = g_date_time_format (device_clock, "%x %X");
  system_clock_str = g_date_time_format (system_clock, "%x %X");
  add_info (self, info_grid, _("Device clock"), device_clock_str);
  add_info (self, info_grid, _("System clock"), system_clock_str);
  w = gtk_button_new_with_label (_("Sync clock"));
  g_signal_connect (w, "clicked",
      G_CALLBACK (sync_clock_clicked_cb), self);
  add_info_widget (self, info_grid, w);

  /* top-right pie hyper/good/hypo percentages chart */
  w = chart_view_new (self);
  gtk_widget_set_size_request (w, -1, 350);
  gtk_box_pack_start (top_box, w, TRUE, TRUE, 0);
  gtk_widget_set_hexpand (w, TRUE),
  gtk_widget_show (w);
  self->priv->average_view = (WebKitWebView *) w;

  /* bottom modal day chart */
  w = chart_view_new (self);
  gtk_widget_set_size_request (w, -1, 350);
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), w, FALSE, FALSE, 0);
  gtk_widget_show (w);
  self->priv->modal_day_view = (WebKitWebView *) w;

  g_free (device_clock_str);
  g_free (system_clock_str);
}

static void
og_device_widget_init (OgDeviceWidget *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_DEVICE_WIDGET, OgDeviceWidgetPrivate);

  self->priv->time_span = -1;
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  OgDeviceWidget *self = (OgDeviceWidget *) object;

  switch (property_id)
    {
      case PROP_DEVICE:
        g_value_set_object (value, self->priv->device);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  OgDeviceWidget *self = (OgDeviceWidget *) object;

  switch (property_id)
    {
      case PROP_DEVICE:
        g_assert (self->priv->device == NULL);
        self->priv->device = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
constructed (GObject *object)
{
  OgDeviceWidget *self = (OgDeviceWidget *) object;
  GtkWidget *content_area;

  G_OBJECT_CLASS (og_device_widget_parent_class)->constructed (object);

  g_assert (self->priv->device != NULL);

  og_base_device_prepare_async (self->priv->device, NULL,
      prepare_cb, self);

  self->priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (self), self->priv->main_vbox);
  gtk_widget_show (self->priv->main_vbox);

  self->priv->info_bar = gtk_info_bar_new ();
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), self->priv->info_bar,
      FALSE, FALSE, 0);

  self->priv->info_bar_label = gtk_label_new ("");
  content_area = gtk_info_bar_get_content_area (
      GTK_INFO_BAR (self->priv->info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), self->priv->info_bar_label);
  gtk_widget_show (self->priv->info_bar_label);

  g_signal_connect_object (self->priv->device, "notify::status",
      G_CALLBACK (update_status), self, G_CONNECT_SWAPPED);
  update_status (self);

  self->priv->spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
  gtk_widget_set_size_request (self->priv->spinner, 200, 200);
  gtk_widget_set_valign (self->priv->spinner, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (self->priv->main_vbox), self->priv->spinner,
      TRUE, TRUE, 0);
  gtk_widget_show (self->priv->spinner);
}

static void
dispose (GObject *object)
{
  OgDeviceWidget *self = (OgDeviceWidget *) object;

  g_clear_object (&self->priv->device);

  G_OBJECT_CLASS (og_device_widget_parent_class)->dispose (object);
}

static void
og_device_widget_class_init (OgDeviceWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;

  g_type_class_add_private (object_class, sizeof (OgDeviceWidgetPrivate));

  param_spec = g_param_spec_object ("device",
      "Device",
      "The OgBaseDevice this widget is displaying",
      OG_TYPE_BASE_DEVICE,
      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_DEVICE, param_spec);
}

GtkWidget *
og_device_widget_new (OgBaseDevice *device)
{
  return g_object_new (OG_TYPE_DEVICE_WIDGET,
      "device", device,
      NULL);
}
