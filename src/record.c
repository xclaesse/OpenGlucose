#include "config.h"

#include "record.h"

OgRecord *
og_record_new (guint year,
    guint month,
    guint day,
    guint hour,
    guint minute,
    guint glycemia)
{
  OgRecord *self;

  self = g_slice_new0 (OgRecord);
  self->datetime = g_date_time_new_local (year, month, day, hour, minute, 0);
  self->glycemia = glycemia;

  return self;
}

void
og_record_free (OgRecord *self)
{
  if (self == NULL)
    return;

  g_date_time_unref (self->datetime);
  g_slice_free (OgRecord, self);
}
