#ifndef __OG_RECORD_H__
#define __OG_RECORD_H__

#include <glib.h>

G_BEGIN_DECLS

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

#endif /* __OG_RECORD__H__ */
