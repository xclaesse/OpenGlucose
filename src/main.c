#include "config.h"

#include <stdlib.h>
#include <gusb.h>

#include "base-device.h"
#include "dummy-device.h"
#include "insulinx.h"
#include "main-window.h"

typedef struct
{
  guint16 vendor_id;
  guint16 product_id;
  OgBaseDevice *(*new) (GUsbDevice *device);
} SupportedDevice;

static SupportedDevice supported_devices[] = {
  /* Abbott FreeStyle InsuLinx */
  { 0x1a61, 0x3460, og_insulinx_new },
};

typedef struct
{
  GtkApplication parent;

  GtkWidget *window;
  /* Owned GUsbDevice -> owned OgBaseDevice */
  GHashTable *devices_table;
  GUsbContext *context;
  GUsbDeviceList *list;
} OgApplication;

typedef struct
{
  GtkApplicationClass parent;
} OgApplicationClass;

#define OG_TYPE_APPLICATION (og_application_get_type ())
GType og_application_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (OgApplication, og_application, GTK_TYPE_APPLICATION)

static void
add_device (OgApplication *self,
    GUsbDevice *device)
{
  guint i;

  if (g_hash_table_contains (self->devices_table, device))
    return;

  for (i = 0; i < G_N_ELEMENTS (supported_devices); i++)
    {
      if (g_usb_device_get_vid (device) == supported_devices[i].vendor_id &&
          g_usb_device_get_pid (device) == supported_devices[i].product_id)
        {
          OgBaseDevice *base;

          base = supported_devices[i].new (device);
          g_hash_table_insert (self->devices_table,
              g_object_ref (device),
              base);
          og_main_window_add_device ((OgMainWindow *) self->window, base);
          break;
        }
    }
}

static void
remove_device (OgApplication *self,
    GUsbDevice *device)
{
  OgBaseDevice *base;

  base = g_hash_table_lookup (self->devices_table, device);
  if (base != NULL)
    {
      og_main_window_remove_device ((OgMainWindow *) self->window, base);
      g_hash_table_remove (self->devices_table, device);
    }
}

static void
startup (GApplication *app)
{
  OgApplication *self = (OgApplication *) app;
  GtkCssProvider *provider;
  GFile *file;
  GPtrArray *devices;
  guint i;
  GError *error = NULL;

  G_APPLICATION_CLASS (og_application_parent_class)->startup (app);

  /* There is no gtk_css_provider_load_from_resource() yet.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=711293 */
  provider = gtk_css_provider_new ();
  file = g_file_new_for_uri (
      "resource:///org/freedesktop/OpenGlucose/src/openglucose.css");
  gtk_css_provider_load_from_file (provider, file, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
      GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  self->window = og_main_window_new ((GtkApplication *) self);
  gtk_widget_show (self->window);

  self->devices_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      g_object_unref, g_object_unref);

  self->context = g_usb_context_new (&error);
  if (self->context == NULL)
    g_error ("Error creating USB context: %s", error->message);

  self->list = g_usb_device_list_new (self->context);
  g_usb_device_list_coldplug (self->list);

  devices = g_usb_device_list_get_devices (self->list);
  for (i = 0; i < devices->len; i++)
    add_device (self, g_ptr_array_index (devices, i));
  g_ptr_array_unref (devices);

  g_signal_connect_swapped (self->list, "device-added",
      G_CALLBACK (add_device), self);
  g_signal_connect_swapped (self->list, "device-removed",
      G_CALLBACK (remove_device), self);

  if (g_getenv ("OPENGLUCOSE_DUMMY_DEVICE") != NULL)
    {
      OgBaseDevice *base;

      base = og_dummy_device_new ();
      og_main_window_add_device ((OgMainWindow *) self->window, base);
      g_object_unref (base);
    }

  g_object_unref (provider);
  g_object_unref (file);
}

static void
shutdown (GApplication *app)
{
  OgApplication *self = (OgApplication *) app;

  g_hash_table_unref (self->devices_table);
  g_object_unref (self->list);
  g_object_unref (self->context);

  G_APPLICATION_CLASS (og_application_parent_class)->shutdown (app);
}

static void
activate (GApplication *app)
{
  OgApplication *self = (OgApplication *) app;

  gtk_window_present (GTK_WINDOW (self->window));

  G_APPLICATION_CLASS (og_application_parent_class)->activate (app);
}

static void
og_application_init (OgApplication *self)
{
}

static void
og_application_class_init (OgApplicationClass *klass)
{
  GApplicationClass *app_class = (GApplicationClass *) klass;

  app_class->startup = startup;
  app_class->shutdown = shutdown;
  app_class->activate = activate;
}

int
main (int argc, char *argv[])
{
  GApplication *app;

  app = g_object_new (OG_TYPE_APPLICATION,
      "application-id", "org.freedesktop.OpenGlucose",
      "flags", G_APPLICATION_FLAGS_NONE,
      NULL);

  g_application_run (app, argc, argv);

  g_object_unref (app);

  return EXIT_SUCCESS;
}
