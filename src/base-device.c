#include "config.h"

#include "base-device.h"

#include <string.h>

G_DEFINE_ABSTRACT_TYPE (OgBaseDevice, og_base_device, G_TYPE_OBJECT)

struct _OgBaseDevicePrivate
{
  OgBaseDeviceStatus status;
};

enum
{
  PROP_0,
  PROP_STATUS,
};

static void
og_base_device_init (OgBaseDevice *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_BASE_DEVICE, OgBaseDevicePrivate);
}

static void
constructed (GObject *object)
{
  OgBaseDevice *self = (OgBaseDevice *) object;

  G_OBJECT_CLASS (og_base_device_parent_class)->constructed (object);

  g_debug ("New device: %s", og_base_device_get_name (self));
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
        g_value_set_uint (value, self->priv->status);
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
  object_class->get_property = get_property;

  g_type_class_add_private (object_class, sizeof (OgBaseDevicePrivate));

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
  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), OG_BASE_DEVICE_STATUS_NONE);

  return self->priv->status;
}

void
og_base_device_change_status (OgBaseDevice *self,
    OgBaseDeviceStatus status)
{
  g_return_if_fail (OG_IS_BASE_DEVICE (self));

  if (status == self->priv->status)
    return;

  self->priv->status = status;
  g_object_notify ((GObject *) self, "status");
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
og_base_device_refresh_device_info_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgBaseDeviceClass *klass;

  g_return_if_fail (OG_IS_BASE_DEVICE (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_if_fail (klass->refresh_device_info_async != NULL);

  klass->refresh_device_info_async (self, cancellable, callback, user_data);
}

gboolean
og_base_device_refresh_device_info_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->refresh_device_info_finish != NULL, FALSE);

  return klass->refresh_device_info_finish (self, result, error);
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
