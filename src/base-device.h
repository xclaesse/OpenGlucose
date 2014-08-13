#ifndef __OG_BASE_DEVICE_H__
#define __OG_BASE_DEVICE_H__

#include <gusb.h>

#include "device-info.h"

G_BEGIN_DECLS

#define OG_TYPE_BASE_DEVICE \
    (og_base_device_get_type ())
#define OG_BASE_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OG_TYPE_BASE_DEVICE, \
        OgBaseDevice))
#define OG_BASE_DEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OG_TYPE_BASE_DEVICE, \
        OgBaseDeviceClass))
#define OG_IS_BASE_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OG_TYPE_BASE_DEVICE))
#define OG_IS_BASE_DEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OG_TYPE_BASE_DEVICE))
#define OG_BASE_DEVICE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OG_TYPE_BASE_DEVICE, \
        OgBaseDeviceClass))

typedef struct _OgBaseDevice OgBaseDevice;
typedef struct _OgBaseDeviceClass OgBaseDeviceClass;
typedef struct _OgBaseDevicePrivate OgBaseDevicePrivate;

struct _OgBaseDevice {
  GObject parent;

  OgBaseDevicePrivate *priv;
};

struct _OgBaseDeviceClass {
  GObjectClass parent_class;

  void (*fetch_device_info_async) (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
  OgDeviceInfo *(*fetch_device_info_finish) (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);
};

GType og_base_device_get_type (void) G_GNUC_CONST;

GUsbDevice *og_base_device_get_usb_device (OgBaseDevice *self);

void og_base_device_fetch_device_info_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
OgDeviceInfo *og_base_device_fetch_device_info_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif /* __OG_BASE_DEVICE_H__ */
