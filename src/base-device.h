#ifndef __OG_BASE_DEVICE_H__
#define __OG_BASE_DEVICE_H__

#include <gusb.h>

#include "record.h"

G_BEGIN_DECLS

typedef enum
{
  OG_BASE_DEVICE_STATUS_NONE,
  OG_BASE_DEVICE_STATUS_BUZY,
  OG_BASE_DEVICE_STATUS_READY,
  OG_BASE_DEVICE_STATUS_ERROR,
} OgBaseDeviceStatus;
#define OG_LAST_BASE_DEVICE_STATUS OG_BASE_DEVICE_STATUS_ERROR

typedef enum
{
  OG_BASE_DEVICE_ERROR_BUZY,
  OG_BASE_DEVICE_ERROR_PARSER,
  OG_BASE_DEVICE_ERROR_UNEXPECTED,
} OgBaseDeviceError;
#define OG_BASE_DEVICE_ERROR og_base_device_error_quark()
GQuark og_base_device_error_quark (void);

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
  OgBaseDeviceStatus (*get_status) (OgBaseDevice *self);

  void (*prepare_async) (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
  gboolean (*prepare_finish) (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);

  /* Those vfunc can return NULL until the first call to
   * prepare_async() succeeds */
  const gchar *(*get_serial_number) (OgBaseDevice *self);
  GDateTime *(*get_clock) (OgBaseDevice *self,
      GDateTime **system_clock);
  const OgRecord * const *(*get_records) (OgBaseDevice *self);
};

GType og_base_device_get_type (void) G_GNUC_CONST;

/* Virtual methods */

const gchar *og_base_device_get_name (OgBaseDevice *self);
OgBaseDeviceStatus og_base_device_get_status (OgBaseDevice *self);

void og_base_device_prepare_async (OgBaseDevice *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean og_base_device_prepare_finish (OgBaseDevice *self,
    GAsyncResult *result,
    GError **error);

const gchar *og_base_device_get_serial_number (OgBaseDevice *self);
GDateTime *og_base_device_get_clock (OgBaseDevice *self,
    GDateTime **system_clock);
const OgRecord * const *og_base_device_get_records (OgBaseDevice *self);

G_END_DECLS

#endif /* __OG_BASE_DEVICE_H__ */
