#include "config.h"

#include "device-info.h"

OgDeviceInfo *
og_device_info_new (void)
{
  OgDeviceInfo *self;

  self = g_slice_new0 (OgDeviceInfo);
  self->records = g_ptr_array_new_with_free_func (
      (GDestroyNotify) og_record_free);

  return self;
}

void
og_device_info_free (OgDeviceInfo *self)
{
  g_free (self->serial_number);
  g_clear_pointer (&self->datetime, g_date_time_unref);
  g_ptr_array_unref (self->records);
  g_slice_free (OgDeviceInfo, self);
}

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
  g_date_time_unref (self->datetime);
  g_slice_free (OgRecord, self);
}
