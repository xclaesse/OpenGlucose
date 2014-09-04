#include "config.h"

#include "main-window.h"

#include <glib/gi18n.h>

#include "device-widget.h"

G_DEFINE_TYPE (OgMainWindow, og_main_window, GTK_TYPE_APPLICATION_WINDOW)

struct _OgMainWindowPrivate
{
  /* Owned OgBaseDevice -> borrowed GtkWidget */
  GHashTable *devices;
  GtkWidget *stack;
};

static GtkWidget *
no_device_page_new (OgMainWindow *self)
{
  GtkStyleContext *style;
  GtkWidget *label;

  label = gtk_label_new (_("No device connected"));
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);

  style = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (style, "og-no-device-label");

  return label;
}

static void
og_main_window_init (OgMainWindow *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      OG_TYPE_MAIN_WINDOW, OgMainWindowPrivate);
  self->priv->devices = g_hash_table_new_full (NULL, NULL,
      g_object_unref, (GDestroyNotify) gtk_widget_destroy);
}

static void
constructed (GObject *object)
{
  OgMainWindow *self = (OgMainWindow *) object;
  GtkWidget *box;
  GtkWidget *stack_switcher;
  GtkWidget *w;

  G_OBJECT_CLASS (og_main_window_parent_class)->constructed (object);

  /* FIXME: We should remember last time's window geometry */
  gtk_window_maximize (GTK_WINDOW (self));
  gtk_window_resize (GTK_WINDOW (self), 640, 480);
  gtk_window_set_title (GTK_WINDOW (self), "OpenGlucose");
  gtk_container_set_border_width (GTK_CONTAINER (self), 6);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (self), box);
  gtk_widget_show (box);

  stack_switcher = gtk_stack_switcher_new ();
  gtk_box_pack_start (GTK_BOX (box), stack_switcher, FALSE, FALSE, 0);
  gtk_widget_show (stack_switcher);

  self->priv->stack = gtk_stack_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (stack_switcher),
      GTK_STACK (self->priv->stack));
  gtk_box_pack_start (GTK_BOX (box), self->priv->stack, TRUE, TRUE, 0);
  gtk_widget_show (self->priv->stack);

  w = no_device_page_new (self);
  gtk_stack_add_named (GTK_STACK (self->priv->stack), w, "no-device");
  gtk_widget_show (w);
}

static void
dispose (GObject *object)
{
  OgMainWindow *self = (OgMainWindow *) object;

  g_clear_pointer (&self->priv->devices, g_hash_table_unref);

  G_OBJECT_CLASS (og_main_window_parent_class)->dispose (object);
}

static void
og_main_window_class_init (OgMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

  g_type_class_add_private (object_class, sizeof (OgMainWindowPrivate));
}

GtkWidget *
og_main_window_new (GtkApplication *application)
{
  return g_object_new (OG_TYPE_MAIN_WINDOW,
      "application", application,
      NULL);
}

void
og_main_window_add_device (OgMainWindow *self,
    OgBaseDevice *device)
{
  GtkWidget *w;

  g_return_if_fail (OG_IS_MAIN_WINDOW (self));
  g_return_if_fail (OG_IS_BASE_DEVICE (device));
  g_return_if_fail (!g_hash_table_contains (self->priv->devices, device));

  w = og_device_widget_new (device);
  gtk_stack_add_titled (GTK_STACK (self->priv->stack), w,
      og_base_device_get_name (device),
      og_base_device_get_name (device));
  gtk_widget_show (w);

  g_hash_table_insert (self->priv->devices, g_object_ref (device), w);

  if (g_hash_table_size (self->priv->devices) == 1)
    gtk_stack_set_visible_child (GTK_STACK (self->priv->stack), w);
}

void
og_main_window_remove_device (OgMainWindow *self,
    OgBaseDevice *device)
{
  g_return_if_fail (OG_IS_MAIN_WINDOW (self));
  g_return_if_fail (OG_IS_BASE_DEVICE (device));
  g_return_if_fail (g_hash_table_contains (self->priv->devices, device));

  g_hash_table_remove (self->priv->devices, device);
}
