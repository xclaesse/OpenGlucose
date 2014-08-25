#include "config.h"

#include "insulinx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

G_DEFINE_TYPE (OgInsulinx, og_insulinx, OG_TYPE_BASE_DEVICE)

#define BUFFER_SIZE 64

struct _OgInsulinxPrivate
{
  GUsbDevice *usb_device;

  GQueue refresh_tasks;
  gint state;
  guint8 buffer[BUFFER_SIZE];

  gchar *serial_number;
  GDateTime *device_clock;
  GDateTime *system_clock;
  GPtrArray *records;

  guint year, month, day;
};

enum
{
  PROP_0,
  PROP_USB_DEVICE,
};

typedef enum
{
  OG_INSULINX_ERROR_CMD_FAILED,
  OG_INSULINX_ERROR_PARSER,
  OG_INSULINX_ERROR_FETCH_ONGOING,
} OgInsulinxError;

#define OG_INSULINX_ERROR og_insulinx_error_quark()
GQuark og_insulinx_error_quark (void);
G_DEFINE_QUARK (og-insulinx-error-quark, og_insulinx_error)

/* Known commands:
 * $time?, $date?, $serlnum?, $ptname?, $ptid?, $lang?, $clktyp?, $taglang?,
 * $custthm?, $tagorder?, $ntsound?, $wktrend?, $foodunits?, $carbratio?,
 * $svgsdef?, $svgsratio?, $actinscal?, $insdose?, $gunits?, $cttype?, $bgdrop?,
 * $bgtrgt?, $bgtgrng?, $inslog?, $corsetup?, $iobstatus?, $btsound?, $hwver?,
 * $swver?, $frststrt?, $alllang?, $ioblog?, $actthm?, $tagsenbl?, $event?,
 * $inslock?
 */
#define DECLARE_PARSER(func) \
  static gboolean parse_##func (OgInsulinx *self, \
      const gchar *msg, \
      GError **error)
DECLARE_PARSER (serial_number);
DECLARE_PARSER (result);
DECLARE_PARSER (date);
DECLARE_PARSER (time);
#undef DECLARE_PARSER

typedef struct
{
  guint8 code;
  const gchar *cmd;
  gboolean (*parse) (OgInsulinx *self,
      const gchar *msg,
      GError **error);
} Request;

static Request requests[] = {
  { 0x04, "", NULL },
  { 0x05, "", parse_serial_number },
  { 0x15, "", NULL },
  { 0x01, "", NULL },
  { 0x60, "$result?\r\n", parse_result },
  { 0x60, "$date?\r\n", parse_date },
  { 0x60, "$time?\r\n", parse_time },
};

static void
clear_device_info (OgInsulinx *self)
{
  g_clear_pointer (&self->priv->serial_number, g_free);
  g_clear_pointer (&self->priv->device_clock, g_date_time_unref);
  g_clear_pointer (&self->priv->system_clock, g_date_time_unref);
  g_clear_pointer (&self->priv->records, g_ptr_array_unref);
}

static void
og_insulinx_init (OgInsulinx *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_INSULINX, OgInsulinxPrivate);
  self->priv->state = -1;
  g_queue_init (&self->priv->refresh_tasks);
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  OgInsulinx *self = (OgInsulinx *) object;

  switch (property_id)
    {
      case PROP_USB_DEVICE:
        g_value_set_object (value, self->priv->usb_device);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  OgInsulinx *self = (OgInsulinx *) object;

  switch (property_id)
    {
      case PROP_USB_DEVICE:
        g_assert (self->priv->usb_device == NULL);
        self->priv->usb_device = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
constructed (GObject *object)
{
  OgInsulinx *self = (OgInsulinx *) object;
  GError *error = NULL;

  G_OBJECT_CLASS (og_insulinx_parent_class)->constructed (object);

  g_assert (self->priv->usb_device != NULL);
  if (!g_usb_device_open (self->priv->usb_device, &error))
    {
      g_warning ("Error opening device: %s", error->message);
      goto out;
    }

  if (!g_usb_device_claim_interface (self->priv->usb_device, 0,
          G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
          &error))
    {
      g_warning ("Error claiming interface: %s", error->message);
      goto out;
    }

  if (!g_usb_device_set_configuration (self->priv->usb_device, 1, &error))
    {
      g_warning ("Error setting configuration: %s", error->message);
      goto out;
    }

  if (!g_usb_device_control_transfer (self->priv->usb_device,
          G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
          G_USB_DEVICE_REQUEST_TYPE_CLASS,
          G_USB_DEVICE_RECIPIENT_INTERFACE,
          0x0a, /* SET_IDLE */
          0,
          0,
          NULL, 0,
          NULL,
          0,
          NULL,
          &error))
    {
      g_warning ("Error setting IDLE: %s", error->message);
      goto out;
    }

out:
  g_clear_error (&error);
}

static void
dispose (GObject *object)
{
  OgInsulinx *self = (OgInsulinx *) object;
  GError *error = NULL;

  clear_device_info (self);

  if (self->priv->usb_device != NULL)
    {
      if (!g_usb_device_close (self->priv->usb_device, &error))
        g_warning ("Error closing device: %s", error->message);
      g_clear_error (&error);
    }
  g_clear_object (&self->priv->usb_device);

  G_OBJECT_CLASS (og_insulinx_parent_class)->dispose (object);
}

static gboolean
parse_serial_number (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  self->priv->serial_number = g_strdup (msg);
  return TRUE;
}

static gboolean
parse_result (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  guint type, month, day, year, hour, minute, glycemia;
  guint ignore;
  gint n_parsed;

  n_parsed = sscanf (msg, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
      &type,
      &ignore, /* FIXME: What is that? */
      &month, &day, &year,
      &hour, &minute,
      &ignore, /* FIXME: What is that? */
      &ignore, /* FIXME: What is that? */
      &ignore, /* FIXME: What is that? */
      &ignore, /* FIXME: What is that? */
      &ignore, /* FIXME: What is that? */
      &ignore, /* FIXME: What is that? */
      &glycemia,
      &ignore, /* FIXME: What is that? */
      &ignore); /* FIXME: What is that? */

  /* FIXME: Not sure what are those results */
  if (type != 0)
    return TRUE;

  if (n_parsed != 16)
    {
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing result");
      return FALSE;
    }

  year += 2000;

  g_ptr_array_add (self->priv->records,
      og_record_new (year, month, day, hour, minute, glycemia));

  return TRUE;
}

static gboolean
parse_date (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  /* Temporaly store those values, we'll create the GDateTime in next state. */
  if (sscanf (msg, "%u,%u,%u", &self->priv->month, &self->priv->day,
          &self->priv->year) != 3)
    {
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing date");
      return FALSE;
    }

  self->priv->year += 2000;

  return TRUE;
}

static gboolean
parse_time (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  guint hour, minute;

  if (sscanf (msg, "%u,%u", &hour, &minute) != 2)
    {
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing time");
      return FALSE;
    }

  /* We should have parsed the date in previous state */
  self->priv->device_clock = g_date_time_new_local (
      self->priv->year, self->priv->month, self->priv->day,
      hour, minute, 0);
  self->priv->system_clock = g_date_time_new_now_local ();

  return TRUE;
}

static void
refresh_return (OgInsulinx *self,
    GError *error)
{
  GTask *task;

  self->priv->state = -1;

  if (error != NULL)
    {
      clear_device_info (self);
      og_base_device_change_status ((OgBaseDevice *) self,
          OG_BASE_DEVICE_STATUS_FAILED);
    }
  else
    {
      g_ptr_array_add (self->priv->records, NULL);
      og_base_device_change_status ((OgBaseDevice *) self,
          OG_BASE_DEVICE_STATUS_READY);
    }

  while ((task = g_queue_pop_head (&self->priv->refresh_tasks)) != NULL)
    {
      if (error != NULL)
        g_task_return_error (task, g_error_copy (error));
      else
        g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }

  g_clear_error (&error);
}

static void fetch_next (OgInsulinx *self);

static void
request_interrupt_transfer_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgInsulinx *self = user_data;
  Request *req;
  gssize len;
  gchar *msg;
  guint8 msg_len;
  gboolean fetch_more = FALSE;
  GError *error = NULL;

  len = g_usb_device_interrupt_transfer_finish (self->priv->usb_device, result,
      &error);
  if (len < BUFFER_SIZE)
    {
      refresh_return (self, error);
      return;
    }

  req = &requests[self->priv->state];

  /* Extract the msg from buffer and make sure it is 0-terminated */
  msg_len = self->priv->buffer[1];
  g_assert (msg_len <= BUFFER_SIZE - 3);
  msg = (gchar *) self->priv->buffer + 2;
  msg[msg_len] = '\0';

  g_strstrip (msg);

  g_debug ("got msg code 0x%02x: '%s'", self->priv->buffer[0], msg);

  /* Check if more messages needs to be fetched */
  if (req->code == 0x60)
    {
      if (self->priv->buffer[0] != 0x60)
        {
          /* FIXME: Not sure what this msg is */
          fetch_more = TRUE;
          goto out;
        }

      if (g_str_has_suffix (msg, "CMD FAIL!"))
        {
          g_set_error (&error,
              OG_INSULINX_ERROR,
              OG_INSULINX_ERROR_CMD_FAILED,
              "Command failed");
          refresh_return (self, error);
          return;
        }

      /* If the message does NOT end with "CMD OK" then more messages
       * needs to be fetched */
      if (!g_str_has_suffix (msg, "CMD OK"))
        fetch_more = TRUE;
    }

  /* Parse the message */
  if (req->parse != NULL)
    {
      if (!req->parse (self, msg, &error))
        {
          refresh_return (self, error);
          return;
        }
    }

out:
  if (fetch_more)
    {
      g_usb_device_interrupt_transfer_async (self->priv->usb_device,
          0x81,
          self->priv->buffer, BUFFER_SIZE,
          0,
          NULL,
          request_interrupt_transfer_cb,
          self);
    }
  else
    {
      fetch_next (self);
    }
}

static void
request_control_transfer_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgInsulinx *self = user_data;
  gssize len;
  GError *error = NULL;

  len = g_usb_device_control_transfer_finish (self->priv->usb_device, result,
      &error);
  if (len < BUFFER_SIZE)
    {
      refresh_return (self, error);
      return;
    }

  /* Fetch the reply */
  g_usb_device_interrupt_transfer_async (self->priv->usb_device,
      0x81,
      self->priv->buffer, BUFFER_SIZE,
      0,
      NULL,
      request_interrupt_transfer_cb,
      self);
}

static void
fetch_next (OgInsulinx *self)
{
  Request *req;
  gsize len;

  self->priv->state++;
  g_assert (self->priv->state >= 0);
  g_assert ((guint) self->priv->state <= G_N_ELEMENTS (requests));

  og_base_device_change_status ((OgBaseDevice *) self,
      OG_BASE_DEVICE_STATUS_REFRESHING);

  /* Are we done? */
  if (self->priv->state == G_N_ELEMENTS (requests))
    {
      refresh_return (self, NULL);
      return;
    }

  req = &requests[self->priv->state];
  len = strlen (req->cmd);
  g_assert (len <= BUFFER_SIZE - 2);

  self->priv->buffer[0] = req->code;
  self->priv->buffer[1] = len;
  g_memmove (self->priv->buffer + 2, req->cmd, len);

  /* Send the request */
  g_debug ("Send request 0x%02x: '%s'", req->code, req->cmd);
  g_usb_device_control_transfer_async (self->priv->usb_device,
      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
      G_USB_DEVICE_REQUEST_TYPE_CLASS,
      G_USB_DEVICE_RECIPIENT_INTERFACE,
      0x09,
      0x0200,
      0,
      self->priv->buffer, BUFFER_SIZE,
      0,
      NULL,
      request_control_transfer_cb,
      self);
}

static void
refresh_device_info_async (OgBaseDevice *base,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgInsulinx *self;
  GTask *task;

  g_return_if_fail (OG_IS_INSULINX (base));
  self = (OgInsulinx *) base;

  task = g_task_new (self, cancellable, callback, user_data);
  g_queue_push_tail (&self->priv->refresh_tasks, task);

  if (self->priv->state == -1)
    {
      clear_device_info (self);
      self->priv->records = g_ptr_array_new_with_free_func (
          (GDestroyNotify) og_record_free);
      fetch_next (self);
    }
}

static gboolean
refresh_device_info_finish (OgBaseDevice *base,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (OG_IS_INSULINX (base), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, base), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static const gchar *
get_name (OgBaseDevice *base)
{
  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  return "Abbott FreeStyle InsuLinx";
}

static const gchar *
get_serial_number (OgBaseDevice *base)
{
  OgInsulinx *self;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);
  self = (OgInsulinx *) base;

  return self->priv->serial_number;
}

static GDateTime *
get_clock (OgBaseDevice *base,
    GDateTime **system_clock)
{
  OgInsulinx *self;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);
  self = (OgInsulinx *) base;

  if (system_clock != NULL)
    *system_clock = self->priv->system_clock;

  return self->priv->device_clock;
}

static const OgRecord * const *
get_records (OgBaseDevice *base)
{
  OgInsulinx *self;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);
  self = (OgInsulinx *) base;

  if (self->priv->records == NULL)
    return NULL;

  return (const OgRecord * const *) self->priv->records->pdata;
}

static void
og_insulinx_class_init (OgInsulinxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  OgBaseDeviceClass *base_class = OG_BASE_DEVICE_CLASS (klass);
  GParamSpec *param_spec;

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  base_class->get_name = get_name;
  base_class->refresh_device_info_async = refresh_device_info_async;
  base_class->refresh_device_info_finish = refresh_device_info_finish;
  base_class->get_serial_number = get_serial_number;
  base_class->get_clock = get_clock;
  base_class->get_records = get_records;

  g_type_class_add_private (object_class, sizeof (OgInsulinxPrivate));

  param_spec = g_param_spec_object ("usb-device",
      "USB Device",
      "The #GUsbDevice associated with this glucometer",
      G_USB_TYPE_DEVICE,
      G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_USB_DEVICE, param_spec);
}

OgBaseDevice *
og_insulinx_new (GUsbDevice *usb_device)
{
  return g_object_new (OG_TYPE_INSULINX,
      "usb-device", usb_device,
      NULL);
}
