/* gproperty.c: Property definitions for GObject
 *
 * Copyright © 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 */

/**
 * SECTION:gproperty
 * @Title: GProperty
 * @Short_Desc: Property definitions for GObject
 *
 * #GProperty is a type of #GParamSpec for defining properties for #GObject.
 *
 * The main difference between #GProperty and #GParamSpec is that #GProperty
 * enforces a specific set of best practices for accessing values exposed
 * as #GObject properties.
 *
 * #GProperty uses direct access to the fields in the instance private data
 * structure whenever possible, and allows specifying accessor functions in
 * cases where it is necessary. For direct access to the private structure
 * members, #GProperty will assume that the instance structure contains a
 * pointer to the private data structure as its last member, e.g.:
 *
 * |[
 *   typedef struct _TestObject         TestObject;
 *   typedef struct _TestObjectPrivate  TestObjectPrivate;
 *
 *   struct _TestObject
 *   {
 *     GObject parent_instance;
 *
 *     TestObjectPrivate *priv;
 *   };
 * ]|
 *
 * #GProperty is strongly typed, both at compilation time and at run time.
 *
 * Unlike #GParamSpec, there is a single public #GProperty class with various
 * constructors; all public #GProperty methods operate depending on the type
 * handled by the #GProperty.
 *
 * The #GParamSpec methods can be used with #GProperty transparently, to
 * allow backward compatibility with existing code that introspects #GObject
 * properties through the #GParamSpec API.
 *
 * <refsect2>
 *   <title>Using GProperty</title>
 *   <para>A typical example of #GProperty usage is:</para>
 *
 * |[
 * /&ast; the private data structure &ast;/
 * struct _TestObjectPrivate
 * {
 *   int x;
 *   int y;
 *   int width;
 *   int height;
 * };
 *
 * /&ast; identifiers for each property in the array &ast;/
 * enum { PROP_0, PROP_X, PROP_Y, PROP_WIDTH, PROP_HEIGHT, LAST_PROP };
 *
 * /&ast; an array of properties &ast;/
 * static GParamSpec *test_object_properties[LAST_PROP] = { 0, };
 *
 * static void
 * test_object_class_init (TestObjectClass *klass)
 * {
 *   g_type_class_add_private (klass, sizeof (TestObjectPrivate));
 *
 *   test_object_properties[PROP_X] =
 *     g_int_property_new ("x", G_PROPERTY_READWRITE,
 *                         G_STRUCT_OFFSET (TestObjectPrivate, x),
 *                         NULL,
 *                         NULL);
 *
 *   test_object_properties[PROP_Y] =
 *     g_int_property_new ("y", G_PROPERTY_READWRITE,
 *                         G_STRUCT_OFFSET (TestObjectPrivate, y),
 *                         NULL,
 *                         NULL);
 *
 *   test_object_properties[PROP_WIDTH] =
 *     g_int_property_new ("width", G_PROPERTY_READWRITE,
 *                         G_STRUCT_OFFSET (TestObjectPrivate, width),
 *                         NULL,
 *                         NULL);
 *
 *   test_object_properties[PROP_HEIGHT] =
 *     g_int_property_new ("height", G_PROPERTY_READWRITE,
 *                         G_STRUCT_OFFSET (TestObjectPrivate, height),
 *                         NULL,
 *                         NULL);
 *
 *   g_object_class_install_properties (G_OBJECT_CLASS (klass),
 *                                      G_N_ELEMENTS (test_object_properties),
 *                                      test_object_properties);
 * }
 * ]|
 *   <para>The main differences with the #GParamSpec creation and installation
 *   code are:</para>
 *
 *   <itemizedlist>
 *     <listitem><para>the constructors take the same parameters</para></listitem>
 *     <listitem><para>there are not #GObject set_property and get_property
 *     virtual function assignments</para></listitem>
 *     <listitem><para>all properties use direct access of the member in the
 *     instance private data structure</para></listitem>
 *   </itemizedlist>
 *
 *   <refsect3>
 *     <title>Setting and getting values</title>
 *     <para>Writing accessors for properties defined using #GProperties is
 *     a simple case of calling g_property_set() or g_property_get(), for
 *     instance the code below is the simplest form of setter and getter
 *     pair for the "x" property as defined above:</para>
 * |[
 *   void
 *   test_object_set_x (TestObject *self,
 *                      int         x)
 *   {
 *     g_return_if_fail (TEST_IS_OBJECT (self));
 *
 *     g_property_set (G_PROPERTY (test_object_properties[PROP_X]), self, x);
 *   }
 *
 *   int
 *   test_object_get_x (TestObject *self)
 *   {
 *     int retval;
 *
 *     g_return_val_if_fail (TEST_IS_OBJECT (self), 0);
 *
 *     g_property_get (G_PROPERTY (test_object_properties[PROP_X]),
 *                     self,
 *                     &amp;retval);
 *
 *     return retval;
 *   }
 * ]|
 *     <para>Note that calling g_property_set() for a property holding a
 *     complex type (e.g. #GObject or #GBoxed) without a specific setter
 *     function will, by default, result in the pointer to the new value
 *     being copied in the private data structure's field; if you need to
 *     copy a boxed type, or take a reference on an object type, you will
 *     need to set the %G_PROPERTY_COPY_SET flag when creating the
 *     property.</para>
 *
 *     <para>Calling g_property_get() will return a pointer to the private
 *     data structure's field, unless %G_PROPERTY_COPY_GET is set when
 *     creating the property, in which case the returned value will either
 *     be a copy of the private data structure field if it is a boxed type
 *     or the instance with its reference count increased if it is an object
 *     type.</para>
 *   </refsect3>
 *
 *   <refsect3>
 *     <title>Ranges and validation</title>
 *     <para>For different property types it is possible to set a range of
 *     valid values; the setter function can then use g_property_validate()
 *     to check whether the new value is valid.</para>
 *
 *     <para>The range is set using g_property_set_range():</para>
 *
 * |[
 *     test_object_properties[PROP_WIDTH] =
 *       g_int_property_new ("width", G_PROPERTY_READWRITE,
 *                           G_STRUCT_OFFSET (TestObjectPrivate, width),
 *                           NULL, NULL);
 *     g_property_set_range (G_PROPERTY (test_object_properties[PROP_WIDTH]),
 *                           0.0,     /&ast; minimum value &ast;/
 *                           G_MAXINT /&ast; maximum value &ast;/)
 *
 *     test_object_properties[PROP_HEIGHT] =
 *       g_int_property_new ("height", G_PROPERTY_READWRITE,
 *                           G_STRUCT_OFFSET (TestObjectPrivate, height),
 *                           NULL, NULL);
 *     g_property_set_range (G_PROPERTY (test_object_properties[PROP_HEIGHT]),
 *                           0.0,     /&ast; minimum value &ast;/
 *                           G_MAXINT /&ast; maximum value &ast;/)
 * ]|
 *
 *     <para>The example above keeps the "width" and "height" properties as
 *     integers, but limits the range of valid values to [ 0, %G_MAXINT ].</para>
 *
 *     <para>Validation is automatically performed when setting a property
 *     through g_property_set(); explicit setter methods can use g_property_validate()
 *     to perform the validation, e.g.:</para>
 *
 * |[
 *   void
 *   test_object_set_width (TestObject *self,
 *                          int         width)
 *   {
 *     GProperty *property;
 *
 *     property = G_PROPERTY (test_object_properties[PROP_WIDTH]);
 *
 *     g_return_if_fail (!g_property_validate (property, width));
 *
 *     /&ast; we deliberately do not use g_property_set() here because
 *      &ast; g_property_set() will implicitly call g_property_validate()
 *      &ast; prior to setting the property
 *      &ast;/
 *     if (self->priv->width == width)
 *       return;
 *
 *     self->priv->width = width;
 *
 *     g_object_notify_by_pspec (G_OBJECT (self), G_PARAM_SPEC (property));
 *
 *     test_object_queue_foo (self);
 *   }
 * ]|
 *   </refsect3>
 *
 *   <refsect3>
 *     <title>Custom accessors</title>
 *     <para>For cases where the direct access to a structure field does not
 *     match the semantics of the property it is possible to pass a setter
 *     and a getter function when creating a #GProperty:</para>
 *
 * |[
 *     test_object_property[PROP_COMPLEX] =
 *       g_object_property_new ("complex", G_PROPERTY_READWRITE, -1,
 *                              test_object_set_complex,
 *                              test_object_get_complex);
 *     g_property_set_prerequisite (G_PROPERTY (test_object_property[PROP_COMPLEX]),
 *                                  TEST_TYPE_COMPLEX);
 * ]|
 *
 *     <para>The accessors can be public or private functions. The implementation
 *     of an explicit setter will be called under the #GProperty lock if the
 *     property is declared using the %G_PROPERTY_ATOMIC flag; the setter should
 *     not notify the property on changes, and should return %TRUE if the value
 *     was modified. An example of a setter is:</para>
 *
 * |[
 *   static gboolean
 *   test_object_set_complex (gpointer self_,
 *                            gpointer value_)
 *   {
 *     TestObject *self = self_;
 *     TestComplex *value = value_;
 *
 *     if (self->priv->complex == value)
 *       return FALSE;
 *
 *     if (self->priv->complex != NULL)
 *       {
 *         test_complex_set_back_pointer (self->priv->complex, NULL);
 *         g_object_unref (self->priv->complex);
 *       }
 *
 *     self->priv->complex = value;
 *
 *     if (self->priv->complex != NULL)
 *       {
 *         g_object_ref (self->priv->complex);
 *         test_complex_set_back_pointer (self->priv->complex, self);
 *       }
 *
 *     test_object_queue_foo (self);
 *
 *     return TRUE;
 *   }
 * ]|
 *
 *     <para>It is also possible to still pass the offset of the structure
 *     field, and provide either the setter or the getter function:</para>
 *
 * |[
 *     test_object_property[PROP_WIDTH] =
 *       g_int_property_new ("width", G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
 *                           G_STRUCT_OFFSET (TestObjectPrivate, width),
 *                           test_object_set_width, /&ast; explicit setter &ast;/
 *                           NULL                   /&ast; implicit getter &ast;/);
 * ]|
 *
 *     <para>Calling g_property_set() using the "width" property in the example
 *     above will result in calling test_object_set_width(); calling, instead,
 *     g_property_get() using the "width" property will result in accessing
 *     the width structure member.</para>
 *
 *     <warning><para>You must not call g_property_set() inside the implementation
 *     of test_object_set_width(), in order to avoid loops.</para></warning>
 *   </refsect3>
 *
 *   <refsect3>
 *     <title>Special flags</title>
 *     <para>#GProperty has its own set of flags to be passed to its
 *     constructor functions. Alongside the usual %G_PROPERTY_READABLE
 *     and %G_PROPERTY_WRITABLE, which control the access policy for
 *     setter and getter functions, there are the following flags:</para>
 *
 *     <itemizedlist>
 *       <listitem><para>%G_PROPERTY_DEPRECATED, a flag for marking
 *       deprecated properties. If the G_ENABLE_DIAGNOSTIC environment
 *       variable is set, calling g_property_set() and g_property_get()
 *       on the deprecated properties will result in a run time warning
 *       printed on the console.</para></listitem>
 *       <listitem><para>%G_PROPERTY_ATOMIC, a flag for marking access
 *       to properties a threadsafe operation. It is possible to change
 *       the locking mechanism by using g_property_set_lock_functions()
 *       and replacing both the lock and the unlock function, which can
 *       then be called by g_property_lock() and g_property_unlock()
 *       when needed.</para></listitem>
 *     </itemizedlist>
 *   </refsect3>
 *
 * </refsect2>
 */

#include "config.h"

#include <string.h>

#include "gproperty.h"

#include "gvaluecollector.h"
#include "gparam.h"
#include "gtype.h"
#include "gvalue.h"
#include "gvaluetypes.h"

struct _GProperty
{
  GParamSpec parent_instance;

  guint flags        : 15;
  guint is_installed : 1;

  guint16 type_size;
  guint16 field_offset;

  GQuark prop_id;

  GPropertyLockFunc lock_func;
  GPropertyUnlockFunc unlock_func;
};

/* defines for the integer sub-types we don't have either in GValue
 * or in the type system; we rely on GValue validation for run-time
 * checks
 */
#define g_value_get_int8        g_value_get_int
#define g_value_get_int16       g_value_get_int
#define g_value_get_int32       g_value_get_int

#define g_value_get_uint8       g_value_get_uint
#define g_value_get_uint16      g_value_get_uint
#define g_value_get_uint32      g_value_get_uint

static GParamFlags
property_flags_to_param_flags (GPropertyFlags flags)
{
  GParamFlags retval = 0;

  if (flags & G_PROPERTY_READABLE)
    retval |= G_PARAM_READABLE;

  if (flags & G_PROPERTY_WRITABLE)
    retval |= G_PARAM_WRITABLE;

  if (flags & G_PROPERTY_DEPRECATED)
    retval |= G_PARAM_DEPRECATED;

  if (flags & G_PROPERTY_CONSTRUCT_ONLY)
    retval |= (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE);

  return retval;
}

static GProperty *
g_property_create (GType           pspec_type,
                   GType           value_type,
                   const char     *name,
                   GPropertyFlags  flags,
                   gssize          field_offset,
                   gsize           value_size)
{
  GProperty *prop;

  prop = g_param_spec_internal (pspec_type,
                                name, NULL, NULL,
                                property_flags_to_param_flags (flags));

  G_PARAM_SPEC (prop)->value_type = value_type;

  prop->flags = flags;
  prop->field_offset = field_offset;
  prop->type_size = value_size;
  prop->is_installed = FALSE;

  return prop;
}

#define DEFINE_PROPERTY_INTEGER(G_t, g_t, c_t, G_T, minVal, maxVal) \
typedef struct { \
  GProperty parent; \
\
  c_t min_value; \
  c_t max_value; \
\
  GProperty##G_t##Set setter; \
  GProperty##G_t##Get getter; \
} G##G_t##Property; \
\
static gint \
property_##g_t##_values_cmp (GParamSpec   *pspec, \
                             const GValue *value_a, \
                             const GValue *value_b) \
{ \
  c_t val_a = g_value_get_##g_t (value_a); \
  c_t val_b = g_value_get_##g_t (value_b); \
\
  if (val_a < val_b) \
    return -1; \
\
  if (val_a > val_b) \
    return 1; \
\
  return 0; \
} \
\
static gboolean \
property_##g_t##_validate (GParamSpec *pspec, \
                           GValue     *value) \
{ \
  G##G_t##Property *internal = (G##G_t##Property *) pspec; \
  c_t oval = g_value_get_##g_t (value); \
  c_t nval = oval; \
\
  nval = CLAMP (nval, internal->min_value, internal->max_value); \
\
  return nval != oval; \
} \
\
static void \
property_##g_t##_class_init (GParamSpecClass *klass) \
{ \
  klass->value_type = G_T; \
\
  klass->value_validate = property_##g_t##_validate; \
  klass->values_cmp = property_##g_t##_values_cmp; \
} \
\
static void \
property_##g_t##_init (GParamSpec *pspec) \
{ \
  G##G_t##Property *property = (G##G_t##Property *) pspec; \
\
  property->min_value = minVal; \
  property->max_value = maxVal; \
} \
\
GType _g_##g_t##_property_get_type (void); /* forward declaration for -Wmissing-prototypes */ \
\
GType \
_g_##g_t##_property_get_type (void) \
{ \
  static volatile gsize pspec_type_id__volatile = 0; \
\
  if (g_once_init_enter (&pspec_type_id__volatile)) \
    { \
      const GTypeInfo info = { \
        sizeof (GParamSpecClass), \
        NULL, NULL, \
        (GClassInitFunc) property_##g_t##_class_init, \
        NULL, NULL, \
        sizeof (G##G_t##Property), \
        0, \
        (GInstanceInitFunc) property_##g_t##_init, \
      }; \
\
      GType pspec_type_id = \
        g_type_register_static (G_TYPE_PROPERTY, \
                                g_intern_static_string ("G" #G_t "Property"), \
                                &info, 0); \
\
      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id); \
    } \
\
  return pspec_type_id__volatile; \
} \
\
GParamSpec * \
g_##g_t##_property_new (const gchar         *name,      \
                        GPropertyFlags       flags,     \
                        gssize               offset,    \
                        GProperty##G_t##Set  setter,    \
                        GProperty##G_t##Get  getter)    \
{ \
  GProperty *prop; \
  G##G_t##Property *internal; \
\
  g_return_val_if_fail (name != NULL, NULL); \
\
  if (setter == NULL && getter == NULL) \
    g_return_val_if_fail (offset >= 0, NULL); \
\
  prop = g_property_create (_g_##g_t##_property_get_type (), \
                            G_T, \
                            name, \
                            flags, \
                            offset, \
                            sizeof (c_t)); \
\
  internal = (G##G_t##Property *) prop; \
  internal->setter = setter; \
  internal->getter = getter; \
\
  return G_PARAM_SPEC (prop); \
} \
\
static inline void \
g_##g_t##_property_set_range (GProperty *property, \
                              c_t        min_value, \
                              c_t        max_value) \
{ \
  if (min_value > max_value) \
    { \
      g_critical (G_STRLOC ": Invalid range for " #g_t " property '%s'", \
                  G_PARAM_SPEC (property)->name); \
      return; \
    } \
\
  ((G##G_t##Property *) property)->min_value = min_value; \
  ((G##G_t##Property *) property)->max_value = max_value; \
} \
\
static inline void \
g_##g_t##_property_get_range (GProperty *property, \
                              c_t       *min_value, \
                              c_t       *max_value) \
{ \
  *min_value = ((G##G_t##Property *) property)->min_value; \
  *max_value = ((G##G_t##Property *) property)->max_value; \
} \
\
static inline gboolean \
g_##g_t##_property_validate (GProperty *property, \
                             c_t        value) \
{ \
  G##G_t##Property *internal = (G##G_t##Property *) property; \
\
  if (value >= internal->min_value && \
      value <= internal->max_value) \
    return TRUE; \
\
  return FALSE; \
} \
\
static inline gboolean \
g_##g_t##_property_set_value (GProperty *property, \
                              gpointer   gobject, \
                              c_t        value) \
{ \
  gboolean retval; \
\
  if ((property->flags & G_PROPERTY_WRITABLE) == 0) \
    { \
      g_critical ("The property '%s' of object '%s' is not writable", \
                  G_PARAM_SPEC (property)->name, \
                  G_OBJECT_TYPE_NAME (gobject)); \
      return FALSE; \
    } \
\
  if (!g_##g_t##_property_validate (property, value)) \
    { \
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range", \
                 G_PARAM_SPEC (property)->name, \
                 G_OBJECT_TYPE_NAME (gobject)); \
      return FALSE; \
    } \
\
  if (((G##G_t##Property *) property)->setter != NULL) \
    { \
      property_lock_internal (property, gobject); \
\
      retval = ((G##G_t##Property *) property)->setter (gobject, value); \
\
      property_unlock_internal (property, gobject); \
\
      if (retval) \
        g_object_notify_by_pspec (gobject, (GParamSpec *) property); \
    } \
  else if (property->field_offset >= 0) \
    { \
      gpointer priv_p, field_p; \
\
      property_lock_internal (property, gobject); \
\
      priv_p = get_private_pointer (gobject); \
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset); \
\
      if ((* (c_t *) field_p) == value) \
        { \
          property_unlock_internal (property, gobject); \
          return FALSE; \
        } \
\
      (* (c_t *) field_p) = value; \
\
      property_unlock_internal (property, gobject); \
\
      g_object_notify_by_pspec (gobject, (GParamSpec *) property); \
\
      retval = TRUE; \
    } \
  else \
    { \
      g_critical (G_STRLOC ": No setter function or field offset specified " \
                  "for property '%s'", \
                  G_PARAM_SPEC (property)->name); \
\
      retval = FALSE; \
    } \
\
  return retval; \
} \
\
static inline c_t \
g_##g_t##_property_get_value (GProperty *property, \
                              gpointer   gobject) \
{ \
  if ((property->flags & G_PROPERTY_READABLE) == 0) \
    { \
      g_critical ("The property '%s' of object '%s' is not readable", \
                  G_PARAM_SPEC (property)->name, \
                  G_OBJECT_TYPE_NAME (gobject)); \
      return FALSE; \
    } \
\
  if (((G##G_t##Property *) property)->getter != NULL) \
    { \
      return ((G##G_t##Property *) property)->getter (gobject); \
    } \
  else if (property->field_offset >= 0) \
    { \
      gpointer priv_p, field_p; \
\
      priv_p = get_private_pointer (gobject); \
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset); \
\
      return (* (c_t *) field_p); \
    } \
  else \
    { \
      g_critical (G_STRLOC ": No setter function or field offset specified " \
                  "for property '%s'", \
                  G_PARAM_SPEC (property)->name); \
      return (c_t) 0; \
    } \
}

static void
g_property_default_lock (GProperty *property,
                         gpointer   gobject)
{
  gpointer bit_lock_p;

  bit_lock_p = g_object_get_qdata (gobject, property->prop_id);
  if (bit_lock_p == NULL)
    {
      bit_lock_p = g_new0 (gint, 1);
      g_object_set_qdata_full (gobject, property->prop_id, bit_lock_p, g_free);
    }

  g_bit_lock (bit_lock_p, 0);
}

static void
g_property_default_unlock (GProperty *property,
                           gpointer   gobject)
{
  gpointer bit_lock_p;

  bit_lock_p = g_object_get_qdata (gobject, property->prop_id);
  if (bit_lock_p == NULL)
    return;

  g_bit_unlock (bit_lock_p, 0);
  g_object_set_qdata (gobject, property->prop_id, NULL);
}

static inline void
property_lock_internal (GProperty *property,
                        gpointer   gobject)
{
  if (G_LIKELY ((property->flags & G_PROPERTY_ATOMIC) == 0))
    return;

  if (property->lock_func != NULL)
    property->lock_func (property, gobject);
  else
    g_property_default_lock (property, gobject);
}

static inline void
property_unlock_internal (GProperty *property,
                          gpointer   gobject)
{
  if (G_LIKELY ((property->flags & G_PROPERTY_ATOMIC) == 0))
    return;

  if (property->unlock_func != NULL)
    property->unlock_func (property, gobject);
  else
    g_property_default_unlock (property, gobject);
}

static inline gpointer
get_private_pointer (gpointer instance)
{
  return g_type_instance_get_private (instance, G_OBJECT_TYPE (instance));
}

/**
 * g_boolean_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a boolean value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Boolean, boolean, gboolean, G_TYPE_BOOLEAN, FALSE, TRUE)

/**
 * g_int_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an integer value.
 *
 * The default range of valid values is [ %G_MININT, %G_MAXINT ].
 *
 * If you require a specific integer size, use g_int8_property_new(),
 * g_int16_property_new(), g_int32_property_new() or g_int64_property_new().
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Int, int, int, G_TYPE_INT, G_MININT, G_MAXINT)

/**
 * g_int8_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an 8 bits integer value.
 *
 * The default range of valid values is [ %G_MININT8, %G_MAXINT8 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Int8, int8, gint8, G_TYPE_INT, G_MININT8, G_MAXINT8)

/**
 * g_int16_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to a 16 bits integer value.
 *
 * The default range of valid values is [ %G_MININT16, %G_MAXINT16 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Int16, int16, gint16, G_TYPE_INT, G_MININT16, G_MAXINT16)

/**
 * g_int32_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to a 32 bits integer value.
 *
 * The default range of valid values is [ %G_MININT32, %G_MAXINT32 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Int32, int32, gint32, G_TYPE_INT, G_MININT32, G_MAXINT32)

/**
 * g_int64_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to a 64 bits integer value.
 *
 * The default range of valid values is [ %G_MININT64, %G_MAXINT64 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Int64, int64, gint64, G_TYPE_INT64, G_MININT64, G_MAXINT64)

/**
 * g_long_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to a long integer value.
 *
 * The default range of valid values is [ %G_MINLONG, %G_MAXLONG ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (Long, long, glong, G_TYPE_LONG, G_MINLONG, G_MAXLONG)

/**
 * g_uint_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned integer value.
 *
 * The default range of valid values is [ 0, %G_MAXUINT ].
 *
 * If you require a specific integer size, use g_uint8_property_new(),
 * g_uint16_property_new(), g_uint32_property_new() or g_uint64_property_new().
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (UInt, uint, guint, G_TYPE_UINT, 0, G_MAXUINT)

/**
 * g_uint8_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned 8 bits integer value.
 *
 * The default range of valid values is [ 0, %G_MAXUINT8 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (UInt8, uint8, guint8, G_TYPE_UINT, 0, G_MAXUINT8)

/**
 * g_uint16_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned 16 bits integer value.
 *
 * The default range of valid values is [ 0, %G_MAXUINT16 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (UInt16, uint16, guint16, G_TYPE_UINT, 0, G_MAXUINT16)

/**
 * g_uint32_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned 32 bits integer value.
 *
 * The default range of valid values is [ 0, %G_MAXUINT32 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (UInt32, uint32, guint32, G_TYPE_UINT, 0, G_MAXUINT32)

/**
 * g_uint64_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned 64 bits integer value.
 *
 * The default range of valid values is [ 0, %G_MAXUINT64 ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (UInt64, uint64, guint64, G_TYPE_UINT64, 0, G_MAXUINT64)

/**
 * g_ulong_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property, or %NULL
 * @getter: (allow-none): the getter function for the property, or %NULL
 *
 * Creates a new #GProperty mapping to an unsigned long integer value.
 *
 * The default range of valid values is [ 0, %G_MAXULONG ].
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
DEFINE_PROPERTY_INTEGER (ULong, ulong, gulong, G_TYPE_ULONG, 0, G_MAXULONG)

/*
 * GEnum
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_enum_property_get_type (void);

typedef struct {
  GProperty parent;

  GEnumClass *e_class;

  GPropertyEnumSet setter;
  GPropertyEnumGet getter;
} GEnumProperty;

static gboolean
property_enum_validate (GParamSpec *pspec,
                        GValue     *value)
{
  GEnumProperty *property = (GEnumProperty *) pspec;
  glong oval = value->data[0].v_long;

  if (property->e_class == NULL ||
      g_enum_get_value (property->e_class, value->data[0].v_long) == NULL)
    value->data[0].v_long = 0;

  return value->data[0].v_long != oval;
}

static void
property_enum_finalize (GParamSpec *pspec)
{
  GEnumProperty *property = (GEnumProperty *) pspec;
  GParamSpecClass *parent_class =
    g_type_class_peek (g_type_parent (_g_enum_property_get_type ()));

  if (property->e_class != NULL)
    {
      g_type_class_unref (property->e_class);
      property->e_class = NULL;
    }

  parent_class->finalize (pspec);
}

static void
property_enum_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_FLAGS;

  klass->value_validate = property_enum_validate;

  klass->finalize = property_enum_finalize;
}

static void
property_enum_init (GParamSpec *pspec)
{
}

GType
_g_enum_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_enum_class_init,
        NULL, NULL,
        sizeof (GEnumProperty),
        0,
        (GInstanceInitFunc) property_enum_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GEnumProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_enum_property_new:
 * @name: canonical name of the property
 * @enum: enum for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a enumeration type registered
 * as a sub-type of %G_TYPE_ENUM.
 *
 * You should use g_property_set_prerequisite() to set the type
 * of the enumeration for validation; if the pre-requisite is unset,
 * setting or getting this property will result in a warning.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_enum_property_new (const gchar      *name,
                     GPropertyFlags    flags,
                     gssize            offset,
                     GPropertyEnumSet  setter,
                     GPropertyEnumGet  getter)
{
  GProperty *prop;
  GEnumProperty *internal;

  prop = g_property_create (_g_enum_property_get_type (),
                            G_TYPE_ENUM,
                            name,
                            flags,
                            offset,
                            sizeof (glong));

  internal = (GEnumProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_enum_property_validate (GProperty *property,
                          glong      value)
{
  GEnumProperty *e_prop = (GEnumProperty *) property;

  if (e_prop->e_class != NULL)
    {
      if (g_enum_get_value (e_prop->e_class, value) != NULL)
        return TRUE;
    }

  return FALSE;
}

static inline gboolean
g_enum_property_set_value (GProperty *property,
                           gpointer   gobject,
                           glong      value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_enum_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GEnumProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GEnumProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gulong *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      (* (gulong *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gulong
g_enum_property_get_value (GProperty *property,
                           gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GEnumProperty *) property)->getter != NULL)
    {
      return ((GEnumProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      return (* (gulong *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return 0.0;
    }
}

/*
 * GFlags
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_flags_property_get_type (void);

typedef struct {
  GProperty parent;

  GFlagsClass *f_class;

  GPropertyFlagsSet setter;
  GPropertyFlagsGet getter;
} GFlagsProperty;

static gboolean
property_flags_validate (GParamSpec *pspec,
                         GValue     *value)
{
  GFlagsProperty *property = (GFlagsProperty *) pspec;
  gulong oval = value->data[0].v_ulong;

  if (property->f_class != NULL)
    value->data[0].v_ulong &= property->f_class->mask;
  else
    value->data[0].v_ulong = 0;

  return value->data[0].v_ulong != oval;
}

static void
property_flags_finalize (GParamSpec *pspec)
{
  GFlagsProperty *property = (GFlagsProperty *) pspec;
  GParamSpecClass *parent_class =
    g_type_class_peek (g_type_parent (_g_flags_property_get_type ()));

  if (property->f_class)
    {
      g_type_class_unref (property->f_class);
      property->f_class = NULL;
    }

  parent_class->finalize (pspec);
}

static void
property_flags_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_FLAGS;

  klass->value_validate = property_flags_validate;

  klass->finalize = property_flags_finalize;
}

static void
property_flags_init (GParamSpec *pspec)
{
}

GType
_g_flags_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_flags_class_init,
        NULL, NULL,
        sizeof (GFlagsProperty),
        0,
        (GInstanceInitFunc) property_flags_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GFlagsProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_flags_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a flag type registered
 * as a sub-type of %G_TYPE_FLAGS.
 *
 * You should use g_property_set_prerequisite() to set the type
 * of the flags for validation; if the pre-requisite is unset,
 * setting or getting this property will result in a warning.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_flags_property_new (const gchar       *name,
                      GPropertyFlags     flags,
                      gssize             offset,
                      GPropertyFlagsSet  setter,
                      GPropertyFlagsGet  getter)
{
  GProperty *prop;
  GFlagsProperty *internal;

  prop = g_property_create (_g_flags_property_get_type (),
                            G_TYPE_FLAGS,
                            name,
                            flags,
                            offset,
                            sizeof (gulong));

  internal = (GFlagsProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_flags_property_validate (GProperty *property,
                           gulong     value)
{
  GFlagsProperty *f_prop = (GFlagsProperty *) property;

  if (f_prop->f_class != NULL)
    {
      gulong masked_value = value;

      masked_value &= f_prop->f_class->mask;

      return masked_value == value;
    }

  return FALSE;
}

static inline gboolean
g_flags_property_set_value (GProperty *property,
                            gpointer   gobject,
                            gulong     value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_flags_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GFlagsProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GFlagsProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gulong *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      (* (gulong *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gulong
g_flags_property_get_value (GProperty *property,
                            gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GFlagsProperty *) property)->getter != NULL)
    {
      return ((GFlagsProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      return (* (gulong *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return 0.0;
    }
}

/*
 * GFloat
 */

#define G_FLOAT_EPSILON         (1e-30)

/* forward declaration for -Wmissing-prototypes */
GType _g_float_property_get_type (void);

typedef struct {
  GProperty parent;

  gfloat min_value;
  gfloat max_value;
  gfloat epsilon;

  GPropertyFloatSet setter;
  GPropertyFloatGet getter;
} GFloatProperty;

static gboolean
property_float_validate (GParamSpec *pspec,
                         GValue     *value)
{
  GFloatProperty *property = (GFloatProperty *) pspec;
  gfloat oval = value->data[0].v_float;

  value->data[0].v_float = CLAMP (value->data[0].v_float,
                                  property->min_value,
                                  property->max_value);

  return value->data[0].v_float != oval;
}

static gint
property_float_values_cmp (GParamSpec   *pspec,
                           const GValue *value1,
                           const GValue *value2)
{
  gfloat epsilon = ((GFloatProperty *) pspec)->epsilon;

  if (value1->data[0].v_float < value2->data[0].v_float)
    return - (value2->data[0].v_float - value1->data[0].v_float > epsilon);
  else
    return value1->data[0].v_float - value2->data[0].v_float > epsilon;
}

static void
property_float_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_FLOAT;

  klass->value_validate = property_float_validate;
  klass->values_cmp = property_float_values_cmp;
}

static void
property_float_init (GParamSpec *pspec)
{
  GFloatProperty *property = (GFloatProperty *) pspec;

  property->min_value = -G_MAXFLOAT;
  property->max_value = G_MAXFLOAT;
  property->epsilon = G_FLOAT_EPSILON;
}

GType
_g_float_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_float_class_init,
        NULL, NULL,
        sizeof (GFloatProperty),
        0,
        (GInstanceInitFunc) property_float_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GFloatProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_float_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a single precision floating
 * point value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_float_property_new (const gchar       *name,
                      GPropertyFlags     flags,
                      gssize             offset,
                      GPropertyFloatSet  setter,
                      GPropertyFloatGet  getter)
{
  GProperty *prop;
  GFloatProperty *internal;

  prop = g_property_create (_g_float_property_get_type (),
                            G_TYPE_FLOAT,
                            name,
                            flags,
                            offset,
                            sizeof (float));

  internal = (GFloatProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline void
g_float_property_set_range (GProperty *property,
                            gfloat     min_value,
                            gfloat     max_value)
{
  if (min_value > max_value)
    {
      g_critical (G_STRLOC ": Invalid range for the property '%s'",
                  G_PARAM_SPEC (property)->name);
      return;
    }

  ((GFloatProperty *) property)->min_value = min_value;
  ((GFloatProperty *) property)->max_value = max_value;
}

static inline void
g_float_property_get_range (GProperty *property,
                            gfloat    *min_value,
                            gfloat    *max_value)
{
  *min_value = ((GFloatProperty *) property)->min_value;
  *max_value = ((GFloatProperty *) property)->max_value;
}

static inline gboolean
g_float_property_validate (GProperty *property,
                           gfloat     value)
{
  GFloatProperty *internal = (GFloatProperty *) property;

  if (value >= internal->min_value &&
      value <= internal->max_value)
    return TRUE;

  return FALSE;
}

static inline gboolean
g_float_property_set_value (GProperty *property,
                            gpointer   gobject,
                            gfloat     value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_float_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GFloatProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GFloatProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gfloat *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      (* (gfloat *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gfloat
g_float_property_get_value (GProperty *property,
                            gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GFloatProperty *) property)->getter != NULL)
    {
      return ((GFloatProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      return (* (gfloat *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return 0.0;
    }
}

/*
 * GDouble
 */

#define G_DOUBLE_EPSILON        (1e-90)

/* forward declaration for -Wmissing-prototypes */
GType _g_double_property_get_type (void);

typedef struct {
  GProperty parent;

  gdouble min_value;
  gdouble max_value;
  gdouble epsilon;

  GPropertyDoubleSet setter;
  GPropertyDoubleGet getter;
} GDoubleProperty;

static gboolean
property_double_validate (GParamSpec *pspec,
                          GValue     *value)
{
  GDoubleProperty *property = (GDoubleProperty *) pspec;
  gdouble oval = value->data[0].v_double;

  value->data[0].v_double = CLAMP (value->data[0].v_double,
                                   property->min_value,
                                   property->max_value);

  return value->data[0].v_double != oval;
}

static gint
property_double_values_cmp (GParamSpec   *pspec,
                            const GValue *value1,
                            const GValue *value2)
{
  gdouble epsilon = ((GDoubleProperty *) pspec)->epsilon;

  if (value1->data[0].v_double < value2->data[0].v_double)
    return - (value2->data[0].v_double - value1->data[0].v_double > epsilon);
  else
    return value1->data[0].v_double - value2->data[0].v_double > epsilon;
}

static void
property_double_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_DOUBLE;

  klass->value_validate = property_double_validate;
  klass->values_cmp = property_double_values_cmp;
}

static void
property_double_init (GParamSpec *pspec)
{
  GDoubleProperty *property = (GDoubleProperty *) pspec;

  property->min_value = -G_MAXDOUBLE;
  property->max_value = G_MAXDOUBLE;
  property->epsilon = G_DOUBLE_EPSILON;
}

GType
_g_double_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_double_class_init,
        NULL, NULL,
        sizeof (GDoubleProperty),
        0,
        (GInstanceInitFunc) property_double_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GDoubleProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_double_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a double precision floating
 * point value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_double_property_new (const gchar        *name,
                       GPropertyFlags      flags,
                       gssize              offset,
                       GPropertyDoubleSet  setter,
                       GPropertyDoubleGet  getter)
{
  GProperty *prop;
  GDoubleProperty *internal;

  prop = g_property_create (_g_double_property_get_type (),
                            G_TYPE_DOUBLE,
                            name,
                            flags,
                            offset,
                            sizeof (double));

  internal = (GDoubleProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline void
g_double_property_set_range (GProperty *property,
                             gdouble    min_value,
                             gdouble    max_value)
{
  if (min_value > max_value)
    {
      g_critical (G_STRLOC ": Invalid range for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return;
    }

  ((GDoubleProperty *) property)->min_value = min_value;
  ((GDoubleProperty *) property)->max_value = max_value;
}

static inline void
g_double_property_get_range (GProperty *property,
                             gdouble   *min_value,
                             gdouble   *max_value)
{
  *min_value = ((GDoubleProperty *) property)->min_value;
  *max_value = ((GDoubleProperty *) property)->max_value;
}

static inline gboolean
g_double_property_validate (GProperty *property,
                            gdouble    value)
{
  GDoubleProperty *internal = (GDoubleProperty *) property;

  if (value >= internal->min_value &&
      value <= internal->max_value)
    return TRUE;

  return FALSE;
}

static inline gboolean
g_double_property_set_value (GProperty *property,
                             gpointer   gobject,
                             gdouble    value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_double_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GDoubleProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GDoubleProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gdouble *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      (* (gdouble *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gdouble
g_double_property_get_value (GProperty *property,
                             gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GDoubleProperty *) property)->getter != NULL)
    {
      return ((GDoubleProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      return (* (gdouble *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return 0.0;
    }
}

/*
 * GString
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_string_property_get_type (void);

typedef struct {
  GProperty parent;

  GPropertyStringSet setter;
  GPropertyStringGet getter;
} GStringProperty;

static void
property_string_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_STRING;
}

static void
property_string_init (GParamSpec *pspec)
{
}

GType
_g_string_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_string_class_init,
        NULL, NULL,
        sizeof (GStringProperty),
        0,
        (GInstanceInitFunc) property_string_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GStringProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_string_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a string value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_string_property_new (const gchar        *name,
                       GPropertyFlags      flags,
                       gssize              offset,
                       GPropertyStringSet  setter,
                       GPropertyStringGet  getter)
{
  GProperty *prop;
  GStringProperty *internal;

  prop = g_property_create (_g_string_property_get_type (),
                            G_TYPE_STRING,
                            name,
                            flags,
                            offset,
                            sizeof (char *));

  internal = (GStringProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_string_property_validate (GProperty   *property,
                            const gchar *value)
{
  return TRUE;
}

static inline gboolean
g_string_property_set_value (GProperty   *property,
                             gpointer     gobject,
                             const gchar *value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_string_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GStringProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GStringProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;
      gchar *str;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      str = (* (gpointer *) field_p);

      if (g_strcmp0 (str, value) == 0)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      if (property->flags & G_PROPERTY_COPY_SET)
        {
          g_free (str);
          (* (gpointer *) field_p) = g_strdup (value);
        }
      else
        (* (gpointer *) field_p) = (gpointer) value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline const gchar *
g_string_property_get_value (GProperty *property,
                             gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GStringProperty *) property)->getter != NULL)
    {
      return ((GStringProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;
      gchar *retval;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if (property->flags & G_PROPERTY_COPY_GET)
        retval = g_strdup ((* (gpointer *) field_p));
      else
        retval = (* (gpointer *) field_p);

      return retval;
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return NULL;
    }
}

/*
 * GBoxed
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_boxed_property_get_type (void);

typedef struct {
  GProperty parent;

  GPropertyBoxedSet setter;
  GPropertyBoxedGet getter;
} GBoxedProperty;

static void
property_boxed_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_BOXED;
}

static void
property_boxed_init (GParamSpec *pspec)
{
}

GType
_g_boxed_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_boxed_class_init,
        NULL, NULL,
        sizeof (GBoxedProperty),
        0,
        (GInstanceInitFunc) property_boxed_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GBoxedProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_boxed_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to a boxed value.
 *
 * You can use g_property_set_prerequisite() to specify the #GType
 * of the boxed value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_boxed_property_new (const gchar       *name,
                      GPropertyFlags     flags,
                      gssize             offset,
                      GPropertyBoxedSet  setter,
                      GPropertyBoxedGet  getter)
{
  GProperty *prop;
  GBoxedProperty *internal;

  prop = g_property_create (_g_boxed_property_get_type (),
                            G_TYPE_BOXED,
                            name,
                            flags,
                            offset,
                            sizeof (gpointer));

  internal = (GBoxedProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_boxed_property_validate (GProperty     *property,
                           gconstpointer  value)
{
  return TRUE;
}

static inline gboolean
g_boxed_property_set_value (GProperty *property,
                            gpointer   gobject,
                            gpointer   value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_boxed_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GBoxedProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GBoxedProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;
      gpointer old_value;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if (property->flags & G_PROPERTY_COPY_SET)
        {
          old_value = (* (gpointer *) field_p);

          if (value != NULL)
            (* (gpointer *) field_p) = g_boxed_copy (((GParamSpec *) property)->value_type, value);
          else
            (* (gpointer *) field_p) = NULL;

          if (old_value != NULL)
            g_boxed_free (((GParamSpec *) property)->value_type, old_value);
        }
      else
        (* (gpointer *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gpointer
g_boxed_property_get_value (GProperty *property,
                            gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GBoxedProperty *) property)->getter != NULL)
    {
      return ((GBoxedProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;
      gpointer value;

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if (property->flags & G_PROPERTY_COPY_GET)
        value = g_boxed_copy (((GParamSpec *) property)->value_type, (* (gpointer *) field_p));
      else
        value = (* (gpointer *) field_p);

      return value;
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return NULL;
    }
}

/*
 * GObject
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_object_property_get_type (void);

typedef struct {
  GProperty parent;

  GPropertyObjectSet setter;
  GPropertyObjectGet getter;
} GObjectProperty;

static void
property_object_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_OBJECT;
}

static void
property_object_init (GParamSpec *pspec)
{
}

GType
_g_object_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_object_class_init,
        NULL, NULL,
        sizeof (GObjectProperty),
        0,
        (GInstanceInitFunc) property_object_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GObjectProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_object_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to an object value.
 *
 * You can use g_property_set_prerequisite() to specify the #GType
 * of the object value.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_object_property_new (const gchar        *name,
                       GPropertyFlags      flags,
                       gssize              offset,
                       GPropertyObjectSet  setter,
                       GPropertyObjectGet  getter)
{
  GProperty *prop;
  GObjectProperty *internal;

  prop = g_property_create (_g_object_property_get_type (),
                            G_TYPE_OBJECT,
                            name,
                            flags,
                            offset,
                            sizeof (gpointer));

  internal = (GObjectProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_object_property_validate (GProperty     *property,
                            gconstpointer  value)
{
  if (value == NULL)
    return FALSE;

  return g_type_is_a (G_OBJECT_TYPE (value), G_PARAM_SPEC (property)->value_type);
}

static inline gboolean
g_object_property_set_value (GProperty *property,
                             gpointer   gobject,
                             gpointer   value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_object_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GObjectProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GObjectProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;
      gpointer obj;

      g_return_val_if_fail (value == NULL || G_IS_OBJECT (value), FALSE);

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gpointer *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      if (property->flags & G_PROPERTY_COPY_SET)
        {
          obj = (* (gpointer *) field_p);
          if (obj != NULL)
            g_object_unref (obj);

          (* (gpointer *) field_p) = obj = value;

          if (obj != NULL)
            {
              if (G_IS_INITIALLY_UNOWNED (obj))
                g_object_ref_sink (obj);
              else
                g_object_ref (obj);
            }
        }
      else
        (* (gpointer *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gpointer
g_object_property_get_value (GProperty *property,
                             gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GObjectProperty *) property)->getter != NULL)
    {
      return ((GObjectProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = g_type_instance_get_private (gobject, G_OBJECT_TYPE (gobject));
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if (property->flags & G_PROPERTY_COPY_GET)
        {
          gpointer value = (* (gpointer *) field_p);

          if (value != NULL)
            return g_object_ref (value);
          else
            return NULL;
        }
      else
        return (* (gpointer *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return NULL;
    }
}

/*
 * gpointer
 */

/* forward declaration for -Wmissing-prototypes */
GType _g_pointer_property_get_type (void);

typedef struct {
  GProperty parent;

  GPropertyPointerSet setter;
  GPropertyPointerGet getter;
} GPointerProperty;

static void
property_pointer_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_POINTER;
}

static void
property_pointer_init (GParamSpec *pspec)
{
}

GType
_g_pointer_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_pointer_class_init,
        NULL, NULL,
        sizeof (GPointerProperty),
        0,
        (GInstanceInitFunc) property_pointer_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PROPERTY,
                                g_intern_static_string ("GPointerProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}

/**
 * g_pointer_property_new:
 * @name: canonical name of the property
 * @flags: flags for the property
 * @offset: the offset in the private structure of the field
 *   that stores the property, or -1
 * @setter: (allow-none): the setter function for the property
 * @getter: (allow-none): the getter function for the property
 *
 * Creates a new #GProperty mapping to an untyped pointer.
 *
 * Return value: the newly created #GProperty
 *
 * Since: 2.38
 */
GParamSpec *
g_pointer_property_new (const gchar        *name,
                        GPropertyFlags      flags,
                        gssize              offset,
                        GPropertyObjectSet  setter,
                        GPropertyObjectGet  getter)
{
  GProperty *prop;
  GPointerProperty *internal;

  prop = g_property_create (_g_pointer_property_get_type (),
                            G_TYPE_POINTER,
                            name,
                            flags,
                            offset,
                            sizeof (gpointer));

  internal = (GPointerProperty *) prop;
  internal->setter = setter;
  internal->getter = getter;

  return G_PARAM_SPEC (prop);
}

static inline gboolean
g_pointer_property_validate (GProperty     *property,
                             gconstpointer  value)
{
  return TRUE;
}

static inline gboolean
g_pointer_property_set_value (GProperty *property,
                              gpointer   gobject,
                              gpointer   value)
{
  gboolean retval = FALSE;

  if ((property->flags & G_PROPERTY_WRITABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not writable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (!g_pointer_property_validate (property, value))
    {
      g_warning ("The value for the property '%s' of object '%s' is out of the valid range",
                 G_PARAM_SPEC (property)->name,
                 G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GPointerProperty *) property)->setter != NULL)
    {
      property_lock_internal (property, gobject);

      retval = ((GPointerProperty *) property)->setter (gobject, value);

      property_unlock_internal (property, gobject);

      if (retval)
        g_object_notify_by_pspec (gobject, (GParamSpec *) property);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      property_lock_internal (property, gobject);

      priv_p = get_private_pointer (gobject);
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      if ((* (gpointer *) field_p) == value)
        {
          property_unlock_internal (property, gobject);
          return FALSE;
        }

      (* (gpointer *) field_p) = value;

      property_unlock_internal (property, gobject);

      g_object_notify_by_pspec (gobject, (GParamSpec *) property);

      retval = TRUE;
    }
  else
    g_critical (G_STRLOC ": No setter function or field offset specified "
                "for property '%s'",
                G_PARAM_SPEC (property)->name);

  return retval;
}

static inline gpointer
g_pointer_property_get_value (GProperty *property,
                              gpointer   gobject)
{
  if ((property->flags & G_PROPERTY_READABLE) == 0)
    {
      g_critical ("The property '%s' of object '%s' is not readable",
                  G_PARAM_SPEC (property)->name,
                  G_OBJECT_TYPE_NAME (gobject));
      return FALSE;
    }

  if (((GPointerProperty *) property)->getter != NULL)
    {
      return ((GPointerProperty *) property)->getter (gobject);
    }
  else if (property->field_offset >= 0)
    {
      gpointer priv_p, field_p;

      priv_p = get_private_pointer (gobject); 
      field_p = G_STRUCT_MEMBER_P (priv_p, property->field_offset);

      return (* (gpointer *) field_p);
    }
  else
    {
      g_critical (G_STRLOC ": No getter function or field offset specified "
                  "for property '%s'",
                  G_PARAM_SPEC (property)->name);
      return NULL;
    }
}

/*
 * GProperty common API
 */

/*< private >
 * g_property_set_installed:
 * @property: a #GProperty
 * @class_gtype: the #GType of the class that installed @property
 *
 * Performs additional work once a type class has been associated to
 * the property.
 */
void
_g_property_set_installed (GProperty *property,
                           gpointer   g_class,
                           GType      class_gtype)
{
  if (property->field_offset >= 0)
    {
      gboolean is_interface = G_TYPE_IS_INTERFACE (class_gtype);

      if (is_interface)
        {
          g_critical (G_STRLOC ": The property '%s' has a field offset value "
                      "but it is being installed on an interface of type '%s'. "
                      "Properties installed on interfaces cannot have direct "
                      "access to a structure field.",
                      G_PARAM_SPEC (property)->name,
                      g_type_name (class_gtype));
        }
    }

  /* if the property is using the default locking, pre-compute the
   * quark for the lock
   */
  if ((property->flags & G_PROPERTY_ATOMIC) != 0 &&
      property->prop_id == 0 &&
      property->lock_func == NULL)
    {
      gchar *lock_n = g_strconcat ("-g-property-id-",
                                   G_PARAM_SPEC (property)->name,
                                   NULL);
      property->prop_id = g_quark_from_string (lock_n);
      g_free (lock_n);
    }

  property->is_installed = TRUE;
}

static gboolean
is_canonical (const gchar *key)
{
  const gchar *p;

  for (p = key; *p != 0; p++)
    {
      gchar c = *p;

      if (c != '-' &&
          (c < '0' || c > '9') &&
          (c < 'A' || c > 'Z') &&
          (c < 'a' || c > 'z'))
        return FALSE;
    }

  return TRUE;
}

static void
canonicalize_name (gchar *key)
{
  gchar *p;

  for (p = key; *p != 0; p++)
    {
      gchar c = *p;

      if (c != '-' &&
          (c < '0' || c > '9') &&
          (c < 'A' || c > 'Z') &&
          (c < 'a' || c > 'z'))
        *p = '-';
    }
}

/**
 * g_property_canonicalize_name:
 * @name: a string
 *
 * Canonicalizes a string into a property name.
 *
 * Return value: (transfer full): a newly allocated string with
 *   the canonical version of @name
 *
 * Since: 2.38
 */
gchar *
g_property_canonicalize_name (const gchar *name)
{
  gchar *retval;

  g_return_val_if_fail (name != NULL, NULL);

  if (is_canonical (name))
    return g_strdup (g_intern_string (name));

  retval = g_strdup (name);
  canonicalize_name (retval);
  g_intern_string (retval);

  return retval;
}

/**
 * g_property_set_prerequisite:
 * @property: a #GProperty
 * @gtype: the prerequisite type
 *
 * Sets the prerequisite type for the @property.
 *
 * The prerequisite type must have the @property GType as a super-type,
 * and will be used to make the type checking stricter.
 *
 * Since: 2.38
 */
void
g_property_set_prerequisite (GProperty *property,
                             GType      gtype)
{
  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (gtype != G_TYPE_INVALID);
  g_return_if_fail (G_PARAM_SPEC (property)->value_type != G_TYPE_INVALID);
  g_return_if_fail (g_type_is_a (gtype, G_PARAM_SPEC (property)->value_type));

  switch (G_PARAM_SPEC (property)->value_type)
    {
    case G_TYPE_BOXED:
    case G_TYPE_OBJECT:
      G_PARAM_SPEC (property)->value_type = gtype;
      break;

    case G_TYPE_ENUM:
      G_PARAM_SPEC (property)->value_type = gtype;
      ((GEnumProperty *) property)->e_class = g_type_class_ref (gtype);
      break;

    case G_TYPE_FLAGS:
      G_PARAM_SPEC (property)->value_type = gtype;
      ((GFlagsProperty *) property)->f_class = g_type_class_ref (gtype);
      break;

    default:
      break;
    }
}

/**
 * g_property_set_range_values:
 * @property: a #GProperty
 * @min_value: a #GValue with the minimum value of the range
 * @max_value: a #GValue with the maximum value of the range
 *
 * Sets the valid range of @property, using #GValue<!-- -->s.
 *
 * This function is intended for language bindings.
 *
 * Since: 2.38
 */
void
g_property_set_range_values (GProperty    *property,
                             const GValue *min_value,
                             const GValue *max_value)
{
  GType gtype;

  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (G_PARAM_SPEC (property)->value_type != G_TYPE_INVALID);
  g_return_if_fail (!property->is_installed);
  g_return_if_fail (min_value != NULL && max_value != NULL);

  gtype = G_PARAM_SPEC (property)->value_type;
  g_return_if_fail (g_value_type_transformable (G_VALUE_TYPE (min_value), gtype));
  g_return_if_fail (g_value_type_transformable (G_VALUE_TYPE (max_value), gtype));

  switch (gtype)
    {
    case G_TYPE_BOOLEAN:
      g_boolean_property_set_range (property,
                                    g_value_get_boolean (min_value),
                                    g_value_get_boolean (max_value));
      break;

    case G_TYPE_INT:
      {
        gint min_v = g_value_get_int (min_value);
        gint max_v = g_value_get_int (max_value);

        switch (property->type_size)
          {
          case 1:
            g_int8_property_set_range (property, min_v, max_v);
            break;

          case 2:
            g_int16_property_set_range (property, min_v, max_v);
            break;

          case 4:
            g_int32_property_set_range (property, min_v, max_v);
            break;

          default:
            g_int_property_set_range (property, min_v, max_v);
            break;
          }
      }
      break;

    case G_TYPE_INT64:
      g_int64_property_set_range (property,
                                  g_value_get_int64 (min_value),
                                  g_value_get_int64 (max_value));
      break;

    case G_TYPE_LONG:
      g_long_property_set_range (property,
                                 g_value_get_long (min_value),
                                 g_value_get_long (max_value));
      break;

    case G_TYPE_UINT:
      {
        guint min_v = g_value_get_uint (min_value);
        guint max_v = g_value_get_uint (max_value);

        switch (property->type_size)
          {
          case 1:
            g_uint8_property_set_range (property, min_v, max_v);
            break;

          case 2:
            g_uint16_property_set_range (property, min_v, max_v);
            break;

          case 4:
            g_uint32_property_set_range (property, min_v, max_v);
            break;

          default:
            g_uint_property_set_range (property, min_v, max_v);
            break;
          }
      }
      break;

    case G_TYPE_UINT64:
      g_uint64_property_set_range (property,
                                   g_value_get_uint64 (min_value),
                                   g_value_get_uint64 (max_value));
      break;

    case G_TYPE_ULONG:
      g_ulong_property_set_range (property,
                                  g_value_get_ulong (min_value),
                                  g_value_get_ulong (max_value));
      break;

    case G_TYPE_FLOAT:
      g_float_property_set_range (property,
                                  g_value_get_float (min_value),
                                  g_value_get_float (max_value));
      break;

    case G_TYPE_DOUBLE:
      g_double_property_set_range (property,
                                   g_value_get_double (min_value),
                                   g_value_get_double (max_value));
      break;

    default:
      break;
    }
}

/**
 * g_property_get_range_values:
 * @property: a #GProperty
 * @min_value: a valid #GValue, initialized to the type of @property
 * @max_value: a valid #GValue, initialized to the type of @property
 *
 * Retrieves the bounds of the range of valid values for @property
 * and stores them into @min_value and @max_value.
 *
 * Return value: %TRUE if successful, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_get_range_values (GProperty *property,
                             GValue    *min_value,
                             GValue    *max_value)
{
  gboolean retval;
  GType gtype;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (min_value != NULL, FALSE);
  g_return_val_if_fail (max_value != NULL, FALSE);

  gtype = G_PARAM_SPEC (property)->value_type;
  g_return_val_if_fail (g_value_type_compatible (gtype, G_VALUE_TYPE (min_value)), FALSE);
  g_return_val_if_fail (g_value_type_compatible (gtype, G_VALUE_TYPE (max_value)), FALSE);

  switch (gtype)
    {
    case G_TYPE_BOOLEAN:
      {
        gboolean min_v, max_v;

        g_boolean_property_get_range (property, &min_v, &max_v);
        g_value_set_boolean (min_value, min_v);
        g_value_set_boolean (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_INT:
      {
        gint min_v, max_v;

        switch (property->type_size)
          {
          case 1:
            g_int8_property_get_range (property, (gint8 *) &min_v, (gint8 *) &max_v);
            break;

          case 2:
            g_int16_property_get_range (property, (gint16 *) &min_v, (gint16 *) &max_v);
            break;

          case 4:
            g_int32_property_get_range (property, (gint32 *) &min_v, (gint32 *) &max_v);
            break;

          default:
            g_int_property_get_range (property, &min_v, &max_v);
            break;
          }

        g_value_set_int (min_value, min_v);
        g_value_set_int (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_INT64:
      {
        gint64 min_v, max_v;

        g_int64_property_get_range (property, &min_v, &max_v);
        g_value_set_int64 (min_value, min_v);
        g_value_set_int64 (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_LONG:
      {
        glong min_v, max_v;

        g_long_property_get_range (property, &min_v, &max_v);
        g_value_set_long (min_value, min_v);
        g_value_set_long (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_UINT:
      {
        guint min_v, max_v;

        switch (property->type_size)
          {
          case 1:
            g_uint8_property_get_range (property, (guint8 *) &min_v, (guint8 *) &max_v);
            break;

          case 2:
            g_uint16_property_get_range (property, (guint16 *) &min_v, (guint16 *) &max_v);
            break;

          case 4:
            g_uint32_property_get_range (property, (guint32 *) &min_v, (guint32 *) &max_v);
            break;

          default:
            g_uint_property_get_range (property, &min_v, &max_v);
            break;
          }

        g_value_set_uint (min_value, min_v);
        g_value_set_uint (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_UINT64:
      {
        guint64 min_v, max_v;

        g_uint64_property_get_range (property, &min_v, &max_v);
        g_value_set_uint64 (min_value, min_v);
        g_value_set_uint64 (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_ULONG:
      {
        gulong min_v, max_v;

        g_ulong_property_get_range (property, &min_v, &max_v);
        g_value_set_ulong (min_value, min_v);
        g_value_set_ulong (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_FLOAT:
      {
        gfloat min_v, max_v;

        g_float_property_get_range (property, &min_v, &max_v);
        g_value_set_float (min_value, min_v);
        g_value_set_float (max_value, max_v);
      }
      retval = TRUE;
      break;

    case G_TYPE_DOUBLE:
      {
        gdouble min_v, max_v;

        g_double_property_get_range (property, &min_v, &max_v);
        g_value_set_double (min_value, min_v);
        g_value_set_double (max_value, max_v);
      }
      retval = TRUE;
      break;

    default:
      g_critical (G_STRLOC ": Invalid type '%s'", g_type_name (gtype));
      retval = FALSE;
      break;
    }

  return retval;
}

/**
 * g_property_set_range:
 * @property: a #GProperty
 * @...: the minimum and maximum values of the range
 *
 * Sets the range of valid values for @property.
 *
 * Since: 2.38
 */
void
g_property_set_range (GProperty *property,
                      ...)
{
  GType gtype;
  va_list args;

  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (!property->is_installed);

  gtype = G_PARAM_SPEC (property)->value_type;
  g_return_if_fail (gtype != G_TYPE_INVALID);

  va_start (args, property);

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      {
        gboolean min_v = va_arg (args, gboolean);
        gboolean max_v = va_arg (args, gboolean);

        g_boolean_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_INT:
      {
        gint min_v = va_arg (args, gint);
        gint max_v = va_arg (args, gint);

        switch (property->type_size)
          {
          case 1:
            g_int8_property_set_range (property, min_v, max_v);
            break;

          case 2:
            g_int16_property_set_range (property, min_v, max_v);
            break;

          case 4:
            g_int32_property_set_range (property, min_v, max_v);
            break;

          default:
            g_int_property_set_range (property, min_v, max_v);
            break;
          }
      }
      break;

    case G_TYPE_INT64:
      {
        gint64 min_v = va_arg (args, gint64);
        gint64 max_v = va_arg (args, gint64);

        g_int64_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_LONG:
      {
        glong min_v = va_arg (args, glong);
        glong max_v = va_arg (args, glong);

        g_long_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_UINT:
      {
        guint min_v = va_arg (args, guint);
        guint max_v = va_arg (args, guint);

        switch (property->type_size)
          {
          case 1:
            g_uint8_property_set_range (property, min_v, max_v);
            break;

          case 2:
            g_uint16_property_set_range (property, min_v, max_v);
            break;

          case 4:
            g_uint32_property_set_range (property, min_v, max_v);
            break;

          default:
            g_uint_property_set_range (property, min_v, max_v);
            break;
          }
      }
      break;

    case G_TYPE_UINT64:
      {
        guint64 min_v = va_arg (args, guint64);
        guint64 max_v = va_arg (args, guint64);

        g_uint64_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_ULONG:
      {
        gulong min_v = va_arg (args, gulong);
        gulong max_v = va_arg (args, gulong);

        g_ulong_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_FLOAT:
      {
        gfloat min_v = va_arg (args, gdouble);
        gfloat max_v = va_arg (args, gdouble);

        g_float_property_set_range (property, min_v, max_v);
      }
      break;

    case G_TYPE_DOUBLE:
      {
        gdouble min_v = va_arg (args, gdouble);
        gdouble max_v = va_arg (args, gdouble);

        g_double_property_set_range (property, min_v, max_v);
      }
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
    }

  va_end (args);
}

/**
 * g_property_get_range:
 * @property: a #GProperty
 * @...: the return locations for the minimum and maximum values
 *   of the range
 *
 * Retrieves the bounds of the range of valid values for @property.
 *
 * Return value: %TRUE on success, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_get_range (GProperty *property,
                      ...)
{
  va_list var_args;
  GType gtype;
  gboolean retval;
  gpointer min_p, max_p;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (G_PARAM_SPEC (property)->value_type != G_TYPE_INVALID, FALSE);

  gtype = G_PARAM_SPEC (property)->value_type;

  va_start (var_args, property);

  min_p = va_arg (var_args, gpointer);
  max_p = va_arg (var_args, gpointer);

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      g_boolean_property_get_range (property, (gboolean *) min_p, (gboolean *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_INT:
      switch (property->type_size)
        {
        case 1:
          g_int8_property_get_range (property, (gint8 *) min_p, (gint8 *) max_p);
          retval = TRUE;
          break;

        case 2:
          g_int16_property_get_range (property, (gint16 *) min_p, (gint16 *) max_p);
          retval = TRUE;
          break;

        case 4:
          g_int32_property_get_range (property, (gint32 *) min_p, (gint32 *) max_p);
          retval = TRUE;
          break;

        default:
          g_int_property_get_range (property, (gint *) min_p, (gint *) max_p);
          retval = TRUE;
          break;
        }
      break;

    case G_TYPE_INT64:
      g_int64_property_get_range (property, (gint64 *) min_p, (gint64 *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_LONG:
      g_long_property_get_range (property, (glong *) min_p, (glong *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_UINT:
      switch (property->type_size)
        {
        case 1:
          g_uint8_property_get_range (property, (guint8 *) min_p, (guint8 *) max_p);
          retval = TRUE;
          break;

        case 2:
          g_uint16_property_get_range (property, (guint16 *) min_p, (guint16 *) max_p);
          retval = TRUE;
          break;

        case 4:
          g_uint32_property_get_range (property, (guint32 *) min_p, (guint32 *) max_p);
          retval = TRUE;
          break;

        default:
          g_uint_property_get_range (property, (guint *) min_p, (guint *) max_p);
          retval = TRUE;
          break;
        }
      break;

    case G_TYPE_UINT64:
      g_uint64_property_get_range (property, (guint64 *) min_p, (guint64 *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_ULONG:
      g_ulong_property_get_range (property, (gulong *) min_p, (gulong *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_FLOAT:
      g_float_property_get_range (property, (gfloat *) min_p, (gfloat *) max_p);
      retval = TRUE;
      break;

    case G_TYPE_DOUBLE:
      g_double_property_get_range (property, (gdouble *) min_p, (gdouble *) max_p);
      retval = TRUE;
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      retval = FALSE;
    }

  va_end (var_args);

  return retval;
}

/**
 * g_property_set_va:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @flags: collection flags, as a bitwise or of #GPropertyCollectFlags values
 * @args: the value to set, inside a pointer to a #va_list
 *
 * Sets the value of @property for the given #GObject instance.
 *
 * This function is the va_list variant of g_property_set().
 *
 * Return value: %TRUE if the value was set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_set_va (GProperty             *property,
                   gpointer               gobject,
                   GPropertyCollectFlags  flags,
                   va_list               *args)
{
  gboolean retval;
  GType gtype;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (gobject), FALSE);
  g_return_val_if_fail (property->is_installed, FALSE);

  g_object_ref (gobject);

  gtype = ((GParamSpec *) property)->value_type;

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      retval = g_boolean_property_set_value (property, gobject, va_arg (*args, gboolean));
      break;

    case G_TYPE_INT:
      switch (property->type_size)
        {
        case 1:
          retval = g_int8_property_set_value (property, gobject, va_arg (*args, gint));
          break;

        case 2:
          retval = g_int16_property_set_value (property, gobject, va_arg (*args, gint));
          break;

        case 4:
          retval = g_int32_property_set_value (property, gobject, va_arg (*args, gint));
          break;

        default:
          retval = g_int_property_set_value (property, gobject, va_arg (*args, gint));
          break;
        }
      break;

    case G_TYPE_INT64:
      retval = g_int64_property_set_value (property, gobject, va_arg (*args, gint64));
      break;

    case G_TYPE_LONG:
      retval = g_long_property_set_value (property, gobject, va_arg (*args, glong));
      break;

    case G_TYPE_UINT:
      switch (property->type_size)
        {
        case 1:
          retval = g_uint8_property_set_value (property, gobject, va_arg (*args, guint));
          break;

        case 2:
          retval = g_uint16_property_set_value (property, gobject, va_arg (*args, guint));
          break;

        case 4:
          retval = g_uint32_property_set_value (property, gobject, va_arg (*args, guint));
          break;

        default:
          retval = g_uint_property_set_value (property, gobject, va_arg (*args, guint));
          break;
        }
      break;

    case G_TYPE_UINT64:
      retval = g_uint64_property_set_value (property, gobject, va_arg (*args, guint64));
      break;

    case G_TYPE_ULONG:
      retval = g_ulong_property_set_value (property, gobject, va_arg (*args, gulong));
      break;

    case G_TYPE_ENUM:
      retval = g_enum_property_set_value (property, gobject, va_arg (*args, glong));
      break;

    case G_TYPE_FLAGS:
      retval = g_flags_property_set_value (property, gobject, va_arg (*args, gulong));
      break;

    case G_TYPE_FLOAT:
      retval = g_float_property_set_value (property, gobject, va_arg (*args, gdouble));
      break;

    case G_TYPE_DOUBLE:
      retval = g_double_property_set_value (property, gobject, va_arg (*args, gdouble));
      break;

    case G_TYPE_STRING:
      retval = g_string_property_set_value (property, gobject, va_arg (*args, gchar *));
      break;

    case G_TYPE_BOXED:
      retval = g_boxed_property_set_value (property, gobject, va_arg (*args, gpointer));
      break;

    case G_TYPE_OBJECT:
      retval = g_object_property_set_value (property, gobject, va_arg (*args, gpointer));
      break;

    case G_TYPE_POINTER:
      retval = g_pointer_property_set_value (property, gobject, va_arg (*args, gpointer));
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      retval = FALSE;
      break;
    }

  g_object_unref (gobject);

  return retval;
}

/**
 * g_property_get_va:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @flags: collection flags
 * @args: a pointer to a #va_list with the property
 *
 * Retrieves the value of @property for the given #GObject instance.
 *
 * This function is the va_list variant of g_property_get().
 *
 * Return value: %TRUE if the value was successfully retrieved, and
 *   %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_get_va (GProperty             *property,
                   gpointer               gobject,
                   GPropertyCollectFlags  flags,
                   va_list               *args)
{
  GType gtype;
  gpointer ret_p = NULL;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (gobject), FALSE);
  g_return_val_if_fail (property->is_installed, FALSE);

  gtype = G_PARAM_SPEC (property)->value_type;

  ret_p = va_arg (*args, gpointer);
  if (ret_p == NULL)
    {
      g_critical (G_STRLOC ": value location for a property of type '%s' passed as NULL",
                  g_type_name (gtype));
      return FALSE;
    }

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      (* (gboolean *) ret_p) = g_boolean_property_get_value (property, gobject);
      break;

    case G_TYPE_INT:
      switch (property->type_size)
        {
        case 1:
          (* (gint8 *) ret_p) = g_int8_property_get_value (property, gobject);
          break;

        case 2:
          (* (gint16 *) ret_p) = g_int16_property_get_value (property, gobject);
          break;

        case 4:
          (* (gint32 *) ret_p) = g_int32_property_get_value (property, gobject);
          break;

        default:
          (* (gint *) ret_p) = g_int_property_get_value (property, gobject);
          break;
        }
      break;

    case G_TYPE_INT64:
      (* (gint64 *) ret_p) = g_int64_property_get_value (property, gobject);
      break;

    case G_TYPE_LONG:
      (* (glong *) ret_p) = g_long_property_get_value (property, gobject);
      break;

    case G_TYPE_UINT:
      switch (property->type_size)
        {
        case 1:
          (* (guint8 *) ret_p) = g_uint8_property_get_value (property, gobject);
          break;

        case 2:
          (* (guint16 *) ret_p) = g_uint16_property_get_value (property, gobject);
          break;

        case 4:
          (* (guint32 *) ret_p) = g_uint32_property_get_value (property, gobject);
          break;

        default:
          (* (guint *) ret_p) = g_uint_property_get_value (property, gobject);
          break;
        }
      break;

    case G_TYPE_UINT64:
      (* (guint64 *) ret_p) = g_uint64_property_get_value (property, gobject);
      break;

    case G_TYPE_ULONG:
      (* (gulong *) ret_p) = g_ulong_property_get_value (property, gobject);
      break;

    case G_TYPE_ENUM:
      (* (glong *) ret_p) = g_enum_property_get_value (property, gobject);
      break;

    case G_TYPE_FLAGS:
      (* (gulong *) ret_p) = g_flags_property_get_value (property, gobject);
      break;

    case G_TYPE_FLOAT:
      (* (gfloat *) ret_p) = g_float_property_get_value (property, gobject);
      break;

    case G_TYPE_DOUBLE:
      (* (gdouble *) ret_p) = g_double_property_get_value (property, gobject);
      break;

    case G_TYPE_STRING:
      {
        const gchar *value;

        value = g_string_property_get_value (property, gobject);

        if (((flags & G_PROPERTY_COLLECT_COPY) != 0) &&
            (property->flags & G_PROPERTY_COPY_GET) == 0)
          {
            (* (gchar **) ret_p) = g_strdup (value);
          }
        else
          (* (gconstpointer *) ret_p) = value;
      }
      break;

    case G_TYPE_BOXED:
      {
        gpointer boxed;

        boxed = g_boxed_property_get_value (property, gobject);

        if (((flags & G_PROPERTY_COLLECT_COPY) != 0) &&
            (property->flags & G_PROPERTY_COPY_GET) == 0)
          {
            if (boxed != NULL)
              (* (gpointer *) ret_p) = g_boxed_copy (gtype, boxed);
            else
              (* (gpointer *) ret_p) = NULL;
          }
        else
          (* (gpointer *) ret_p) = (gpointer) boxed;
      }
      break;

    case G_TYPE_OBJECT:
      {
        gpointer obj = g_object_property_get_value (property, gobject);

        if (((flags & G_PROPERTY_COLLECT_REF) != 0) &&
            (property->flags & G_PROPERTY_COPY_GET) == 0)
          {
            if (obj != NULL)
              (* (gpointer *) ret_p) = g_object_ref (obj);
            else
              (* (gpointer *) ret_p) = NULL;
          }
        else
          (* (gpointer *) ret_p) = obj;
      }
      break;

    case G_TYPE_POINTER:
      (* (gpointer *) ret_p) = g_pointer_property_get_value (property, gobject);
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      return FALSE;
    }

  return TRUE;
}

/**
 * g_property_set:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @...: the value to be set
 *
 * Sets the value of the @property for the given #GObject instance.
 *
 * The value will either be copied or have its reference count increased.
 *
 * Since: 2.38
 */
gboolean
g_property_set (GProperty *property,
                gpointer   gobject,
                ...)
{
  va_list args;
  gboolean res;

  va_start (args, gobject);
  res = g_property_set_va (property, gobject, 0, &args);
  va_end (args);

  return res;
}

/**
 * g_property_get:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @...: a pointer to the value to be retrieved
 *
 * Retrieves the value of the @property for the given #GObject instance.
 *
 * Since: 2.38
 */
gboolean
g_property_get (GProperty *property,
                gpointer   gobject,
                ...)
{
  va_list args;
  gboolean retval;

  va_start (args, gobject);
  retval = g_property_get_va (property, gobject, 0, &args);
  va_end (args);

  return retval;
}

static inline void
g_property_set_value_internal (GProperty    *property,
                               gpointer      gobject,
                               const GValue *value)
{
  GType gtype;

  g_object_ref (gobject);

  gtype = G_PARAM_SPEC (property)->value_type;

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      g_boolean_property_set_value (property, gobject, g_value_get_boolean (value));
      break;

    case G_TYPE_INT:
      {
        gint val = g_value_get_int (value);

        switch (property->type_size)
          {
          case 1:
            g_int8_property_set_value (property, gobject, val);
            break;

          case 2:
            g_int16_property_set_value (property, gobject, val);
            break;

          case 4:
            g_int32_property_set_value (property, gobject, val);
            break;

          default:
            g_int_property_set_value (property, gobject, val);
            break;
          }
      }
      break;

    case G_TYPE_INT64:
      g_int64_property_set_value (property, gobject, g_value_get_int64 (value));
      break;

    case G_TYPE_LONG:
      g_long_property_set_value (property, gobject, g_value_get_long (value));
      break;

    case G_TYPE_UINT:
      {
        guint val = g_value_get_uint (value);

        switch (property->type_size)
          {
          case 1:
            g_uint8_property_set_value (property, gobject, val);
            break;

          case 2:
            g_uint16_property_set_value (property, gobject, val);
            break;

          case 4:
            g_uint32_property_set_value (property, gobject, val);
            break;

          default:
            g_uint_property_set_value (property, gobject, val);
            break;
          }
      }
      break;

    case G_TYPE_UINT64:
      g_uint64_property_set_value (property, gobject, g_value_get_uint64 (value));
      break;

    case G_TYPE_ULONG:
      g_ulong_property_set_value (property, gobject, g_value_get_ulong (value));
      break;

    case G_TYPE_FLOAT:
      g_float_property_set_value (property, gobject, g_value_get_float (value));
      break;

    case G_TYPE_DOUBLE:
      g_double_property_set_value (property, gobject, g_value_get_double (value));
      break;

    case G_TYPE_ENUM:
      g_enum_property_set_value (property, gobject, g_value_get_enum (value));
      break;

    case G_TYPE_FLAGS:
      g_flags_property_set_value (property, gobject, g_value_get_flags (value));
      break;

    case G_TYPE_STRING:
      g_string_property_set_value (property, gobject, g_value_get_string (value));
      break;

    case G_TYPE_BOXED:
      g_boxed_property_set_value (property, gobject, g_value_get_boxed (value));
      break;

    case G_TYPE_OBJECT:
      g_object_property_set_value (property, gobject, g_value_get_object (value));
      break;

    case G_TYPE_POINTER:
      g_pointer_property_set_value (property, gobject, g_value_get_pointer (value));
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (G_VALUE_TYPE (value)));
      break;
    }

  g_object_unref (gobject);
}

/**
 * g_property_set_value:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @value: a #GValue
 *
 * Sets the value of the @property for the given #GObject instance
 * by unboxing it from the #GValue, honouring eventual transformation
 * functions between the #GValue type and the property type.
 *
 * Since: 2.38
 */
void
g_property_set_value (GProperty    *property,
                      gpointer      gobject,
                      const GValue *value)
{
  GValue copy = { 0, };
  GType gtype;

  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (gobject));
  g_return_if_fail (value != NULL);
  g_return_if_fail (property->is_installed);

  /* do not go jump through the transformability hoops if we're getting
   * the type we expect in the first place
   */
  gtype = G_PARAM_SPEC (property)->value_type;
  if (gtype == G_VALUE_TYPE (value))
    {
      g_property_set_value_internal (property, gobject, value);
      return;
    }

  g_return_if_fail (g_value_type_transformable (G_VALUE_TYPE (value), gtype));

  g_value_init (&copy, gtype);
  if (!g_value_transform (value, &copy))
    {
      g_critical (G_STRLOC ": Unable to transform a value of type '%s' "
                  "into a value of type '%s'",
                  g_type_name (G_VALUE_TYPE (value)),
                  g_type_name (gtype));
      return;
    }

  g_property_set_value_internal (property, gobject, &copy);

  g_value_unset (&copy);
}

/**
 * g_property_get_value:
 * @property: a #GProperty
 * @gobject: a #GObject instance
 * @value: a #GValue, initialized to the type of the property or to a
 *   type that satisfies the transformable relation
 *
 * Retrieves the value of @property for the object instance, and
 * boxes it inside a #GValue, honouring eventual transformation
 * functions between the #GValue type and the property type.
 *
 * Since: 2.38
 */
void
g_property_get_value (GProperty *property,
                      gpointer   gobject,
                      GValue    *value)
{
  GType gtype;
  GValue copy = { 0, };

  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (gobject));
  g_return_if_fail (value != NULL);
  g_return_if_fail (property->is_installed);

  gtype = G_PARAM_SPEC (property)->value_type;

  g_return_if_fail (g_value_type_transformable (G_VALUE_TYPE (value), gtype));

  g_value_init (&copy, gtype);

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (&copy, g_boolean_property_get_value (property, gobject));
      break;

    case G_TYPE_INT:
      {
        gint val;

        switch (property->type_size)
          {
          case 1:
            val = g_int8_property_get_value (property, gobject);
            break;

          case 2:
            val = g_int16_property_get_value (property, gobject);
            break;

          case 4:
            val = g_int32_property_get_value (property, gobject);
            break;

          default:
            val = g_int_property_get_value (property, gobject);
            break;
          }

        g_value_set_int (&copy, val);
      }
      break;

    case G_TYPE_INT64:
      g_value_set_int64 (&copy, g_int64_property_get_value (property, gobject));
      break;

    case G_TYPE_LONG:
      g_value_set_long (&copy, g_long_property_get_value (property, gobject));
      break;

    case G_TYPE_UINT:
      {
        guint val;

        switch (property->type_size)
          {
          case 1:
            val = g_uint8_property_get_value (property, gobject);
            break;

          case 2:
            val = g_uint16_property_get_value (property, gobject);
            break;

          case 4:
            val = g_uint32_property_get_value (property, gobject);
            break;

          default:
            val = g_uint_property_get_value (property, gobject);
            break;
          }

        g_value_set_uint (&copy, val);
      }
      break;

    case G_TYPE_UINT64:
      g_value_set_uint64 (&copy, g_uint64_property_get_value (property, gobject));
      break;

    case G_TYPE_ULONG:
      g_value_set_ulong (&copy, g_ulong_property_get_value (property, gobject));
      break;

    case G_TYPE_STRING:
      g_value_set_string (&copy, g_string_property_get_value (property, gobject));
      break;

    case G_TYPE_CHAR:
      g_value_set_schar (&copy, g_int8_property_get_value (property, gobject));
      break;

    case G_TYPE_UCHAR:
      g_value_set_uchar (&copy, g_uint8_property_get_value (property, gobject));
      break;

    case G_TYPE_ENUM:
      g_value_set_enum (&copy, g_enum_property_get_value (property, gobject));
      break;

    case G_TYPE_FLAGS:
      g_value_set_flags (&copy, g_flags_property_get_value (property, gobject));
      break;

    case G_TYPE_FLOAT:
      g_value_set_float (&copy, g_float_property_get_value (property, gobject));
      break;

    case G_TYPE_DOUBLE:
      g_value_set_double (&copy, g_double_property_get_value (property, gobject));
      break;

    case G_TYPE_BOXED:
      g_value_set_boxed (&copy, g_boxed_property_get_value (property, gobject));
      break;

    case G_TYPE_OBJECT:
      g_value_set_object (&copy, g_object_property_get_value (property, gobject));
      break;

    case G_TYPE_POINTER:
      g_value_set_pointer (&copy, g_pointer_property_get_value (property, gobject));
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      break;
    }

  if (!g_value_transform (&copy, value))
    {
      g_critical (G_STRLOC ": Unable to transform a value of type '%s' into "
                  "a value of type '%s'",
                  g_type_name (gtype),
                  g_type_name (G_VALUE_TYPE (value)));
    }

  g_value_unset (&copy);
}

/**
 * g_property_get_value_type:
 * @property: a #GProperty
 *
 * Retrieves the #GType of the value stored by the property.
 *
 * If a prerequisite type has been set, it will be the returned type.
 *
 * Return value: a #GType
 *
 * Since: 2.38
 */
GType
g_property_get_value_type (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), G_TYPE_INVALID);

  return G_PARAM_SPEC (property)->value_type;
}

/**
 * g_property_validate:
 * @property: a #GProperty
 * @...: the value to validate
 *
 * Validates the passed value against the validation rules of
 * the @property.
 *
 * Return value: %TRUE if the value is valid, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_validate (GProperty *property,
                     ...)
{
  gboolean retval = FALSE;
  GType gtype;
  va_list args;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  va_start (args, property);

  gtype = G_PARAM_SPEC (property)->value_type;

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      retval = g_boolean_property_validate (property, va_arg (args, gboolean));
      break;

    case G_TYPE_INT:
      switch (property->type_size)
        {
        case 1:
          retval = g_int8_property_validate (property, va_arg (args, gint));
          break;

        case 2:
          retval = g_int16_property_validate (property, va_arg (args, gint));
          break;

        case 4:
          retval = g_int32_property_validate (property, va_arg (args, gint));
          break;

        default:
          retval = g_int_property_validate (property, va_arg (args, gint));
          break;
        }
      break;

    case G_TYPE_INT64:
      retval = g_int64_property_validate (property, va_arg (args, gint64));
      break;

    case G_TYPE_LONG:
      retval = g_long_property_validate (property, va_arg (args, glong));
      break;

    case G_TYPE_UINT:
      switch (property->type_size)
        {
        case 1:
          retval = g_uint8_property_validate (property, va_arg (args, guint));
          break;

        case 2:
          retval = g_uint16_property_validate (property, va_arg (args, guint));
          break;

        case 4:
          retval = g_uint32_property_validate (property, va_arg (args, guint));
          break;

        default:
          retval = g_uint_property_validate (property, va_arg (args, guint));
          break;
        }
      break;

    case G_TYPE_UINT64:
      retval = g_uint64_property_validate (property, va_arg (args, guint64));
      break;

    case G_TYPE_ULONG:
      retval = g_ulong_property_validate (property, va_arg (args, gulong));
      break;

    case G_TYPE_FLOAT:
      retval = g_float_property_validate (property, va_arg (args, gdouble));
      break;

    case G_TYPE_DOUBLE:
      retval = g_double_property_validate (property, va_arg (args, gdouble));
      break;

    case G_TYPE_ENUM:
      retval = g_enum_property_validate (property, va_arg (args, glong));
      break;

    case G_TYPE_FLAGS:
      retval = g_enum_property_validate (property, va_arg (args, gulong));
      break;

    case G_TYPE_STRING:
      retval = g_string_property_validate (property, va_arg (args, gchar *));
      break;

    case G_TYPE_BOXED:
      retval = g_boxed_property_validate (property, va_arg (args, gpointer));
      break;

    case G_TYPE_OBJECT:
      retval = g_object_property_validate (property, va_arg (args, gpointer));
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      break;
    }

  va_end (args);

  return retval;
}

/**
 * g_property_validate_value:
 * @property: a #GProperty
 * @value: a #GValue initialized to the property type or to a type
 *   that is transformable into the property type
 *
 * Validates the value stored inside the passed #GValue against the
 * @property rules.
 *
 * Return value: %TRUE if the value is valid, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_validate_value (GProperty *property,
                           GValue    *value)
{
  GValue copy = { 0, };
  gboolean retval = FALSE;
  GType gtype;

  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  gtype = G_PARAM_SPEC (property)->value_type;

  g_return_val_if_fail (g_value_type_transformable (gtype, G_VALUE_TYPE (value)), FALSE);

  g_value_init (&copy, gtype);

  if (!g_value_transform (value, &copy))
    {
      g_critical (G_STRLOC ": Unable to transform a value of type '%s' "
                  "to a value of type '%s'",
                  g_type_name (G_VALUE_TYPE (value)),
                  g_type_name (gtype));
      g_value_unset (&copy);
      return FALSE;
    }

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_BOOLEAN:
      retval = g_boolean_property_validate (property, g_value_get_boolean (&copy));
      break;

    case G_TYPE_INT:
      {
        gint val = g_value_get_int (&copy);

        switch (property->type_size)
          {
          case 1:
            retval = g_int8_property_validate (property, val);
            break;

          case 2:
            retval = g_int16_property_validate (property, val);
            break;

          case 4:
            retval = g_int32_property_validate (property, val);
            break;

          default:
            retval = g_int_property_validate (property, val);
            break;
          }
      }
      break;

    case G_TYPE_INT64:
      retval = g_int64_property_validate (property, g_value_get_int64 (&copy));
      break;

    case G_TYPE_LONG:
      retval = g_long_property_validate (property, g_value_get_long (&copy));
      break;

    case G_TYPE_UINT:
      {
        guint val = g_value_get_uint (&copy);

        switch (property->type_size)
          {
          case 1:
            retval = g_uint8_property_validate (property, val);
            break;

          case 2:
            retval = g_uint16_property_validate (property, val);
            break;

          case 4:
            retval = g_uint32_property_validate (property, val);
            break;

          default:
            retval = g_uint_property_validate (property, val);
            break;
          }
      }
      break;

    case G_TYPE_UINT64:
      retval = g_uint64_property_validate (property, g_value_get_uint64 (&copy));
      break;

    case G_TYPE_ULONG:
      retval = g_ulong_property_validate (property, g_value_get_ulong (&copy));
      break;

    case G_TYPE_FLOAT:
      retval = g_float_property_validate (property, g_value_get_float (&copy));
      break;

    case G_TYPE_DOUBLE:
      retval = g_double_property_validate (property, g_value_get_double (&copy));
      break;

    case G_TYPE_ENUM:
      retval = g_enum_property_validate (property, g_value_get_enum (&copy));
      break;

    case G_TYPE_FLAGS:
      retval = g_flags_property_validate (property, g_value_get_flags (&copy));
      break;

    case G_TYPE_STRING:
      retval = g_string_property_validate (property, g_value_get_string (&copy));
      break;

    case G_TYPE_BOXED:
      retval = g_boxed_property_validate (property, g_value_get_boxed (&copy));
      break;

    case G_TYPE_OBJECT:
      retval = g_object_property_validate (property, g_value_get_object (&copy));
      break;

    default:
      g_critical (G_STRLOC ": Invalid type %s", g_type_name (gtype));
      retval = FALSE;
      break;
    }

  g_value_unset (&copy);

  return retval;
}

/**
 * g_property_is_writable:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_WRITABLE flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_writable (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_WRITABLE) != 0;
}

/**
 * g_property_is_readable:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_READABLE flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_readable (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_READABLE) != 0 ||
         (property->flags & G_PROPERTY_CONSTRUCT_ONLY) != 0;
}

/**
 * g_property_is_deprecated:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_DEPRECATED flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_deprecated (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_DEPRECATED) != 0;
}

/**
 * g_property_is_atomic:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_ATOMIC flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_atomic (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_ATOMIC) != 0;
}

/**
 * g_property_is_copy_set:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_COPY_SET flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_copy_set (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_COPY_SET) !=  0;
}

/**
 * g_property_is_copy_get:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_COPY_GET flag set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_copy_get (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_COPY_GET) !=  0;
}

/**
 * g_property_is_construct_only:
 * @property: a #GProperty
 *
 * Checks whether the @property has the %G_PROPERTY_CONSTRUCT_ONLY flag
 * set.
 *
 * Return value: %TRUE if the flag is set, and %FALSE otherwise
 *
 * Since: 2.38
 */
gboolean
g_property_is_construct_only (GProperty *property)
{
  g_return_val_if_fail (G_IS_PROPERTY (property), FALSE);

  return (property->flags & G_PROPERTY_CONSTRUCT_ONLY) != 0;
}

/**
 * g_property_lock:
 * @property: a #GProperty
 * @gobject: a #GObject
 *
 * Locks a property on the given object.
 *
 * Use g_property_unlock() to unlock the property when done.
 *
 * Since: 2.38
 */
void
g_property_lock (GProperty *property,
                 gpointer   gobject)
{
  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (gobject));

  property_lock_internal (property, gobject);
}

/**
 * g_property_unlock:
 * @property: a #GProperty
 * @gobject: a #GObject
 *
 * Unlocks a property on the given object previously locked
 * using g_property_lock().
 *
 * Since: 2.38
 */
void
g_property_unlock (GProperty *property,
                   gpointer   gobject)
{
  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (gobject));

  property_unlock_internal (property, gobject);
}

/**
 * g_property_set_lock_functions:
 * @property: a #GProperty
 * @lock_func: (allow-none): the function to be called when locking
 *   the @property, or %NULL for the default locking function
 * @unlock_func: (allow-none): the function to be called when unlocking
 *   the @property, or %NULL for the default unlocking function
 *
 * Replaces the locking and unlocking functions for @property with
 * custom functions.
 *
 * Since: 2.38
 */
void
g_property_set_lock_functions (GProperty           *property,
                               GPropertyLockFunc    lock_func,
                               GPropertyUnlockFunc  unlock_func)
{
  g_return_if_fail (G_IS_PROPERTY (property));
  g_return_if_fail (!property->is_installed);

  if (lock_func == NULL)
    g_return_if_fail (unlock_func == NULL);

  property->lock_func = lock_func;
  property->unlock_func = unlock_func;
}

static void
property_finalize (GParamSpec *pspec)
{
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PROPERTY));

  parent_class->finalize (pspec);
}

static gboolean
property_validate (GParamSpec *pspec,
                   GValue     *value)
{
  GProperty *property = G_PROPERTY (pspec);

  if (!g_value_type_transformable (G_VALUE_TYPE (value), pspec->value_type))
    return TRUE;

  return !g_property_validate_value (property, value);
}

static gint
property_values_cmp (GParamSpec   *pspec,
                     const GValue *value1,
                     const GValue *value2)
{
  return 0;
}

static void
property_class_init (GParamSpecClass *klass)
{
  klass->value_type = G_TYPE_INVALID;

  klass->value_validate = property_validate;
  klass->values_cmp = property_values_cmp;

  klass->finalize = property_finalize;
}

static void
property_init (GParamSpec *pspec)
{
  GProperty *property = G_PROPERTY (pspec);

  pspec->value_type = G_TYPE_INVALID;

  property->field_offset = -1;

  property->lock_func = NULL;
  property->unlock_func = NULL;
}

GType
g_property_get_type (void)
{
  static volatile gsize pspec_type_id__volatile = 0;

  if (g_once_init_enter (&pspec_type_id__volatile))
    {
      const GTypeInfo info = {
        sizeof (GParamSpecClass),
        NULL, NULL,
        (GClassInitFunc) property_class_init,
        NULL, NULL,
        sizeof (GProperty),
        0,
        (GInstanceInitFunc) property_init,
      };

      GType pspec_type_id =
        g_type_register_static (G_TYPE_PARAM,
                                g_intern_static_string ("GProperty"),
                                &info, 0);

      g_once_init_leave (&pspec_type_id__volatile, pspec_type_id);
    }

  return pspec_type_id__volatile;
}
