#include "config.h"

#include "base-device.h"

#include <string.h>

G_DEFINE_ABSTRACT_TYPE (OgBaseDevice, og_base_device, G_TYPE_OBJECT)

struct _OgBaseDevicePrivate
{
  GUsbDevice *usb_device;
};

enum
{
  PROP_0,
  PROP_USB_DEVICE,
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
  GError *error = NULL;

  G_OBJECT_CLASS (og_base_device_parent_class)->constructed (object);

  g_assert (self->priv->usb_device != NULL);
  if (!g_usb_device_open (self->priv->usb_device, &error))
    g_warning ("Error opening device: %s", error->message);
  g_clear_error (&error);
}

static void
dispose (GObject *object)
{
  OgBaseDevice *self = (OgBaseDevice *) object;
  GError *error = NULL;

  if (self->priv->usb_device != NULL)
    {
      if (!g_usb_device_close (self->priv->usb_device, &error))
        g_warning ("Error closing device: %s", error->message);
      g_clear_error (&error);
    }
  g_clear_object (&self->priv->usb_device);

  G_OBJECT_CLASS (og_base_device_parent_class)->dispose (object);
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
      case PROP_USB_DEVICE:
        g_value_set_object (value, self->priv->usb_device);
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
  OgBaseDevice *self = (OgBaseDevice *) object;

  switch (property_id)
    {
      case PROP_USB_DEVICE:
        g_assert (self->priv->usb_device == NULL);
        self->priv->usb_device = g_value_dup_object (value);
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
  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_type_class_add_private (object_class, sizeof (OgBaseDevicePrivate));

  param_spec = g_param_spec_object ("usb-device",
      "usb-device",
      "The GUSbDevice.",
      G_USB_TYPE_DEVICE,
      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_USB_DEVICE, param_spec);
}

GUsbDevice *
og_base_device_get_usb_device (OgBaseDevice *self)
{
  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);

  return self->priv->usb_device;
}

void
og_base_device_fetch_device_info_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgBaseDeviceClass *klass;

  g_return_if_fail (OG_IS_BASE_DEVICE (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_if_fail (klass->fetch_device_info_async != NULL);

  klass->fetch_device_info_async (self, cancellable, callback, user_data);
}

OgDeviceInfo *
og_base_device_fetch_device_info_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error)
{
  OgBaseDeviceClass *klass;

  g_return_val_if_fail (OG_IS_BASE_DEVICE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  klass = OG_BASE_DEVICE_GET_CLASS (self);
  g_return_val_if_fail (klass->fetch_device_info_finish != NULL, NULL);

  return klass->fetch_device_info_finish (self, result, error);
}
