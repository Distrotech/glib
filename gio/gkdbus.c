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

#include "config.h"

#include "gkdbus.h"
#include "glib-unix.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include "gcancellable.h"
#include "gioenumtypes.h"
#include "ginitable.h"
#include "gioerror.h"
#include "gioenums.h"
#include "gioerror.h"
#include "glibintl.h"
#include "kdbus.h"
#include "gdbusmessage.h"
#include "gdbusconnection.h"

#define KDBUS_PART_FOREACH(part, head, first)				\
	for (part = (head)->first;					\
	     (guint8 *)(part) < (guint8 *)(head) + (head)->size;	\
	     part = KDBUS_PART_NEXT(part))
#define RECEIVE_POOL_SIZE (10 * 1024LU * 1024LU)

#define MSG_ITEM_BUILD_VEC(data, datasize)                                    \
	item->type = KDBUS_MSG_PAYLOAD_VEC;					\
        item->size = KDBUS_PART_HEADER_SIZE + sizeof(struct kdbus_vec);		\
        item->vec.address = (unsigned long) data;       			\
        item->vec.size = datasize;

#define KDBUS_ALIGN8(l) (((l) + 7) & ~7)
#define KDBUS_PART_NEXT(part) \
	(typeof(part))(((guint8 *)part) + KDBUS_ALIGN8((part)->size))
#define KDBUS_ITEM_SIZE(s) KDBUS_ALIGN8((s) + KDBUS_PART_HEADER_SIZE)

/*
 * SECTION:gkdbus
 * @short_description: Low-level kdbus object
 * @include: gio/gio.h
 */

static void     g_kdbus_initable_iface_init (GInitableIface  *iface);
static gboolean g_kdbus_initable_init       (GInitable       *initable,
					      GCancellable    *cancellable,
					      GError         **error);

G_DEFINE_TYPE_WITH_CODE (GKdbus, g_kdbus, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_kdbus_initable_iface_init));

enum
{
  PROP_0,
  PROP_FD,
  PROP_TIMEOUT,
  PROP_PEER_ID
};

struct _GKdbusPrivate
{
  gint            fd;
  gchar          *path;
  gchar          *buffer_ptr;
  gchar          *sender;
  gint            peer_id;
  guint64         bloom_size;
  guint           registered : 1;
  guint           closed : 1;
  guint           inited : 1;
  guint           timeout;
  guint           timed_out : 1;
};

/*
 * g_kdbus_get_property:
 * 
 */
static void
g_kdbus_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  GKdbus *kdbus = G_KDBUS (object);

  switch (prop_id)
    {
      case PROP_FD:
        g_value_set_int (value, kdbus->priv->fd);
        break;

      case PROP_TIMEOUT:
        g_value_set_int (value, kdbus->priv->timeout);
        break;

      case PROP_PEER_ID:
        g_value_set_int (value, kdbus->priv->peer_id);
        break;

      default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*
 * g_kdbus_set_property:
 * 
 */
static void
g_kdbus_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GKdbus *kdbus = G_KDBUS (object);

  switch (prop_id)
    {
      case PROP_TIMEOUT:
        kdbus->priv->timeout = g_value_get_int (value);
        break;

      default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*
 * g_kdbus_finalize:
 * TODO: Compare with gsocket
 */
static void
g_kdbus_finalize (GObject *object)
{
  GKdbus *kdbus = G_KDBUS (object);

  if (kdbus->priv->fd != -1 && !kdbus->priv->closed)
    g_kdbus_close (kdbus, NULL);

  if (G_OBJECT_CLASS (g_kdbus_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_kdbus_parent_class)->finalize) (object);

}

/*
 * g_kdbus_class_init:
 *
 */
static void
g_kdbus_class_init (GKdbusClass *klass)
{
  GObjectClass *gobject_class G_GNUC_UNUSED = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GKdbusPrivate));

  gobject_class->finalize = g_kdbus_finalize;
  gobject_class->set_property = g_kdbus_set_property;
  gobject_class->get_property = g_kdbus_get_property;
/*
  g_object_class_install_property (gobject_class, PROP_FD,
                                   g_param_spec_int ("fd",
                                                     P_("File descriptor"),
                                                     P_("The kdbus file descriptor"),
                                                     G_MININT,
                                                     G_MAXINT,
                                                    -1,
                                                     G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
                                   g_param_spec_uint ("timeout",
                                                      P_("Timeout"),
                                                      P_("The timeout in seconds on kdbus I/O"),
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PEER_ID,
                                   g_param_spec_uint ("Peer ID",
                                                      P_("kdbus peer ID"),
                                                      P_("The kdbus peer ID"),
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_STRINGS));
*/
}


/*
 * g_kdbus_initable_iface_init:
 * 
 */
static void
g_kdbus_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_kdbus_initable_init;
}

/*
 * g_kdbus_init:
 * 
 */
static void
g_kdbus_init (GKdbus *kdbus)
{
  kdbus->priv = G_TYPE_INSTANCE_GET_PRIVATE (kdbus, G_TYPE_KDBUS, GKdbusPrivate);

  kdbus->priv->fd = -1;
  kdbus->priv->peer_id = -1;
  kdbus->priv->bloom_size = 0;
  kdbus->priv->path = NULL;
  kdbus->priv->buffer_ptr = NULL;
  kdbus->priv->sender = NULL;
}

/*
 * g_kdbus_initable_init:
 *
 */
static gboolean
g_kdbus_initable_init (GInitable *initable,
			GCancellable *cancellable,
			GError  **error)
{
  GKdbus  *kdbus;

  g_return_val_if_fail (G_IS_KDBUS (initable), FALSE);

  kdbus = G_KDBUS (initable);

  if (cancellable != NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cancellable initialization not supported"));
      return FALSE;
    }

  kdbus->priv->inited = TRUE;

  return TRUE;
}

/*
 * g_kdbus_get_fd: returns the file descriptor of the kdbus
 *
 */

gint
g_kdbus_get_fd (GKdbus *kdbus)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  return kdbus->priv->fd;
}

/*
 * g_kdbus_open:
 * 
 */
gboolean
g_kdbus_open (GKdbus         *kdbus,
    	      const gchar    *address,
              GCancellable   *cancellable,
	          GError         **error)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);
  kdbus->priv->fd = open(address, O_RDWR|O_CLOEXEC|O_NONBLOCK);

  if (kdbus->priv->fd<0)
  {
    g_error(" KDBUS_DEBUG: (%s()): error when mmap: %m, %d",__FUNCTION__,errno);
    return FALSE;
  }

  #ifdef KDBUS_DEBUG
    g_print (" KDBUS_DEBUG: (%s()): kdbus endpoint opened\n",__FUNCTION__);
  #endif

  kdbus->priv->closed = FALSE;
  return TRUE;
}

/*
 * g_kdbus_close:
 * 
 */
gboolean
g_kdbus_close (GKdbus  *kdbus,
	        	GError  **error)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);
  close(kdbus->priv->fd);
 
  kdbus->priv->closed = TRUE;
  kdbus->priv->fd = -1;
  kdbus->priv->registered = FALSE;

  #ifdef KDBUS_DEBUG
    g_print (" KDBUS_DEBUG: (%s()): kdbus endpoint closed\n",__FUNCTION__);
  #endif
  
  return TRUE;
}

/*
 * g_kdbus_is_closed: checks whether a kdbus is closed.
 * 
 */
gboolean
g_kdbus_is_closed (GKdbus *kdbus)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus), FALSE);

  return kdbus->priv->closed;
}

/*
 * g_kdbus_register: hello message + unique name + mapping memory for incoming messages
 * TODO: 
 */
gboolean g_kdbus_register(GKdbus           *kdbus)
{
  struct kdbus_cmd_hello __attribute__ ((__aligned__(8))) hello;

  hello.conn_flags = KDBUS_HELLO_ACCEPT_FD/*    |
                     KDBUS_HELLO_ATTACH_COMM    |
                     KDBUS_HELLO_ATTACH_EXE     |
                     KDBUS_HELLO_ATTACH_CMDLINE |
                     KDBUS_HELLO_ATTACH_CAPS    |
                     KDBUS_HELLO_ATTACH_CGROUP  |
                     KDBUS_HELLO_ATTACH_SECLABEL|
                     KDBUS_HELLO_ATTACH_AUDIT*/;
  hello.size = sizeof(struct kdbus_cmd_hello);
  hello.pool_size = RECEIVE_POOL_SIZE;

  if (ioctl(kdbus->priv->fd, KDBUS_CMD_HELLO, &hello))
    {
      g_error(" KDBUS_DEBUG: (%s()): fd=%d failed to send hello: %m, %d",__FUNCTION__,kdbus->priv->fd,errno);
      return FALSE;
    }

  kdbus->priv->registered = TRUE;
  kdbus->priv->peer_id = hello.id;

  #ifdef KDBUS_DEBUG
    g_print(" KDBUS_DEBUG: (%s()): Our peer ID=%llu\n",__FUNCTION__,hello.id);
  #endif
   
  kdbus->priv->bloom_size = hello.bloom_size;
  kdbus->priv->buffer_ptr = mmap(NULL, RECEIVE_POOL_SIZE, PROT_READ, MAP_SHARED, kdbus->priv->fd, 0);

  if (kdbus->priv->buffer_ptr == MAP_FAILED)
    {
      g_error(" KDBUS_DEBUG: (%s()): error when mmap: %m, %d",__FUNCTION__,errno);
      return FALSE;
    }

  return TRUE;
}

/*
 * g_kdbus_decode_msg: kdbus message received into buffer
 * TODO:
 */
static int 
g_kdbus_decode_msg(GKdbus           *kdbus,
                   struct kdbus_msg *msg, 
                   char             *data)
{
  const struct kdbus_item *item;
  int ret_size = 0;

  KDBUS_PART_FOREACH(item, msg, items)
	{
      if (item->size <= KDBUS_PART_HEADER_SIZE)
	    {
		  g_error(" KDBUS_DEBUG: (%s()): %llu bytes - invalid data record\n",__FUNCTION__,item->size);
		  break; //TODO: continue (because dbus will find error) or break
	    }

		switch (item->type)
		{
		  case KDBUS_MSG_PAYLOAD_OFF:
		    memcpy(data, (char *)kdbus->priv->buffer_ptr + item->vec.offset, item->vec.size);
		    data += item->vec.size;
			ret_size += item->vec.size;			

            #ifdef KDBUS_DEBUG 
			  g_print(" KDBUS_DEBUG: (%s()): KDBUS_MSG_PAYLOAD: %llu bytes, off=%llu, size=%llu\n",__FUNCTION__,item->size,
			    (unsigned long long)item->vec.offset,
				(unsigned long long)item->vec.size);
            #endif
			break;

          case KDBUS_MSG_REPLY_TIMEOUT:

            #ifdef KDBUS_DEBUG 
		      g_print(" KDBUS_DEBUG: (%s()): KDBUS_MSG_REPLY_TIMEOUT: %llu bytes, cookie=%llu\n",__FUNCTION__,item->size, msg->cookie_reply);
            #endif

			/* TODO
              message = generate_local_error_message(msg->cookie_reply, DBUS_ERROR_NO_REPLY, NULL);
			    if(message == NULL)
				  {
				    ret_size = -1;
				    goto out;
				  }

				ret_size = put_message_into_data(message, data);
            */
			break;

		  case KDBUS_MSG_REPLY_DEAD:
		    g_print(" KDBUS_DEBUG: (%s()): KDBUS_MSG_REPLY_DEAD: %llu bytes, cookie=%llu\n",__FUNCTION__,item->size, msg->cookie_reply);

            /* TODO
			  message = generate_local_error_message(msg->cookie_reply, DBUS_ERROR_NAME_HAS_NO_OWNER, NULL);
			  if(message == NULL)
			    {
				  ret_size = -1;
				  goto out;
				}
        
			  ret_size = put_message_into_data(message, data);
            */
			break;
    }
  }

  return ret_size;
}

typedef struct {
  GSource       source;
  GPollFD       pollfd;
  GKdbus       *kdbus;
  GIOCondition  condition;
  GCancellable *cancellable;
  GPollFD       cancel_pollfd;
  gint64        timeout_time;
} GKdbusSource;

/*
 * kdbus_source_prepare:
 * 
 */
static gboolean
kdbus_source_prepare (GSource *source,
		       gint    *timeout)
{
  GKdbusSource *kdbus_source = (GKdbusSource *)source;

  if (g_cancellable_is_cancelled (kdbus_source->cancellable))
    return TRUE;

  if (kdbus_source->timeout_time)
    {
      gint64 now;

      now = g_source_get_time (source);
      /* Round up to ensure that we don't try again too early */
      *timeout = (kdbus_source->timeout_time - now + 999) / 1000;
      if (*timeout < 0)
        {
          kdbus_source->kdbus->priv->timed_out = TRUE;
          *timeout = 0;
          return TRUE;
        }
    }
  else
    *timeout = -1;

  if ((kdbus_source->condition & kdbus_source->pollfd.revents) != 0)
    return TRUE;

  return FALSE;
}

/*
 * kdbus_source_check:
 * 
 */
static gboolean
kdbus_source_check (GSource *source)
{
  int timeout;

  return kdbus_source_prepare (source, &timeout);
}

/*
 * kdbus_source_dispatch
 * 
 */
static gboolean
kdbus_source_dispatch  (GSource     *source,
			            GSourceFunc  callback,
			            gpointer     user_data)
{
  GKdbusSourceFunc func = (GKdbusSourceFunc)callback;
  GKdbusSource *kdbus_source = (GKdbusSource *)source;
  GKdbus *kdbus = kdbus_source->kdbus;
  gboolean ret;

  if (kdbus_source->kdbus->priv->timed_out)
    kdbus_source->pollfd.revents |= kdbus_source->condition & (G_IO_IN | G_IO_OUT);

  ret = (*func) (kdbus,
		 kdbus_source->pollfd.revents & kdbus_source->condition,
		 user_data);

  if (kdbus->priv->timeout)
    kdbus_source->timeout_time = g_get_monotonic_time () +
                                  kdbus->priv->timeout * 1000000;

  else
    kdbus_source->timeout_time = 0;

  return ret;
}

/*
 * kdbus_source_finalize
 * 
 */
static void
kdbus_source_finalize (GSource *source)
{
  GKdbusSource *kdbus_source = (GKdbusSource *)source;
  GKdbus *kdbus;

  kdbus = kdbus_source->kdbus;

  g_object_unref (kdbus);

  if (kdbus_source->cancellable)
    {
      g_cancellable_release_fd (kdbus_source->cancellable);
      g_object_unref (kdbus_source->cancellable);
    }
}

/*
 * kdbus_source_closure_callback:
 * 
 */
static gboolean
kdbus_source_closure_callback (GKdbus         *kdbus,
				                GIOCondition  condition,
				                gpointer      data)
{
  GClosure *closure = data;
  GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&params[0], G_TYPE_KDBUS);
  g_value_set_object (&params[0], kdbus);
  g_value_init (&params[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&params[1], condition);

  g_closure_invoke (closure, &result_value, 2, params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]);
  g_value_unset (&params[1]);

  return result;
}

static GSourceFuncs kdbus_source_funcs =
{
  kdbus_source_prepare,
  kdbus_source_check,
  kdbus_source_dispatch,
  kdbus_source_finalize,
  (GSourceFunc)kdbus_source_closure_callback,
};

/*
 * kdbus_source_new:
 *
 */
static GSource *
kdbus_source_new (GKdbus        *kdbus,
		          GIOCondition  condition,
		          GCancellable  *cancellable)
{
  GSource *source;
  GKdbusSource *kdbus_source;

  condition |= G_IO_HUP | G_IO_ERR;

  source = g_source_new (&kdbus_source_funcs, sizeof (GKdbusSource));
  g_source_set_name (source, "GKdbus");
  kdbus_source = (GKdbusSource *)source;

  kdbus_source->kdbus = g_object_ref (kdbus);
  kdbus_source->condition = condition;

  if (g_cancellable_make_pollfd (cancellable,
                                 &kdbus_source->cancel_pollfd))
    {
      kdbus_source->cancellable = g_object_ref (cancellable);
      g_source_add_poll (source, &kdbus_source->cancel_pollfd);
    }

  kdbus_source->pollfd.fd = kdbus->priv->fd;
  kdbus_source->pollfd.events = condition;
  kdbus_source->pollfd.revents = 0;
  g_source_add_poll (source, &kdbus_source->pollfd);

  if (kdbus->priv->timeout)
    kdbus_source->timeout_time = g_get_monotonic_time () +
                                  kdbus->priv->timeout * 1000000;

  else
    kdbus_source->timeout_time = 0;

  return source;
}

/*
 * g_kdbus_create_source:
 *
 */
GSource *              
g_kdbus_create_source (GKdbus         *kdbus,
					   GIOCondition   condition,
					   GCancellable   *cancellable)
{
  g_return_val_if_fail (G_IS_KDBUS (kdbus) && (cancellable == NULL || G_IS_CANCELLABLE (cancellable)), NULL);

  return kdbus_source_new (kdbus, condition, cancellable);
}

/*
 * g_kdbus_receive:
 * TODO: Handle errors
 */
gssize
g_kdbus_receive (GKdbus       *kdbus,
                 char         *data,
		         GError       **error)
{
  int ret_size = 0;
  guint64 __attribute__ ((__aligned__(8))) offset;
  struct kdbus_msg *msg;

  /* TODO: Temporary hack */
  if (kdbus->priv->closed == TRUE)
    return 1;

  //get memory offset of msg
  again:
    if (ioctl(kdbus->priv->fd, KDBUS_CMD_MSG_RECV, &offset) < 0)
      {
	    if(errno == EINTR)
		  goto again;

        /* TODO: Temporary hack */
        if (errno == EAGAIN)
          return 1;

        g_error (" KDBUS_DEBUG: (%s()): ioctl MSG_RECV failed! %d (%m)\n",__FUNCTION__,errno);
	    return 1;
      }

  msg = (struct kdbus_msg *)((char*)kdbus->priv->buffer_ptr + offset);
  ret_size = g_kdbus_decode_msg(kdbus, msg, (char*)data);

  //release memory occupied by msg
  again_2:
	if (ioctl(kdbus->priv->fd, KDBUS_CMD_MSG_RELEASE, &offset) < 0)
	  {
        if(errno == EINTR)
		  goto again_2;
        g_print (" KDBUS_DEBUG: (%s()): ioctl MSG_RELEASE failed!\n",__FUNCTION__);
		return -1;
	  }
  
  return ret_size;
}

/*
 * g_kdbus_send_reply:
 * TODO: Handle errors,remove unused variables
 */
static gboolean
g_kdbus_send_reply (GDBusWorker     *worker, 
                    GKdbus          *kdbus, 
                    GDBusMessage    *dbus_msg)
{
  GDBusMessage    *reply = NULL;
  char            *unique_name = NULL;
  char            *sender = NULL;

  reply = g_dbus_message_new_method_reply(dbus_msg);
  g_dbus_message_set_sender(reply, "org.freedesktop.DBus");

  unique_name = malloc(30); //TODO: should allow for Kdbus peer ID max value?
  sprintf(unique_name, "%i", kdbus->priv->peer_id);

  sender = malloc (strlen(unique_name) + 4);
  if(!sender)
    {
      //TODO: Handle error
      g_error (" KDBUS_DEBUG: (%s()): sender malloc error\n",__FUNCTION__);
      return -1;
    }
			
  sprintf(sender, ":1.%s", unique_name);
  kdbus->priv->sender = sender;

  #ifdef KDBUS_DEBUG
    g_print ("g_kdbus_send_reply: sender set to:%s! \n", kdbus->priv->sender);
  #endif

  //g_dbus_message_set_body(reply, g_variant_new ("(s)", unique_name));
  g_dbus_message_set_body (reply, g_variant_new ("(s)",sender));
  _g_dbus_worker_queue_or_deliver_received_message (worker, reply);

  g_free (unique_name);
  g_free (sender);
  return TRUE;
}

/*
 * g_kdbus_send_message:
 * TODO: Handle errors
 */
gssize
g_kdbus_send_message (GDBusWorker     *worker,
                      GKdbus          *kdbus,
                      GDBusMessage    *dbus_msg,
                      gchar           *blob,
                      gsize           blob_size,
                      GError          **error)
{
  struct kdbus_msg* kmsg;
  struct kdbus_item *item;
  guint64 kmsg_size = 0;
  const gchar *name;
  guint64 dst_id = KDBUS_DST_ID_BROADCAST;

  if (kdbus->priv->registered == FALSE)
    {
      if (!g_kdbus_register(kdbus))
        {
          g_error (" KDBUS_DEBUG: (%s()): registering failed!\n",__FUNCTION__);
          return -1;
        }

      if (g_strcmp0(g_dbus_message_get_member(dbus_msg), "Hello") == 0)
        {
          #ifdef KDBUS_DEBUG    
            g_print (" KDBUS_DEBUG: (%s()): sending \"Hello\" message!\n",__FUNCTION__);
          #endif

          g_kdbus_send_reply(worker, kdbus, dbus_msg);
          goto out;
        }
    }

  if ((name = g_dbus_message_get_destination(dbus_msg)))
    {
      dst_id = KDBUS_DST_ID_WELL_KNOWN_NAME;
      if ((name[0] == ':') && (name[1] == '1') && (name[2] == '.'))
        {
          dst_id = strtoull(&name[3], NULL, 10);
          name=NULL;
        }
    }

  kmsg_size = sizeof(struct kdbus_msg);
  kmsg_size += KDBUS_ITEM_SIZE(sizeof(struct kdbus_vec)); //vector for blob

  if (name)
  	kmsg_size += KDBUS_ITEM_SIZE(strlen(name) + 1);
  else if (dst_id == KDBUS_DST_ID_BROADCAST)
  	kmsg_size += KDBUS_PART_HEADER_SIZE + kdbus->priv->bloom_size;

  kmsg = malloc(kmsg_size);
  if (!kmsg)
    {
      //TODO: Handle error
      g_error (" KDBUS_DEBUG: (%s()): kmsg malloc error\n",__FUNCTION__);
      return -1;
    }

  memset(kmsg, 0, kmsg_size);
  kmsg->size = kmsg_size;
  kmsg->payload_type = KDBUS_PAYLOAD_DBUS1;
  kmsg->dst_id = name ? 0 : dst_id;
  kmsg->src_id = kdbus->priv->peer_id;
  kmsg->cookie = g_dbus_message_get_serial(dbus_msg);

  #ifdef KDBUS_DEBUG
    g_print (" KDBUS_DEBUG: (%s()): destination name: %s\n",__FUNCTION__,name);
    g_print (" KDBUS_DEBUG: (%s()): blob size: %d\n",__FUNCTION__,(gint)blob_size);
    g_print (" KDBUS_DEBUG: (%s()): serial: %i\n",__FUNCTION__,kmsg->cookie);
    g_print (" KDBUS_DEBUG: (%s()): src_id/peer_id: %i\n",__FUNCTION__,kdbus->priv->peer_id);
  #endif

  //build message contents
  item = kmsg->items;
  MSG_ITEM_BUILD_VEC(blob, blob_size);

  if (name)
	{
	  item = KDBUS_PART_NEXT(item);
	  item->type = KDBUS_MSG_DST_NAME;
	  item->size = KDBUS_PART_HEADER_SIZE + strlen(name) + 1;
	  strcpy(item->str, name);
	}
  else if (dst_id == KDBUS_DST_ID_BROADCAST)
	{
	  item = KDBUS_PART_NEXT(item);
	  item->type = KDBUS_MSG_BLOOM;
	  item->size = KDBUS_PART_HEADER_SIZE + kdbus->priv->bloom_size;
	  strncpy(item->data,g_dbus_message_get_interface(dbus_msg),kdbus->priv->bloom_size);
	}

again:
	if (ioctl(kdbus->priv->fd, KDBUS_CMD_MSG_SEND, kmsg))
	  {
		if(errno == EINTR)
		  goto again;
    else
      g_error (" KDBUS_DEBUG: (%s()): ioctl error sending kdbus message:%d (%m)\n",__FUNCTION__,errno);
    }

    #ifdef KDBUS_DEBUG 
      g_print (" KDBUS_DEBUG: (%s()): ioctl(CMD_MSG_SEND) sent successfully\n",__FUNCTION__);
    #endif

    free(kmsg);

out:
  return blob_size;
}

