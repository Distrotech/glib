/*  GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2013 Samsung
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Michal Eljasiewicz <m.eljasiewic@samsung.com>
 * Authors: Lukasz Skalski <l.skalski@partner.samsung.com>
 */

#include <fcntl.h>
#include "config.h"

#include <gio/gtask.h>

#include "gkdbusconnection.h"
#include "gunixconnection.h"


/**
 * SECTION:gkdbusconnection
 * @short_description: A kdbus connection
 * @include: gio/gio.h
 * @see_also: #GIOStream, #GKdbusClient
 *
 * #GKdbusConnection is a #GIOStream for a connected kdbus bus.
 */

G_DEFINE_TYPE (GKdbusConnection, g_kdbus_connection, G_TYPE_IO_STREAM);

struct _GKdbusConnectionPrivate
{
  GKdbus        *kdbus;
  gboolean       in_dispose;
};

/*
 * g_kdbus_connection_new:
 *
 */
GKdbusConnection *
g_kdbus_connection_new (void)
{
  return g_object_new(G_TYPE_KDBUS_CONNECTION,NULL);
}

/*
 * g_kdbus_connection_connect:
 *
 */
gboolean
g_kdbus_connection_connect  (GKdbusConnection   *connection,
			                 const gchar        *address,
			                 GCancellable       *cancellable,
			                 GError             **error)
{
  g_return_val_if_fail (G_IS_KDBUS_CONNECTION (connection), FALSE);

  return g_kdbus_open (connection->priv->kdbus,address,cancellable,error);
}

/*
 * g_kdbus_connection_is_connected:
 *
 */
gboolean
g_kdbus_connection_is_connected (GKdbusConnection  *connection)
{
  return (!g_kdbus_is_closed (connection->priv->kdbus));
}

/*
 * g_kdbus_connection_get_property:
 *
 */
static void
g_kdbus_connection_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  //GKdbusConnection *connection = G_KDBUS_CONNECTION (object);
  switch (prop_id)
    {
      default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*
 * g_kdbus_connection_set_property
 *
 */
static void
g_kdbus_connection_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  //GKdbusConnection *connection = G_KDBUS_CONNECTION (object);
  switch (prop_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

// TODO
static void
g_kdbus_connection_dispose (GObject *object)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (object);

  connection->priv->in_dispose = TRUE;

  G_OBJECT_CLASS (g_kdbus_connection_parent_class)
    ->dispose (object);

  connection->priv->in_dispose = FALSE;
}

/*
 * g_kdbus_connection_finalize:
 *
 */
static void
g_kdbus_connection_finalize (GObject *object)
{
  //GKdbusConnection *connection = G_KDBUS_CONNECTION (object);
  G_OBJECT_CLASS (g_kdbus_connection_parent_class)
    ->finalize (object);
}

/*
 * g_kdbus_connection_close
 *
 */
gboolean
g_kdbus_connection_close (GIOStream     *stream,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (stream);

  if (connection->priv->in_dispose)
    return TRUE;

  return g_kdbus_close (connection->priv->kdbus, error);
  return TRUE; 
}

/*
 * g_kdbus_connection_class_init:
 *
 */
static void
g_kdbus_connection_class_init (GKdbusConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GKdbusConnectionPrivate));

  gobject_class->set_property = g_kdbus_connection_set_property;
  gobject_class->get_property = g_kdbus_connection_get_property;
  gobject_class->finalize = g_kdbus_connection_finalize;
  gobject_class->dispose = g_kdbus_connection_dispose;
}

/*
 * g_kdbus_connection_init:
 *
 */
static void
g_kdbus_connection_init (GKdbusConnection *connection)
{
  connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (connection,
                                                  G_TYPE_KDBUS_CONNECTION,
                                                  GKdbusConnectionPrivate);
  connection->priv->kdbus = g_object_new(G_TYPE_KDBUS,NULL);
}

/*
 * g_kdbus_connection_get_kdbus: gets the underlying #GKdbus object of the connection.
 *
 */
GKdbus *
g_kdbus_connection_get_kdbus (GKdbusConnection *connection)
{
  g_return_val_if_fail (G_IS_KDBUS_CONNECTION (connection), NULL);

  return connection->priv->kdbus;
}

