#include "config.h"

#include "insulinx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

G_DEFINE_TYPE (OgInsulinx, og_insulinx, OG_TYPE_BASE_DEVICE)

#define BUFFER_SIZE 64

/* Known commands:
 * $time?, $date?, $serlnum?, $ptname?, $ptid?, $lang?, $clktyp?, $taglang?,
 * $custthm?, $tagorder?, $ntsound?, $wktrend?, $foodunits?, $carbratio?,
 * $svgsdef?, $svgsratio?, $actinscal?, $insdose?, $gunits?, $cttype?, $bgdrop?,
 * $bgtrgt?, $bgtgrng?, $inslog?, $corsetup?, $iobstatus?, $btsound?, $hwver?,
 * $swver?, $frststrt?, $alllang?, $ioblog?, $actthm?, $tagsenbl?, $event?,
 * $inslock?
 */

static gboolean parse_serial_number (OgInsulinx *self,
    const gchar *msg,
    GError **error);
static gboolean parse_result (OgInsulinx *self,
    const gchar *msg,
    GError **error);
static gboolean parse_date (OgInsulinx *self,
    const gchar *msg,
    GError **error);
static gboolean parse_time (OgInsulinx *self,
    const gchar *msg,
    GError **error);

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

struct _OgInsulinxPrivate
{
  /* Borrowed */
  GUsbDevice *usb_device;

  GTask *task;
  gint state;
  guint8 buffer[BUFFER_SIZE];

  OgDeviceInfo *info;
  guint year, month, day;
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

static void
og_insulinx_init (OgInsulinx *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_INSULINX, OgInsulinxPrivate);
  self->priv->state = -1;
}

static void
constructed (GObject *object)
{
  OgInsulinx *self = (OgInsulinx *) object;
  GError *error = NULL;

  g_debug ("New Abbott FreeStyle InsuLinx device");

  G_OBJECT_CLASS (og_insulinx_parent_class)->constructed (object);

  self->priv->usb_device = og_base_device_get_usb_device (
      (OgBaseDevice *) self);

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

static void fetch_next (OgInsulinx *self);

static void
fetch_device_info_reset (OgInsulinx *self)
{
    g_clear_object (&self->priv->task);
    g_clear_pointer (&self->priv->info, og_device_info_free);
    self->priv->state = -1;
}

static gboolean
parse_serial_number (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  if (sscanf (msg, "%ms\r\n", &self->priv->info->serial_number) != 1)
    {
      self->priv->info->serial_number = NULL;
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing serial number");
      return FALSE;
    }

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

  n_parsed = sscanf (msg, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
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

  g_ptr_array_add (self->priv->info->records,
      og_record_new (year, month, day, hour, minute, glycemia));

  return TRUE;
}

static gboolean
parse_date (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  /* Temporaly store those values, we'll create the GDateTime in next state. */
  if (sscanf (msg, "%u,%u,%u\r\n", &self->priv->month, &self->priv->day,
          &self->priv->year) != 3)
    {
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing date");
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_time (OgInsulinx *self,
    const gchar *msg,
    GError **error)
{
  guint hour, minute;

  if (sscanf (msg, "%u,%u\r\n", &hour, &minute) != 2)
    {
      g_set_error (error, OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_PARSER,
          "Error parsing time");
      return FALSE;
    }

  /* We should have parsed the date in previous state */
  self->priv->info->datetime = g_date_time_new_local (
      self->priv->year, self->priv->month, self->priv->day,
      hour, minute, 0);

  return TRUE;
}

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
      g_task_return_error (self->priv->task, error);
      fetch_device_info_reset (self);
      return;
    }

  req = &requests[self->priv->state];

  /* Extract the msg from buffer and make sure it is 0-terminated */
  msg_len = self->priv->buffer[1];
  g_assert (msg_len <= BUFFER_SIZE - 3);
  msg = (gchar *) self->priv->buffer + 2;
  msg[msg_len] = '\0';

  g_debug ("got msg: '%s'", msg);

  /* Check if more messages needs to be fetched */
  if (req->code == 0x60)
    {
      if (self->priv->buffer[0] != 0x60)
        {
          /* FIXME: Not sure what this msg is */
          fetch_more = TRUE;
          goto out;
        }

      if (g_str_has_suffix (msg, "CMD FAIL!\r\n"))
        {
          g_task_return_new_error (self->priv->task,
              OG_INSULINX_ERROR,
              OG_INSULINX_ERROR_CMD_FAILED,
              "Command failed");
          fetch_device_info_reset (self);
          return;
        }

      /* If the message does NOT end with "CMD OK" then more messages
       * needs to be fetched */
      if (!g_str_has_suffix (msg, "CMD OK\r\n"))
        fetch_more = TRUE;
    }

  /* Parse the message */
  if (req->parse != NULL)
    {
      if (!req->parse (self, msg, &error))
        {
          g_task_return_error (self->priv->task, error);
          fetch_device_info_reset (self);
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
          g_task_get_cancellable (self->priv->task),
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
      g_task_return_error (self->priv->task, error);
      fetch_device_info_reset (self);
      return;
    }

  /* Fetch the reply */
  g_usb_device_interrupt_transfer_async (self->priv->usb_device,
      0x81,
      self->priv->buffer, BUFFER_SIZE,
      0,
      g_task_get_cancellable (self->priv->task),
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

  /* Are we done? */
  if (self->priv->state == G_N_ELEMENTS (requests))
    {
      g_task_return_pointer (self->priv->task,
          self->priv->info, (GDestroyNotify) og_device_info_free);
      self->priv->info = NULL;
      fetch_device_info_reset (self);
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
      g_task_get_cancellable (self->priv->task),
      request_control_transfer_cb,
      self);
}

static void
fetch_device_info_async (OgBaseDevice *base,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgInsulinx *self;
  GTask *task;

  g_return_if_fail (OG_IS_INSULINX (base));
  self = (OgInsulinx *) base;

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->priv->state != -1)
    {
      g_task_return_new_error (task,
          OG_INSULINX_ERROR,
          OG_INSULINX_ERROR_FETCH_ONGOING,
          "Device is already fetching information");
      g_object_unref (task);
      return;
    }

  g_assert (self->priv->task == NULL);
  g_assert (self->priv->info == NULL);
  self->priv->task = task;
  self->priv->info = og_device_info_new ();

  fetch_next (self);
}

static OgDeviceInfo *
fetch_device_info_finish (OgBaseDevice *base,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);
  g_return_val_if_fail (g_task_is_valid (result, base), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
og_insulinx_class_init (OgInsulinxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  OgBaseDeviceClass *base_class = OG_BASE_DEVICE_CLASS (klass);

  object_class->constructed = constructed;
  base_class->fetch_device_info_async = fetch_device_info_async;
  base_class->fetch_device_info_finish = fetch_device_info_finish;

  g_type_class_add_private (object_class, sizeof (OgInsulinxPrivate));
}

OgBaseDevice *
og_insulinx_new (GUsbDevice *usb_device)
{
  return g_object_new (OG_TYPE_INSULINX,
      "usb-device", usb_device,
      NULL);
}
