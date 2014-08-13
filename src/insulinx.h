#ifndef __OG_INSULINX_H__
#define __OG_INSULINX_H__

#include "base-device.h"

G_BEGIN_DECLS

#define OG_TYPE_INSULINX \
    (og_insulinx_get_type ())
#define OG_INSULINX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OG_TYPE_INSULINX, \
        OgInsulinx))
#define OG_INSULINX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OG_TYPE_INSULINX, \
        OgInsulinxClass))
#define OG_IS_INSULINX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OG_TYPE_INSULINX))
#define OG_IS_INSULINX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OG_TYPE_INSULINX))
#define OG_INSULINX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OG_TYPE_INSULINX, \
        OgInsulinxClass))

typedef struct _OgInsulinx OgInsulinx;
typedef struct _OgInsulinxClass OgInsulinxClass;
typedef struct _OgInsulinxPrivate OgInsulinxPrivate;

struct _OgInsulinx {
  OgBaseDevice parent;

  OgInsulinxPrivate *priv;
};

struct _OgInsulinxClass {
  OgBaseDeviceClass parent_class;
};

GType og_insulinx_get_type (void) G_GNUC_CONST;

OgBaseDevice *og_insulinx_new (GUsbDevice *usb_device);

G_END_DECLS

#endif /* __OG_INSULINX_H__ */
