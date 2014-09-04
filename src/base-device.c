#include "config.h"

#include "base-device.h"

#include <string.h>

G_DEFINE_QUARK (og-base-device-error-quark, og_base_device_error)
G_DEFINE_ABSTRACT_TYPE (OgBaseDevice, og_base_device, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_STATUS,
};

static void
og_base_device_init (OgBaseDevice *self)
{
}

static void
constructed (GObject *object)
{
  OgBaseDevice *self = (OgBaseDevice *) object;

  G_OBJECT_CLASS (og_base_device_parent_class)->constructed (object);

  g_debug ("New device %p: %s", self, og_base_device_get_name (self));
}

static void
finalize (GObject *object)
{
  OgBaseDevice *self = (OgBaseDevice *) object;

  g_debug ("Finalize device %p", self);

  G_OBJECT_CLASS (og_base_device_parent_class)->finalize (object);
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  OgBaseDevice *self = (OgBaseDevice *) object;

  switch (property_id)
    {
      case PROP_STATUS:
        g_value_set_uint (value, og_base_device_get_status (self));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
og_base_device_class_init (OgBaseDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->constructed = constructed;
  object_class->finalize = finalize;
  object_class->get_property = get_property;

  param_spec = g_param_spec_uint ("status",
      "Status",
      "The current status of this device",
      OG_BASE_DEVICE_STATUS_NONE,
      OG_LAST_BASE_DEVICE_STATUS,
      OG_BASE_DEVICE_STATUS_NONE,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_STATUS, param_spec);
}

OgBaseDeviceStatus
og_base_device_get_status (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), OG_BASE_DEVICE_STATUS_ERROR);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_status != NULL, OG_BASE_DEVICE_STATUS_ERROR);

  return klass->get_status (self);
}

const gchar *
og_base_device_get_name (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_name != NULL, NULL);

  return klass->get_name (self);
}

void
og_base_device_prepare_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgBaseDeviceClass *klass;

  g_return_if_fail (OG_IS_BASE_DEVICE (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_if_fail (klass->prepare_async != NULL);

  klass->prepare_async (self, cancellable, callback, user_data);
}

gboolean
og_base_device_prepare_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->prepare_finish != NULL, FALSE);

  return klass->prepare_finish (self, result, error);
}

void
og_base_device_sync_clock_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgBaseDeviceClass *klass;

  g_return_if_fail (OG_IS_BASE_DEVICE (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_if_fail (klass->sync_clock_async != NULL);

  klass->sync_clock_async (self, cancellable, callback, user_data);
}

gboolean
og_base_device_sync_clock_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->sync_clock_finish != NULL, FALSE);

  return klass->sync_clock_finish (self, result, error);
}

const gchar *
og_base_device_get_serial_number (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_serial_number != NULL, NULL);

  return klass->get_serial_number (self);
}

GDateTime *
og_base_device_get_clock (OgBaseDevice *self,
    GDateTime **system_clock)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_clock != NULL, NULL);

  return klass->get_clock (self, system_clock);
}

const OgRecord * const *
og_base_device_get_records (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_records != NULL, NULL);

  return klass->get_records (self);
}

const gchar *
og_base_device_get_first_name (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_first_name != NULL, NULL);

  return klass->get_first_name (self);
}

const gchar *
og_base_device_get_last_name (OgBaseDevice *self)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->get_last_name != NULL, NULL);

  return klass->get_last_name (self);
}
