#include "config.h"

#include "dummy-device.h"

#include <math.h>

G_DEFINE_TYPE (OgDummyDevice, og_dummy_device, OG_TYPE_BASE_DEVICE)

struct _OgDummyDevicePrivate
{
  OgBaseDeviceStatus status;
  GPtrArray *records;
};

static void
og_dummy_device_init (OgDummyDevice *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_DUMMY_DEVICE, OgDummyDevicePrivate);
}

static gboolean
prepare_timeout_cb (gpointer user_data)
{
  GTask *task = user_data;
  OgDummyDevice *self = g_task_get_source_object (task);
  GRand *rand;
  GDateTime *now;
  GDateTime *dt;

  self->priv->records = g_ptr_array_new_with_free_func (
      (GDestroyNotify) og_record_free);

  rand = g_rand_new ();
  now = g_date_time_new_now_local ();

  /* Generate random glycemia for the last 2 years */
  dt = g_date_time_add_years (now, -2);
  while (g_date_time_compare (dt, now) < 0)
    {
      GDateTime *tmp;
      gdouble delta;

      delta = 5 - log2 (g_rand_double_range (rand, 1, (1 << 5) + 1));
      delta *= g_rand_boolean (rand) ? 1 : -1;
      delta *= 20;

      g_ptr_array_add (self->priv->records, og_record_new (
          g_date_time_get_year (dt),
          g_date_time_get_month (dt),
          g_date_time_get_day_of_month (dt),
          g_date_time_get_hour (dt),
          g_rand_int_range (rand, 0, 60),
          115 + (int) delta));

      tmp = g_date_time_add_hours (dt, g_rand_int_range (rand, 1, 13));
      g_date_time_unref (dt);
      dt = tmp;
    }
  g_ptr_array_add (self->priv->records, NULL);

  self->priv->status = OG_BASE_DEVICE_STATUS_READY;
  g_object_notify ((GObject *) self, "status");

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);

  g_rand_free (rand);
  g_date_time_unref (now);
  g_date_time_unref (dt);

  return G_SOURCE_REMOVE;
}

static void
prepare_async (OgBaseDevice *base,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgDummyDevice *self = (OgDummyDevice *) base;
  GTask *task;

  g_return_if_fail (OG_IS_DUMMY_DEVICE (base));

  task = g_task_new (self, cancellable, callback, user_data);
  self->priv->status = OG_BASE_DEVICE_STATUS_BUZY;
  g_object_notify ((GObject *) self, "status");

  g_timeout_add (1000, prepare_timeout_cb, task);
}

static gboolean
prepare_finish (OgBaseDevice *base,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, base), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static const gchar *
get_name (OgBaseDevice *base)
{
  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), NULL);

  return "Dummy Glucometer";
}

static OgBaseDeviceStatus
get_status (OgBaseDevice *base)
{
  OgDummyDevice *self = (OgDummyDevice *) base;

  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), OG_BASE_DEVICE_STATUS_ERROR);

  return self->priv->status;
}

static const gchar *
get_serial_number (OgBaseDevice *base)
{
  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), NULL);

  return "1234";
}

static GDateTime *
get_clock (OgBaseDevice *base,
    GDateTime **system_clock)
{
  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), NULL);

  if (system_clock != NULL)
    *system_clock = g_date_time_new_now_local ();

  return g_date_time_new_now_local ();
}

static const OgRecord * const *
get_records (OgBaseDevice *base)
{
  OgDummyDevice *self = (OgDummyDevice *) base;

  g_return_val_if_fail (OG_IS_DUMMY_DEVICE (base), NULL);

  if (self->priv->records == NULL)
    return NULL;

  return (const OgRecord * const *) self->priv->records->pdata;
}

static void
og_dummy_device_class_init (OgDummyDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  OgBaseDeviceClass *base_class = OG_BASE_DEVICE_CLASS (klass);

  base_class->get_name = get_name;
  base_class->get_status = get_status;
  base_class->prepare_async = prepare_async;
  base_class->prepare_finish = prepare_finish;
  base_class->get_serial_number = get_serial_number;
  base_class->get_clock = get_clock;
  base_class->get_records = get_records;

  g_type_class_add_private (object_class, sizeof (OgDummyDevicePrivate));
}

OgBaseDevice *
og_dummy_device_new (void)
{
  return g_object_new (OG_TYPE_DUMMY_DEVICE,
      NULL);
}
