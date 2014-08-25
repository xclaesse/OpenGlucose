#ifndef __OG_DUMMY_DEVICE_H__
#define __OG_DUMMY_DEVICE_H__

#include "base-device.h"

G_BEGIN_DECLS

#define OG_TYPE_DUMMY_DEVICE \
    (og_dummy_device_get_type ())
#define OG_DUMMY_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OG_TYPE_DUMMY_DEVICE, \
        OgDummyDevice))
#define OG_DUMMY_DEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OG_TYPE_DUMMY_DEVICE, \
        OgDummyDeviceClass))
#define OG_IS_DUMMY_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OG_TYPE_DUMMY_DEVICE))
#define OG_IS_DUMMY_DEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OG_TYPE_DUMMY_DEVICE))
#define OG_DUMMY_DEVICE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OG_TYPE_DUMMY_DEVICE, \
        OgDummyDeviceClass))

typedef struct _OgDummyDevice OgDummyDevice;
typedef struct _OgDummyDeviceClass OgDummyDeviceClass;
typedef struct _OgDummyDevicePrivate OgDummyDevicePrivate;

struct _OgDummyDevice {
  OgBaseDevice parent;

  OgDummyDevicePrivate *priv;
};

struct _OgDummyDeviceClass {
  OgBaseDeviceClass parent_class;
};

GType og_dummy_device_get_type (void) G_GNUC_CONST;

OgBaseDevice *og_dummy_device_new (void);

G_END_DECLS

#endif /* __OG_DUMMY_DEVICE_H__ */
