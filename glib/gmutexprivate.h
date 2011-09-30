/* temporary */

#ifndef __G_MUTEXPRIVATE_H__
#define __G_MUTEXPRIVATE_H__

#include "gmutex.h"

struct _GPrivate
{
  gpointer single_value;
  gboolean ready;
#ifdef G_OS_WIN32
  gint index;
#else
  pthread_key_t key;
#endif
};

G_GNUC_INTERNAL
void            g_private_init          (GPrivate       *key,
                                         GDestroyNotify  notify);

#endif /* __G_MUTEXPRIVATE_H__ */
