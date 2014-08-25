#ifndef __OG_BASE_DEVICE_H__
#define __OG_BASE_DEVICE_H__

#include <gusb.h>

#include "record.h"

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

  /* Must always return non-empty, human-readable string */
  const gchar *(*get_name) (OgBaseDevice *self);

  void (*refresh_device_info_async) (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
  gboolean (*refresh_device_info_finish) (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);

  /* Those vfunc can return NULL until the first call to
   * refresh_device_info_async() succeeds */
  const gchar *(*get_serial_number) (OgBaseDevice *self);
  GDateTime *(*get_clock) (OgBaseDevice *self,
      GDateTime **system_clock);
  const OgRecord * const *(*get_records) (OgBaseDevice *self);
};

typedef enum
{
  OG_BASE_DEVICE_STATUS_NONE,
  OG_BASE_DEVICE_STATUS_REFRESHING,
  OG_BASE_DEVICE_STATUS_READY,
  OG_BASE_DEVICE_STATUS_FAILED,
} OgBaseDeviceStatus;
#define OG_LAST_BASE_DEVICE_STATUS OG_BASE_DEVICE_STATUS_REFRESHING

GType og_base_device_get_type (void) G_GNUC_CONST;

OgBaseDeviceStatus og_base_device_get_status (OgBaseDevice *self);
void og_base_device_change_status (OgBaseDevice *self,
    OgBaseDeviceStatus status);

/* Virtual methods */

const gchar *og_base_device_get_name (OgBaseDevice *self);

void og_base_device_refresh_device_info_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean og_base_device_refresh_device_info_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);

const gchar *og_base_device_get_serial_number (OgBaseDevice *self);
GDateTime *og_base_device_get_clock (OgBaseDevice *self,
    GDateTime **system_clock);
const OgRecord * const *og_base_device_get_records (OgBaseDevice *self);

G_END_DECLS

#endif /* __OG_BASE_DEVICE_H__ */
