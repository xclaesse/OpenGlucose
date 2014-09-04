#ifndef __OG_DEVICE_WIDGET_H__
#define __OG_DEVICE_WIDGET_H__

#include <gtk/gtk.h>

#include "base-device.h"

G_BEGIN_DECLS

#define OG_TYPE_DEVICE_WIDGET \
    (og_device_widget_get_type ())
#define OG_DEVICE_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OG_TYPE_DEVICE_WIDGET, \
        OgDeviceWidget))
#define OG_DEVICE_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OG_TYPE_DEVICE_WIDGET, \
        OgDeviceWidgetClass))
#define OG_IS_DEVICE_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OG_TYPE_DEVICE_WIDGET))
#define OG_IS_DEVICE_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OG_TYPE_DEVICE_WIDGET))
#define OG_DEVICE_WIDGET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OG_TYPE_DEVICE_WIDGET, \
        OgDeviceWidgetClass))

typedef struct _OgDeviceWidget OgDeviceWidget;
typedef struct _OgDeviceWidgetClass OgDeviceWidgetClass;
typedef struct _OgDeviceWidgetPrivate OgDeviceWidgetPrivate;

struct _OgDeviceWidget {
  GtkBin parent;

  OgDeviceWidgetPrivate *priv;
};

struct _OgDeviceWidgetClass {
  GtkBinClass parent_class;
};

GType og_device_widget_get_type (void) G_GNUC_CONST;

GtkWidget *og_device_widget_new (OgBaseDevice *device);

G_END_DECLS

#endif /* __OG_DEVICE_WIDGET_H__ */
