#include "config.h"

#include <stdlib.h>
#include <gusb.h>

#include "base-device.h"
#include "insulinx.h"

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
  GApplication parent;

  GHashTable *devices_table;
  GUsbContext *context;
  GUsbDeviceList *list;
} OgApplication;

typedef struct
{
  GApplicationClass parent;
} OgApplicationClass;

#define OG_TYPE_APPLICATION (og_application_get_type ())
GType og_application_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (OgApplication, og_application, G_TYPE_APPLICATION)

static void
fetch_device_info_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgBaseDevice *base = (OgBaseDevice *) source;
  OgDeviceInfo *info;
  gchar *str;
  guint i;
  GError *error = NULL;

  info = og_base_device_fetch_device_info_finish (base, result, &error);
  if (info == NULL)
    {
      g_warning ("Error getting device info: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_print ("Got device info for %s:\n", info->serial_number);

  str = g_date_time_format (info->datetime, "%x %X");
  g_print ("  Device time: %s\n", str);
  g_free (str);

  for (i = 0; i < info->records->len; i++)
    {
      OgRecord *record = g_ptr_array_index (info->records, i);

      str = g_date_time_format (record->datetime, "%x %X");
      g_print ("  %s, glycemia: %u\n", str, record->glycemia);
      g_free (str);
    }

  og_device_info_free (info);
}

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

          og_base_device_fetch_device_info_async (base, NULL,
              fetch_device_info_cb, self);
          break;
        }
    }
}

static void
remove_device (OgApplication *self,
    GUsbDevice *device)
{
  g_hash_table_remove (self->devices_table, device);
}

static void
startup (GApplication *app)
{
  OgApplication *self = (OgApplication *) app;
  GPtrArray *devices;
  guint i;
  GError *error = NULL;

  g_application_hold (app);

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

  G_APPLICATION_CLASS (og_application_parent_class)->startup (app);
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
