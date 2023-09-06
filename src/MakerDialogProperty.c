#include <stdlib.h>
#include <glib/gprintf.h>
#include "MakerDialogUtil.h"
#include "MakerDialogProperty.h"

/*============================================
 * PropertyContext Methods
 */

void property_context_default(PropertyContext * ctx)
{
    if (ctx->spec->defaultValue == NULL)
        return;
    mkdg_log(DEBUG, "property_context_default(%s)", ctx->spec->key);
    gboolean ret = property_context_from_string(ctx, ctx->spec->defaultValue);

    if (!ret) {
        mkdg_log(WARN,
                 "property_context_default(%s): failed to convert string %s, return NULL",
                 ctx->spec->key, ctx->spec->defaultValue);
    }
}

PropertyContext *property_context_new(MkdgPropertySpec * spec,
                                      MkdgBackend * backend,
                                      gpointer parent, gpointer auxData)
{
    if (spec == NULL) {
        return NULL;
    }
    mkdg_log(INFO, "property_context_new(%s,-,-,-)", spec->key);
    PropertyContext *ctx = g_new0(PropertyContext, 1);

    ctx->spec = spec;
    ctx->backend = backend;
    ctx->parent = parent;
    ctx->auxData = auxData;
    g_value_init(&(ctx->value), ctx->spec->valueType);
    if (ctx->backend != NULL) {
        GValue *value = property_context_read(ctx, NULL);

        if (value == NULL) {
            property_context_default(ctx);
        }
    } else {
        property_context_default(ctx);
    }
    mkdg_log(DEBUG, "property_context_new(%s):Done", ctx->spec->key);
    return ctx;
}

gchar *property_context_to_string(PropertyContext * ctx)
{
    if (ctx == NULL) {
        return NULL;
    }
    return mkdg_g_value_to_string(&(ctx->value));
}

gboolean property_context_from_string(PropertyContext * ctx, const gchar * str)
{
    if (ctx == NULL) {
        return FALSE;
    }
    return mkdg_g_value_from_string(&(ctx->value), str);
}

gboolean property_context_from_gvalue(PropertyContext * ctx, GValue * value)
{
    if (ctx == NULL) {
        return FALSE;
    }
    if (!G_IS_VALUE(value)) {
        return FALSE;
    }
    g_value_copy(value, &(ctx->value));
    return TRUE;
}

/* read: backend -> Context */
/* write: Context -> backend */
/* get: Context -> GValue */
/* set: GValue -> Context */
/* load: read then get, errors in read are ignored */
/* save: set then write */
/* apply: Context -> apply callback */
/* use: load then apply */
/* assign: save then apply */
GValue *property_context_read(PropertyContext * ctx, gpointer userData)
{
    if (ctx == NULL) {
        return NULL;
    }
    mkdg_log(DEBUG, "property_context_read(%s,-)", ctx->spec->key);
    if (mkdg_has_flag(ctx->spec->propertyFlags, MKDG_PROPERTY_FLAG_NO_BACKEND)) {
        return NULL;
    }
    if (ctx->backend == NULL) {
        return NULL;
    }
    GValue *result = mkdg_backend_read(ctx->backend, &(ctx->value),
                                       ctx->spec->subSection,
                                       ctx->spec->key, userData);

    if (result == NULL) {
        mkdg_log(WARN, "property_context_read(%s): failed to read key",
                 ctx->spec->key);
    }
    return result;
}

gboolean property_context_write(PropertyContext * ctx, gpointer userData)
{
    if (ctx == NULL) {
        return FALSE;
    }
    mkdg_log(DEBUG, "property_context_read(%s,-)", ctx->spec->key);
    if (mkdg_has_flag(ctx->spec->propertyFlags, MKDG_PROPERTY_FLAG_NO_BACKEND)) {
        return FALSE;
    }
    if (ctx->backend == NULL) {
        return FALSE;
    }
    return ctx->backend->writeFunc(ctx->backend, &(ctx->value),
                                   ctx->spec->subSection, ctx->spec->key,
                                   userData);
}

GValue *property_context_get(PropertyContext * ctx)
{
    if (ctx == NULL) {
        mkdg_log(WARN, "property_context_get(-): ctx is NULL");
        return NULL;
    }
    mkdg_log(DEBUG, "property_context_get(%s): value=%s",
             ctx->spec->key, mkdg_g_value_to_string(&(ctx->value)));
    return &(ctx->value);
}

gboolean property_context_set(PropertyContext * ctx, GValue * value)
{
    if (ctx == NULL) {
        mkdg_log(WARN, "property_context_set(-): ctx is NULL");
        return FALSE;
    }
    if (!G_IS_VALUE(value)) {
        mkdg_log(WARN, "property_context_set(%s): value is not GValue",
                 ctx->spec->key);
        return FALSE;
    }
    mkdg_log(DEBUG, "property_context_set(%s,%s)", ctx->spec->key,
             mkdg_g_value_to_string(value));
    g_value_copy(value, &(ctx->value));
    return TRUE;
}

GValue *property_context_load(PropertyContext * ctx, gpointer userData)
{
    mkdg_log(DEBUG, "property_context_load(%s,-)", ctx->spec->key);
    property_context_read(ctx, userData);
    return property_context_get(ctx);
}

gboolean property_context_save(PropertyContext * ctx, GValue * value,
                               gpointer userData)
{
    mkdg_log(DEBUG, "property_context_save(%s,-)", ctx->spec->key);
    if (!property_context_set(ctx, value)) {
        return FALSE;
    }
    return property_context_write(ctx, userData);
}

gboolean property_context_apply(PropertyContext * ctx, gpointer userData)
{
    if (ctx == NULL || ctx->parent == NULL) {
        mkdg_log(WARN,
                 "property_context_apply(%s): either ctx or ctx->parent is NULL",
                 ctx->spec->key);
        return FALSE;
    }
    if (ctx->spec->applyFunc == NULL) {
        mkdg_log(DEBUG, "property_context_apply(%s,-): No apply function, skip",
                 ctx->spec->key);
        return TRUE;
    }
    mkdg_log(DEBUG, "property_context_apply(%s,-): value %s",
             ctx->spec->key, mkdg_g_value_to_string(&(ctx->value)));
    return ctx->spec->applyFunc(ctx, userData);
}

gboolean property_context_use(PropertyContext * ctx, gpointer userData)
{
    mkdg_log(DEBUG, "property_context_use(%s,-)", ctx->spec->key);
    GValue *ret = property_context_load(ctx, userData);

    if (ret == NULL) {
        mkdg_log(WARN,
                 "property_context_use(%s): property_context_load return NULL",
                 ctx->spec->key);
        return FALSE;
    }
    return property_context_apply(ctx, userData);
}

void property_context_free(PropertyContext * ctx)
{
    mkdg_log(INFO, "property_context_free(%s,-)", ctx->spec->key);
    g_value_unset(&(ctx->value));
    g_free(ctx);
}


/*============================================
 * MkdgProperties Methods
 */

/* This alone is sufficient to generate schemas */
MkdgProperties *mkdg_properties_from_spec_array(MkdgPropertySpec specs[],
                                                MkdgBackend * backend,
                                                gpointer parent,
                                                gpointer auxData)
{
    gsize arraySize = 0;
    gsize i;

    for (i = 0; specs[i].valueType != G_TYPE_INVALID; i++) {
        arraySize++;
    }
    MkdgProperties *result = g_new0(MkdgProperties, 1);

    result->backend = backend;
    result->contexts = g_ptr_array_sized_new(arraySize);
    result->auxData = auxData;
    for (i = 0; i < arraySize; i++) {
        PropertyContext *ctx = property_context_new(&specs[i], backend,
                                                    parent, auxData);

        g_ptr_array_add(result->contexts, (gpointer) ctx);
    }
    return result;
}

PropertyContext *mkdg_properties_find_by_key(MkdgProperties * properties,
                                             const gchar * key)
{
    gsize i;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);

        if (STRING_EQUALS(ctx->spec->key, key)) {
            return ctx;
        }
    }
    return NULL;
}

PropertyContext *mkdg_properties_index(MkdgProperties * properties, guint index)
{
    return (PropertyContext *)
        g_ptr_array_index(properties->contexts, index);
}

GValue *mkdg_properties_get_by_key(MkdgProperties * properties,
                                   const gchar * key)
{
    if (properties == NULL) {
        return NULL;
    }
    PropertyContext *ctx = mkdg_properties_find_by_key(properties, key);

    return property_context_get(ctx);
}

gboolean mkdg_properties_set_by_key(MkdgProperties * properties,
                                    const gchar * key, GValue * value)
{
    if (properties == NULL) {
        return FALSE;
    }
    PropertyContext *ctx = mkdg_properties_find_by_key(properties, key);

    return property_context_set(ctx, value);
}

gboolean mkdg_properties_set_boolean_by_key(MkdgProperties * properties,
                                            const gchar * key,
                                            gboolean boolValue)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_BOOLEAN);
    g_value_set_boolean(&gValue, boolValue);
    gboolean result = mkdg_properties_set_by_key(properties, key, &gValue);

    g_value_unset(&gValue);
    return result;
}

gboolean mkdg_properties_set_int_by_key(MkdgProperties * properties,
                                        const gchar * key, gint intValue)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_INT);
    g_value_set_int(&gValue, intValue);
    gboolean result = mkdg_properties_set_by_key(properties, key, &gValue);

    g_value_unset(&gValue);
    return result;
}

gboolean mkdg_properties_set_string_by_key(MkdgProperties * properties,
                                           const gchar * key,
                                           const gchar * stringValue)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_STRING);
    g_value_set_string(&gValue, stringValue);
    gboolean result = mkdg_properties_set_by_key(properties, key, &gValue);

    g_value_unset(&gValue);
    return result;
}

GValue *mkdg_properties_load_by_key(MkdgProperties * properties,
                                    const gchar * key, gpointer userData)
{
    if (properties == NULL) {
        return NULL;
    }
    PropertyContext *ctx = mkdg_properties_find_by_key(properties, key);

    return property_context_load(ctx, userData);
}

gboolean mkdg_properties_save_by_key(MkdgProperties * properties,
                                     const gchar * key, GValue * value,
                                     gpointer userData)
{
    if (properties == NULL) {
        return FALSE;
    }
    PropertyContext *ctx = mkdg_properties_find_by_key(properties, key);

    return property_context_save(ctx, value, userData);
}

gboolean mkdg_properties_save_boolean_by_key(MkdgProperties * properties,
                                             const gchar * key,
                                             gboolean boolValue,
                                             gpointer userData)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_BOOLEAN);
    g_value_set_boolean(&gValue, boolValue);
    gboolean result =
        mkdg_properties_save_by_key(properties, key, &gValue, userData);
    g_value_unset(&gValue);
    return result;
}

gboolean mkdg_properties_save_int_by_key(MkdgProperties * properties,
                                         const gchar * key, gint intValue,
                                         gpointer userData)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_INT);
    g_value_set_int(&gValue, intValue);
    gboolean result =
        mkdg_properties_save_by_key(properties, key, &gValue, userData);
    g_value_unset(&gValue);
    return result;
}

gboolean mkdg_properties_save_string_by_key(MkdgProperties * properties,
                                            const gchar * key,
                                            const gchar * stringValue,
                                            gpointer userData)
{
    GValue gValue = { 0 };
    g_value_init(&gValue, G_TYPE_STRING);
    g_value_set_string(&gValue, stringValue);
    gboolean result =
        mkdg_properties_save_by_key(properties, key, &gValue, userData);
    g_value_unset(&gValue);
    return result;
}

gboolean mkdg_properties_apply_by_key(MkdgProperties * properties,
                                      const gchar * key, gpointer userData)
{
    if (properties == NULL) {
        return FALSE;
    }
    PropertyContext *ctx = mkdg_properties_find_by_key(properties, key);

    return property_context_apply(ctx, userData);
}

gsize mkdg_properties_size(MkdgProperties * properties)
{
    return properties->contexts->len;
}

/* For setup interface */
gboolean mkdg_properties_load_all(MkdgProperties * properties,
                                  gpointer userData)
{
    gsize i;
    gboolean result = TRUE;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);
        GValue *value = property_context_load(ctx, userData);

        if (value == NULL) {
            result = FALSE;
        }
    }
    return result;
}

gboolean mkdg_properties_write_all(MkdgProperties * properties,
                                   gpointer userData)
{
    gsize i;
    gboolean result = TRUE;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);
        gboolean ret = property_context_write(ctx, userData);

        if (!ret) {
            result = FALSE;
        }
    }
    return result;
}

/* For actual runtime */
gboolean mkdg_properties_apply_all(MkdgProperties * properties,
                                   gpointer userData)
{
    gsize i;
    gboolean result = TRUE;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);
        gboolean ret = property_context_apply(ctx, userData);

        if (!ret) {
            result = FALSE;
        }
    }
    return result;
}

gboolean mkdg_properties_use_all(MkdgProperties * properties, gpointer userData)
{
    gsize i;
    gboolean result = TRUE;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);
        gboolean ret = property_context_use(ctx, userData);

        if (!ret) {
            result = FALSE;
        }
    }
    return result;
}

void mkdg_properties_free(MkdgProperties * properties)
{
    gsize i;

    for (i = 0; i < mkdg_properties_size(properties); i++) {
        PropertyContext *ctx = mkdg_properties_index(properties, i);

        property_context_free(ctx);
    }
    g_ptr_array_free(properties->contexts, TRUE);
    g_free(properties);
}
