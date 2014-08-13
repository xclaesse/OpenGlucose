#ifndef __OG_DEVICE_INFO_H__
#define __OG_DEVICE_INFO_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  gchar *serial_number;
  GDateTime *datetime;
  /* Owned OgRecord */
  GPtrArray *records;
} OgDeviceInfo;

OgDeviceInfo *og_device_info_new (void);
void og_device_info_free (OgDeviceInfo *self);

typedef struct
{
  GDateTime *datetime;
  guint glycemia;
} OgRecord;

OgRecord *og_record_new (guint year,
    guint month,
    guint day,
    guint hour,
    guint minute,
    guint glycemia);

void og_record_free (OgRecord *self);

G_END_DECLS

#endif /* __OG_DEVICE_INFO__H__ */
