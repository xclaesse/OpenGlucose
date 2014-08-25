#ifndef __OG_MAIN_WINDOW_H__
#define __OG_MAIN_WINDOW_H__

#include <gtk/gtk.h>

#include "base-device.h"
#include "openglucose-resources.h"

G_BEGIN_DECLS

#define OG_TYPE_MAIN_WINDOW \
    (og_main_window_get_type ())
#define OG_MAIN_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OG_TYPE_MAIN_WINDOW, \
        OgMainWindow))
#define OG_MAIN_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OG_TYPE_MAIN_WINDOW, \
        OgMainWindowClass))
#define OG_IS_MAIN_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OG_TYPE_MAIN_WINDOW))
#define OG_IS_MAIN_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OG_TYPE_MAIN_WINDOW))
#define OG_MAIN_WINDOW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OG_TYPE_MAIN_WINDOW, \
        OgMainWindowClass))

typedef struct _OgMainWindow OgMainWindow;
typedef struct _OgMainWindowClass OgMainWindowClass;
typedef struct _OgMainWindowPrivate OgMainWindowPrivate;

struct _OgMainWindow {
  GtkApplicationWindow parent;

  OgMainWindowPrivate *priv;
};

struct _OgMainWindowClass {
  GtkApplicationWindowClass parent_class;
};

GType og_main_window_get_type (void) G_GNUC_CONST;

GtkWidget *og_main_window_new (GtkApplication *application);
void og_main_window_add_device (OgMainWindow *self,
    OgBaseDevice *device);
void og_main_window_remove_device (OgMainWindow *self,
    OgBaseDevice *device);

G_END_DECLS

#endif /* __OG_MAIN_WINDOW_H__ */
