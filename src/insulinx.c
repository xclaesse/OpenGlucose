#include "config.h"

#include "insulinx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Abbott FreeStyle InsuLinx reverse-engineered protocol.
 *
 * This is based on USB logs of 'auto-assist' Windows application, captured
 * using USBSnoop (http://www.pcausa.com/Utilities/UsbSnoop/).
 * See log files and parser.py openglucose/data/insulinx/.
 *
 * Buffers of 64 bytes are transferred between the host and the device. The host
 * sends a request to the device and pull the reply. The first byte of the
 * buffer is (probably) the type of the message, the 2nd byte is the length of
 * the message, and rest is ASCII message. Bytes after lenght+2 are meaningless.
 *
 * There is first an init sequence, it is still a bit obscure but it goes like
 * this:
 *
 * 1) No idea if it has particular meaning
 * Request: code=0x4,  msg=""
 * Reply:   code=0x34, msg="0xc"
 *
 * 2) That's the serial number followed by \0
 * Request: code=0x5,  msg=""
 * Reply:   code=0x6,  msg="JAGT241-U62420x0"
 *
 * 3) That's device' software version followed by \0
 * Request: code=0x15, msg=""
 * Reply:   code=0x35, msg="1.400x0"
 *
 * 4) No idea if it has particular meaning
 * Request: code=0x1,  msg=""
 * Reply:   code=0x71, msg="0x1"
 *
 * After that all kind of commands can be sent in the form:
 *
 * Request: code=0x60, msg="$foo?\r\n"
 * Reply: code=0x60, msg="bar\r\nCKSM:0000014C\r\nCMD OK\r\n"
 *
 * The reply can be on multiple buffers, thus it need to pull replies until the
 * "CKSM:XXXXXXXX\r\nCMD OK\r\n" is received.
 *
 * CKSM is the checksum of the reply, it is the simple sum of ASCII values in
 * hexadecimal. In the above example it would be:
 * 'b' + 'a' + 'r' + '\r' + '\n' = 0x14c
 *
 * Every 3 requests, the reply will start first with a special buffer starting
 * with 0x22 0x01 0x03. I don't know what it means, they are ignored.
 *
 * To change the value of "foo":
 *
 * Request: code=0x60, msg="$foo,newvalue\r\n"
 * Reply: code=0x60, msg="CKSM:00000000\r\nCMD OK\r\n"
 *
 * Here are all the commands issued by auto-assist when plugging the device:
 *
 *   $serlnum?, $swver?, $date?, $time?, $ptname?, $ptid?, $getrmndrst,0,
 *   $getrmndr,0, $rmdstrorder?, $actthm?, $wktrend?, $gunits?, $clktyp?,
 *   $alllang?, $lang?, $inslock?, $actinscal?, $iobstatus?, $foodunits?,
 *   $svgsdef?, $corsetup?, $insdose?, $inslog?, $inscalsetup?, $carbratio?,
 *   $svgsratio?, $mlcalget,3, $cttype?, $bgdrop?, $bgtrgt?, $bgtgrng?,
 *   $ntsound?, $btsound?, $custthm?, $taglang?, $tagsenbl?, $tagorder?,
 *   $result?, $gettags,2,2, $frststrt?
 */

G_DEFINE_TYPE (OgInsulinx, og_insulinx, OG_TYPE_BASE_DEVICE)

#define BUFFER_SIZE 64
#define DEBUG g_debug
#define DEBUG_MSG debug_msg

typedef void (*ParserFunc) (OgInsulinx *self,
      guint8 code,
      const gchar *msg);

typedef struct
{
  guint8 code;
  gchar *cmd;
  ParserFunc parser;
} Request;

struct _OgInsulinxPrivate
{
  OgBaseDeviceStatus status;
  GUsbDevice *usb_device;

  GCancellable *cancellable;

  /* +1 so we can always add a \0 at the end for safety */
  guint8 send_buffer[BUFFER_SIZE + 1];
  guint8 receive_buffer[BUFFER_SIZE + 1];
  GString *received;
  guint cksm;
  gboolean cksm_received;

  /* GQueue<owned Request> */
  GQueue request_queue;
  Request *req;
  GTask *task;

  gchar *serial_number;
  gchar *sw_version;
  GDateTime *device_clock;
  GDateTime *system_clock;
  GPtrArray *records;
  gchar *first_name;
  gchar *last_name;

  guint year, month, day;
};

enum
{
  PROP_0,
  PROP_USB_DEVICE,
};

static void
ptr_array_add_null_term (GPtrArray *array,
    gpointer ptr)
{
  /* Remove ending NULL first */
  g_ptr_array_remove_index_fast (array, array->len - 1);
  g_ptr_array_add (array, ptr);
  g_ptr_array_add (array, NULL);
}

static void
debug_msg (const gchar *way,
    guint8 code,
    const gchar *msg)
{
  GString *string;
  guint i;

  string = g_string_sized_new (strlen (msg));
  for (i = 0; msg[i] != '\0'; i++)
    {
      if (g_ascii_isprint (msg[i]))
        g_string_append_c (string, msg[i]);
      else if (msg[i] == '\r')
        g_string_append (string, "\\r");
      else if (msg[i] == '\n')
        g_string_append (string, "\\n");
      else
        g_string_append_printf (string, "0x%02x", msg[i]);
    }

  DEBUG ("%s: code=0x%02x, msg=\"%s\"", way, code, string->str);

  g_string_free (string, TRUE);
}

static void
change_status (OgInsulinx *self,
    OgBaseDeviceStatus status)
{
  if (self->priv->status == status)
    return;

  self->priv->status = status;
  g_object_notify ((GObject *) self, "status");
}

static void
report_error (OgInsulinx *self,
    GError *error)
{
  /* Ignore CANCELLED error, it is either voluntary or consequence of an earlier
   * error. */
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      DEBUG ("Error: %s", error->message);
      change_status (self, OG_BASE_DEVICE_STATUS_ERROR);
      g_cancellable_cancel (self->priv->cancellable);
    }

  if (self->priv->task != NULL)
    {
      g_task_return_error (self->priv->task, error);
      g_clear_object (&self->priv->task);
    }
  else
    {
      g_error_free (error);
    }
}

static void
control_transfer_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgInsulinx *self = user_data;
  GError *error = NULL;

  if (g_usb_device_control_transfer_finish (self->priv->usb_device, result,
          &error) < 0)
    {
      report_error (self, error);
      goto out;
    }

out:
  g_object_unref (self);
}

static void
request_queue_continue (OgInsulinx *self)
{
  gsize len;

  if (self->priv->req != NULL)
    return;

  self->priv->req = g_queue_pop_head (&self->priv->request_queue);
  if (self->priv->req == NULL)
    {
      GTask *task = self->priv->task;

      self->priv->task = NULL;
      change_status (self, OG_BASE_DEVICE_STATUS_READY);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }

  change_status (self, OG_BASE_DEVICE_STATUS_BUZY);

  len = strlen (self->priv->req->cmd);
  g_assert (len <= BUFFER_SIZE - 2);

  self->priv->send_buffer[0] = self->priv->req->code;
  self->priv->send_buffer[1] = len;
  g_memmove (self->priv->send_buffer + 2, self->priv->req->cmd, len);

  /* Send the request */
  DEBUG_MSG ("Sent", self->priv->req->code, self->priv->req->cmd);
  g_usb_device_control_transfer_async (self->priv->usb_device,
      G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
      G_USB_DEVICE_REQUEST_TYPE_CLASS,
      G_USB_DEVICE_RECIPIENT_INTERFACE,
      0x09,
      0x0200,
      0,
      self->priv->send_buffer, BUFFER_SIZE,
      0,
      self->priv->cancellable,
      control_transfer_cb,
      g_object_ref (self));
}

static void
queue_request (OgInsulinx *self,
    guint8 code,
    const gchar *cmd,
    ParserFunc parser)
{
  Request *req;

  req = g_slice_new0 (Request);
  req->code = code;
  req->cmd = g_strdup (cmd);
  req->parser = parser;

  g_queue_push_tail (&self->priv->request_queue, req);
  request_queue_continue (self);
}

static void
request_free (Request *req)
{
  g_free (req->cmd);
  g_slice_free (Request, req);
}

static void
request_done (OgInsulinx *self)
{
  g_clear_pointer (&self->priv->req, request_free);
  self->priv->cksm_received = FALSE;
  self->priv->cksm = 0;
  request_queue_continue (self);
}

static gboolean
find_line_break (GString *string,
    guint *pos)
{
  gchar *p;

  p = g_strstr_len (string->str, string->len, "\r\n");
  if (p == NULL)
    return FALSE;

  *pos = p - string->str;
  return TRUE;
}

static guint
checksum (const gchar *str)
{
  guint ret = 0;
  guint i;

  for (i = 0; str[i] != '\0'; i++)
    ret += str[i];

  return ret;
}

static void
parser_common (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  guint pos;

  g_assert (self->priv->req != NULL);
  g_assert (self->priv->req->parser != NULL);

  /* If it is one of the initialization requests, pass it to the specialized
   * parser directly. */
  if (self->priv->req->code != 0x60)
    {
      self->priv->req->parser (self, code, msg);
      return;
    }

  /* FIXME: Not sure what they are, ignore */
  if (code == 0x22)
    {
      if (msg[0] != 0x3 || msg[1] != '\0')
        {
          report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
              OG_BASE_DEVICE_ERROR_PARSER,
              "Received 0x22 buffer with unusual msg"));
        }
      return;
    }

  if (code != 0x60)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Made a 0x60 request and received something else"));
      return;
    }

  /* Accumulate the received msg with what's left unparsed of the previous msg.
   * It can happen that a msg is split into multiple buffers. */
  g_string_append (self->priv->received, msg);

  /* Let's parse what we received line by line */
  while (find_line_break (self->priv->received, &pos))
    {
      gchar *line;
      guint cksm;

      line = self->priv->received->str;
      line[pos] = '\0';

      if (sscanf (line, "CKSM:%8x", &cksm) == 1)
        {
          /* We received the checksum */
          if (cksm != self->priv->cksm)
            {
              report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
                  OG_BASE_DEVICE_ERROR_PARSER,
                  "Checksum mismatch: expected %x, calculated %x",
                  cksm, self->priv->cksm));
              return;
            }
          self->priv->cksm_received = TRUE;
        }
      else if (self->priv->cksm_received)
        {
          /* Previous line was the checksum, the only valid line afterward is
           * "CMD OK", we can start the next request after that. */
          if (!g_str_equal (line, "CMD OK"))
            {
              report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
                  OG_BASE_DEVICE_ERROR_PARSER,
                  "Checksum not followed by \"CMD OK\""));
              return;
            }

          request_done (self);
        }
      else
        {
          /* Give that line to the specialized parser */
          self->priv->req->parser (self, code, line);
          if (self->priv->status == OG_BASE_DEVICE_STATUS_ERROR)
            return;

          /* Incrementaly calculate the checksum, including the line break
           * that we stripped. */
          self->priv->cksm += checksum (line) + checksum ("\r\n");
        }

      g_string_erase (self->priv->received, 0, pos + 2);
    }
}

static void start_interrupt_transfer (OgInsulinx *self);

static void
interrupt_transfer_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  OgInsulinx *self = user_data;
  guint8 code;
  guint8 msg_len;
  gchar *msg;
  GError *error = NULL;

  if (g_usb_device_interrupt_transfer_finish (self->priv->usb_device, result,
          &error) < 0)
    {
      report_error (self, error);
      goto out;
    }

  if (self->priv->req == NULL)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_UNEXPECTED,
          "Received a buffer while nothing was requested"));
      goto out;
    }

  /* 1st byte is the type of the message */
  code = self->priv->receive_buffer[0];

  /* 2nd byte is the length of the message */
  msg_len = self->priv->receive_buffer[1];
  if (msg_len > BUFFER_SIZE - 2)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Message length bigger than buffer size"));
      goto out;
    }

  /* Extract the message and ensure it is 0-terminated */
  msg = (gchar *) self->priv->receive_buffer + 2;
  msg[msg_len] = '\0';

  DEBUG_MSG ("Received", code, msg);
  parser_common (self, code, msg);

  /* continue pulling */
  if (self->priv->status != OG_BASE_DEVICE_STATUS_ERROR)
    start_interrupt_transfer (self);

out:
  g_object_unref (self);
}

static void
start_interrupt_transfer (OgInsulinx *self)
{
  g_usb_device_interrupt_transfer_async (self->priv->usb_device,
      0x81,
      self->priv->receive_buffer, BUFFER_SIZE,
      0,
      self->priv->cancellable,
      interrupt_transfer_cb,
      g_object_ref (self));
}

static void
og_insulinx_init (OgInsulinx *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_INSULINX, OgInsulinxPrivate);

  g_queue_init (&self->priv->request_queue);
  self->priv->cancellable = g_cancellable_new ();
  self->priv->received = g_string_new (NULL);

  self->priv->records = g_ptr_array_new_with_free_func (
      (GDestroyNotify) og_record_free);
  g_ptr_array_add (self->priv->records, NULL);
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
finalize (GObject *object)
{
  OgInsulinx *self = (OgInsulinx *) object;

  g_object_unref (self->priv->usb_device);
  g_object_unref (self->priv->cancellable);
  g_string_free (self->priv->received, TRUE);
  g_free (self->priv->serial_number);
  g_free (self->priv->sw_version);
  g_clear_pointer (&self->priv->device_clock, g_date_time_unref);
  g_clear_pointer (&self->priv->system_clock, g_date_time_unref);
  g_clear_pointer (&self->priv->records, g_ptr_array_unref);

  G_OBJECT_CLASS (og_insulinx_parent_class)->finalize (object);
}

static void
parse_init_first (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  /* We could be receiving replies from a previous request that made the app
   * crash and restart. Ignore them until we receive what we want. */
  if (code != 0x34)
    return;

  /* FIXME: What's the meaning of this message? In windows logs, msg[0] == 0xc
   * but here I get 0xd. Why? */
  if (msg[0] != 0xd || msg[1] != '\0')
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Prepare: wrong first request message"));
      return;
    }

  request_done (self);
}

static void
parse_init_serial_number (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  if (code != 0x6)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Prepare: wrong code for serial number"));
      return;
    }

  g_assert (self->priv->serial_number == NULL);
  self->priv->serial_number = g_strdup (msg);
  request_done (self);
}

static void
parse_init_sw_version (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  if (code != 0x35)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Prepare: wrong code for sw version"));
      return;
    }

  g_assert (self->priv->sw_version == NULL);
  self->priv->sw_version = g_strdup (msg);
  request_done (self);
}

static void
parse_init_last (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  /* FIXME: What's the meaning of this message? */
  if (code != 0x71 || msg[0] != 0x1 || msg[1] != '\0')
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Prepare: wrong last request message"));
      return;
    }

  request_done (self);
}

static void
parse_date (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  /* Temporaly store those values, we'll create the GDateTime in next request. */
  if (sscanf (msg, "%u,%u,%u", &self->priv->month, &self->priv->day,
          &self->priv->year) != 3)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Error parsing date"));
      return;
    }

  /* 2 digits year, they didn't learn from the Y2K bug? Let's see what happens
   * in 2100... */
  self->priv->year += 2000;
}

static void
parse_time (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  guint hour, minute;

  if (sscanf (msg, "%u,%u", &hour, &minute) != 2)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Error parsing time"));
      return;
    }

  self->priv->system_clock = g_date_time_new_now_local ();

  /* We should have parsed the date in previous request */
  self->priv->device_clock = g_date_time_new_local (
      self->priv->year, self->priv->month, self->priv->day,
      hour, minute, 0);
}

static void
parse_result (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  guint type, month, day, year, hour, minute, glycemia;
  guint ignore;
  gint n_parsed;

  n_parsed = sscanf (msg, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
      &type,
      &ignore, /* Record Number */
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
    return;

  if (n_parsed != 16)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Error parsing result"));
      return;
    }

  /* Fix 2 digits year */
  year += 2000;

  ptr_array_add_null_term (self->priv->records,
      og_record_new (year, month, day, hour, minute, glycemia));
}

static void
parse_ptname (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  gchar **names;

  if (msg == NULL || *msg == '\0')
    {
      DEBUG ("Patient name not set");
      self->priv->first_name = g_strdup ("");
      self->priv->last_name = g_strdup ("");
      return;
    }

  names = g_strsplit (msg, ",", 3);
  if (names == NULL || g_strv_length (names) != 3)
    {
      report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_PARSER,
          "Error parsing patient name"));
      return;
    }

  g_assert (self->priv->first_name == NULL);
  self->priv->first_name = g_strdup (names[0]);

  g_assert (self->priv->last_name == NULL);
  self->priv->last_name = g_strdup (names[1]);

  /* names[2] is the middle initial, ignore */

  g_strfreev (names);
}

static void
prepare_async (OgBaseDevice *base,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgInsulinx *self = (OgInsulinx *) base;
  GError *error = NULL;

  g_return_if_fail (OG_IS_INSULINX (base));

  /* FIXME: We could be nicer and support queueing tasks until device is
   * prepared */
  if (self->priv->status != OG_BASE_DEVICE_STATUS_NONE)
    {
      g_task_report_new_error (self, callback, user_data, prepare_async,
          OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_BUZY,
          "Cannot prepare when status is not NONE");
      return;
    }

  change_status (self, OG_BASE_DEVICE_STATUS_BUZY);

  g_assert (self->priv->task == NULL);
  self->priv->task = g_task_new (self, cancellable, callback, user_data);

  if (!g_usb_device_open (self->priv->usb_device, &error))
    {
      report_error (self, error);
      return;
    }

  if (!g_usb_device_claim_interface (self->priv->usb_device, 0,
          G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
          &error))
    {
      report_error (self, error);
      return;
    }

  if (!g_usb_device_set_configuration (self->priv->usb_device, 1, &error))
    {
      report_error (self, error);
      return;
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
          self->priv->cancellable,
          &error))
    {
      report_error (self, error);
      return;
    }

  /* Start pulling reply buffers, to get them as soon as one is ready */
  start_interrupt_transfer (self);

  /* Start our init sequence */
  queue_request (self, 0x4, "", parse_init_first);
  queue_request (self, 0x5, "", parse_init_serial_number);
  queue_request (self, 0x15, "", parse_init_sw_version);
  queue_request (self, 0x1, "", parse_init_last);
  queue_request (self, 0x60, "$date?\r\n", parse_date);
  queue_request (self, 0x60, "$time?\r\n", parse_time);
  queue_request (self, 0x60, "$result?\r\n", parse_result);
  queue_request (self, 0x60, "$ptname?\r\n", parse_ptname);
}

static gboolean
prepare_finish (OgBaseDevice *base,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (OG_IS_INSULINX (base), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, base), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
parse_nothing (OgInsulinx *self,
    guint8 code,
    const gchar *msg)
{
  report_error (self, g_error_new (OG_BASE_DEVICE_ERROR,
      OG_BASE_DEVICE_ERROR_PARSER,
      "No message was expected"));
}

static void
sync_clock_async (OgBaseDevice *base,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  OgInsulinx *self = (OgInsulinx *) base;
  GDateTime *now;
  gchar *cmd;

  g_return_if_fail (OG_IS_INSULINX (base));

  /* FIXME: We could be nicer and support queueing tasks until device is
   * prepared */
  if (self->priv->status != OG_BASE_DEVICE_STATUS_READY)
    {
      g_task_report_new_error (self, callback, user_data, prepare_async,
          OG_BASE_DEVICE_ERROR,
          OG_BASE_DEVICE_ERROR_BUZY,
          "Cannot sync clock when status is not READY");
      return;
    }

  change_status (self, OG_BASE_DEVICE_STATUS_BUZY);

  g_assert (self->priv->task == NULL);
  self->priv->task = g_task_new (self, cancellable, callback, user_data);

  now = g_date_time_new_now_local ();

  cmd = g_strdup_printf ("$date,%u,%u,%u\r\n",
      g_date_time_get_month (now),
      g_date_time_get_day_of_month (now),
      g_date_time_get_year (now) - 2000);
  queue_request (self, 0x60, cmd, parse_nothing);
  g_free (cmd);

  cmd = g_strdup_printf ("$time,%u,%u\r\n",
      g_date_time_get_hour (now),
      g_date_time_get_minute (now));
  queue_request (self, 0x60, cmd, parse_nothing);
  g_free (cmd);

  g_date_time_unref (now);
}

static gboolean
sync_clock_finish (OgBaseDevice *base,
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

static OgBaseDeviceStatus
get_status (OgBaseDevice *base)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), OG_BASE_DEVICE_STATUS_ERROR);

  return self->priv->status;
}

static const gchar *
get_serial_number (OgBaseDevice *base)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  return self->priv->serial_number;
}

static GDateTime *
get_clock (OgBaseDevice *base,
    GDateTime **system_clock)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  if (system_clock != NULL)
    *system_clock = self->priv->system_clock;

  return self->priv->device_clock;
}

static const OgRecord * const *
get_records (OgBaseDevice *base)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  if (self->priv->records == NULL)
    return NULL;

  return (const OgRecord * const *) self->priv->records->pdata;
}

static const gchar *
get_first_name (OgBaseDevice *base)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  return self->priv->first_name;
}

static const gchar *
get_last_name (OgBaseDevice *base)
{
  OgInsulinx *self = (OgInsulinx *) base;

  g_return_val_if_fail (OG_IS_INSULINX (base), NULL);

  return self->priv->last_name;
}

static void
og_insulinx_class_init (OgInsulinxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  OgBaseDeviceClass *base_class = OG_BASE_DEVICE_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  base_class->get_name = get_name;
  base_class->get_status = get_status;
  base_class->prepare_async = prepare_async;
  base_class->prepare_finish = prepare_finish;
  base_class->sync_clock_async = sync_clock_async;
  base_class->sync_clock_finish = sync_clock_finish;
  base_class->get_serial_number = get_serial_number;
  base_class->get_clock = get_clock;
  base_class->get_records = get_records;
  base_class->get_first_name = get_first_name;
  base_class->get_last_name = get_last_name;

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
