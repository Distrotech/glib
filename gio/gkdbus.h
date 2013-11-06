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

#ifndef __G_KDBUS_H__
#define __G_KDBUS_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>
#include "gdbusprivate.h"

G_BEGIN_DECLS

#define G_TYPE_KDBUS                                       (g_kdbus_get_type ())
#define G_KDBUS(inst)                                      (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_KDBUS, GKdbus))
#define G_KDBUS_CLASS(class)                               (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_KDBUS, GKdbusClass))
#define G_IS_KDBUS(inst)                                   (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_KDBUS))
#define G_IS_KDBUS_CLASS(class)                            (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_KDBUS))
#define G_KDBUS_GET_CLASS(inst)                            (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_KDBUS, GKdbusClass))

typedef struct _GKdbusPrivate                              GKdbusPrivate;
typedef struct _GKdbusClass                                GKdbusClass;

struct _GKdbusClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
  void (*_g_reserved9) (void);
  void (*_g_reserved10) (void);
};

struct _GKdbus
{
  GObject parent_instance;
  GKdbusPrivate *priv;
};

GLIB_AVAILABLE_IN_ALL
GType					g_kdbus_get_type		(void) G_GNUC_CONST;
GLIB_AVAILABLE_IN_ALL
gint					g_kdbus_get_fd			(GKdbus				*kdbus);
GLIB_AVAILABLE_IN_ALL
gboolean				g_kdbus_open 			(GKdbus				*kdbus,
												const gchar			*address,
												GCancellable		*cancellable,
												GError				**error);
GLIB_AVAILABLE_IN_ALL
gboolean				g_kdbus_close			(GKdbus				*kdbus,
												GError				**error);
GLIB_AVAILABLE_IN_ALL
gboolean				g_kdbus_is_closed		(GKdbus				*kdbus);
GLIB_AVAILABLE_IN_ALL
gssize					g_kdbus_receive			(GKdbus				*kdbus,
												char				*data,
												GError				**error);
GLIB_AVAILABLE_IN_ALL
gssize					g_kdbus_send_message	(GDBusWorker		*worker,
												GKdbus				*kdbus,
												GDBusMessage		*dbus_msg,
												gchar				*blob,
												gsize				blob_size,
												GError				**error);
GLIB_AVAILABLE_IN_ALL
gboolean				g_kdbus_register		(GKdbus				*kdbus);
GLIB_AVAILABLE_IN_ALL
GSource *				g_kdbus_create_source 	(GKdbus				*kdbus,
												GIOCondition		condition,
												GCancellable		*cancellable);
GLIB_AVAILABLE_IN_ALL
GIOCondition			g_kdbus_condition_check	(GKdbus				*kdbus,
												GIOCondition		condition);

G_END_DECLS

#endif /* __G_KDBUS_H__ */
