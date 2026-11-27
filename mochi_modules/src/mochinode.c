/*
 * mochinode.c - native loader / dynalib bridge for the MochiBerry engine.
 *
 * This is the "exokernel" half of the runtime: it lets a script load a
 * shared library, resolve a symbol by name and call it through three
 * common C signatures, without any wrapper object in JS.
 *
 * The plugin itself never links against the engine.  Engine entry points
 * are resolved from the host image at init time (see mbjs_runtime.h), so
 * the engine's JSContext remains the single owner of atoms and GC.
 *
 * Exports (6): load, unload, resolve, callVoid, callInt, callStr.
 * Handles and symbol addresses are returned as BigInt.
 * `resolve` returns 0n when the symbol does not exist.
 */
#include "mbjs_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MBJSRuntime rt;

/* ---------------------------------------------------------------- helpers */

/* Copy a JS string into a malloc'd buffer and NEVER free the engine's
 * pointer: MBJS_ToCStringLen has an inconsistent ownership contract
 * (borrowed for plain ASCII, allocated otherwise), and calling
 * FreeCString on the ASCII path underflows the string refcount.  The
 * rare non-ASCII leak is bounded by the module's lifetime. */
static char *js_cstr_dup(MBJSContext *ctx, MBJSValueConst v, size_t *out_len)
{
    size_t len = 0;
    const char *p = rt.ToCStringLen(ctx, &len, v);
    char *out;
    if (!p)
        return NULL;
    out = (char *)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, p, len);
    out[len] = 0;
    if (out_len)
        *out_len = len;
    return out;
}

static MBJSValue nd_err(MBJSContext *ctx, const char *what)
{
    rt.ThrowTypeError(ctx, "mochinode: %s", what);
    return MBJS_EXCEPTION; /* contract: a C func that threw MUST return it */
}

/* Parse argv[0] as a BigInt handle. Returns NULL when it is not a bigint. */
static void *nd_handle(MBJSContext *ctx, MBJSValueConst v)
{
    uint64_t h = 0;
    if (rt.ToBigUint64(ctx, &h, v) != 0) /* 0 == success (QuickJS convention) */
        return (void *)-1;
    return (void *)(uintptr_t)h;
}

typedef void (*nd_void_fn)(void);
typedef int (*nd_int_fn)(void);
typedef const char *(*nd_str_fn)(void);

static MBJSValue nd_num(MBJSContext *ctx, uint64_t v)
{
    return rt.NewBigUint64(ctx, v);
}

/* ----------------------------------------------------------------- native */

static MBJSValue nd_load(MBJSContext *ctx, MBJSValueConst this_val,
                          int argc, MBJSValueConst *argv)
{
    char *path;
    void *h;
    (void)this_val;
    if (argc < 1)
        return nd_err(ctx, "load(name) requires a library path");
    path = js_cstr_dup(ctx, argv[0], NULL);
    if (!path)
        return nd_err(ctx, "load(name): cannot convert name");
#ifdef _WIN32
    h = (void *)LoadLibraryA(path);
    if (!h) {
        char msg[128];
        DWORD e = GetLastError();
        snprintf(msg, sizeof(msg), "LoadLibraryA failed (error %lu)",
                 (unsigned long)e);
        free(path);
        return nd_err(ctx, msg);
    }
#else
    h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!h) {
        char msg[256];
        const char *why = dlerror();
        snprintf(msg, sizeof(msg), "dlopen failed: %s", why ? why : "unknown");
        free(path);
        return nd_err(ctx, msg);
    }
#endif
    free(path);
    return nd_num(ctx, (uint64_t)(uintptr_t)h);
}

static MBJSValue nd_unload(MBJSContext *ctx, MBJSValueConst this_val,
                           int argc, MBJSValueConst *argv)
{
    void *h;
    (void)this_val;
    if (argc < 1)
        return nd_err(ctx, "unload(handle) requires a handle");
    h = nd_handle(ctx, argv[0]);
    if (!h || h == (void *)-1)
        return nd_err(ctx, "unload(handle): invalid handle");
#ifdef _WIN32
    if (!FreeLibrary((HMODULE)h))
        return nd_num(ctx, 0);
#else
    if (dlclose(h) != 0)
        return nd_num(ctx, 0);
#endif
    return nd_num(ctx, 1);
}

static MBJSValue nd_resolve(MBJSContext *ctx, MBJSValueConst this_val,
                            int argc, MBJSValueConst *argv)
{
    void *h, *sym;
    char *name;
    (void)this_val;
    if (argc < 2)
        return nd_err(ctx, "resolve(handle, symbol) requires both arguments");
    h = nd_handle(ctx, argv[0]);
    if (!h || h == (void *)-1)
        return nd_err(ctx, "resolve(handle, symbol): invalid handle");
    name = js_cstr_dup(ctx, argv[1], NULL);
    if (!name)
        return nd_err(ctx, "resolve(handle, symbol): cannot convert symbol");
#ifdef _WIN32
    sym = (void *)GetProcAddress((HMODULE)h, name);
#else
    sym = dlsym(h, name);
    if (!sym)
        sym = (void *)(uintptr_t)-1; /* keep dlerror() message around */
#endif
    if (sym == (void *)(uintptr_t)-1) {
        free(name);
#ifdef _WIN32
        return nd_num(ctx, 0);
#else
        return nd_num(ctx, 0);
#endif
    }
    free(name);
    return nd_num(ctx, (uint64_t)(uintptr_t)sym);
}

static MBJSValue nd_call_void(MBJSContext *ctx, MBJSValueConst this_val,
                              int argc, MBJSValueConst *argv)
{
    void *h, *sym;
    char *name;
    nd_void_fn f;
    (void)this_val;
    if (argc < 2)
        return nd_err(ctx, "callVoid(handle, symbol) requires both arguments");
    h = nd_handle(ctx, argv[0]);
    name = js_cstr_dup(ctx, argv[1], NULL);
    if (!h || h == (void *)-1 || !name)
        return nd_err(ctx, "callVoid: bad handle or symbol");
#ifdef _WIN32
    sym = (void *)GetProcAddress((HMODULE)h, name);
#else
    sym = dlsym(h, name);
#endif
    if (!sym) {
        free(name);
        return nd_err(ctx, "callVoid: symbol not found");
    }
    f = (nd_void_fn)sym;
    f();
    free(name);
    return nd_num(ctx, 1);
}

static MBJSValue nd_call_int(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    void *h, *sym;
    char *name;
    nd_int_fn f;
    int r;
    (void)this_val;
    if (argc < 2)
        return nd_err(ctx, "callInt(handle, symbol) requires both arguments");
    h = nd_handle(ctx, argv[0]);
    name = js_cstr_dup(ctx, argv[1], NULL);
    if (!h || h == (void *)-1 || !name)
        return nd_err(ctx, "callInt: bad handle or symbol");
#ifdef _WIN32
    sym = (void *)GetProcAddress((HMODULE)h, name);
#else
    sym = dlsym(h, name);
#endif
    if (!sym) {
        free(name);
        return nd_err(ctx, "callInt: symbol not found");
    }
    f = (nd_int_fn)sym;
    r = f();
    free(name);
    if (r < 0)
        return rt.NewBigInt64(ctx, (int64_t)r);
    return nd_num(ctx, (uint64_t)r);
}

static MBJSValue nd_call_str(MBJSContext *ctx, MBJSValueConst this_val,
                             int argc, MBJSValueConst *argv)
{
    void *h, *sym;
    char *name;
    nd_str_fn f;
    const char *s;
    (void)this_val;
    if (argc < 2)
        return nd_err(ctx, "callStr(handle, symbol) requires both arguments");
    h = nd_handle(ctx, argv[0]);
    name = js_cstr_dup(ctx, argv[1], NULL);
    if (!h || h == (void *)-1 || !name)
        return nd_err(ctx, "callStr: bad handle or symbol");
#ifdef _WIN32
    sym = (void *)GetProcAddress((HMODULE)h, name);
#else
    sym = dlsym(h, name);
#endif
    if (!sym) {
        free(name);
        return nd_err(ctx, "callStr: symbol not found");
    }
    f = (nd_str_fn)sym;
    s = f();
    free(name);
    return rt.NewStringLen(ctx, s ? s : "", s ? strlen(s) : 0);
}

/* ------------------------------------------------------------------ init */

static int nd_mod_init(MBJSContext *ctx, MBJSModuleDef *m)
{
    const MBJSCFunctionListEntry tab[] = {
        MBJS_CFUNC_DEF("load", 1, nd_load),
        MBJS_CFUNC_DEF("unload", 1, nd_unload),
        MBJS_CFUNC_DEF("resolve", 2, nd_resolve),
        MBJS_CFUNC_DEF("callVoid", 2, nd_call_void),
        MBJS_CFUNC_DEF("callInt", 2, nd_call_int),
        MBJS_CFUNC_DEF("callStr", 2, nd_call_str),
    };
    return rt.SetModuleExportList(ctx, m, tab, 6);
}

MBJS_MODULE_EXTERN MBJSModuleDef *mbjs_init_module(MBJSContext *ctx,
                                                    const char *name)
{
    const MBJSCFunctionListEntry tab[] = {
        MBJS_CFUNC_DEF("load", 1, nd_load),
        MBJS_CFUNC_DEF("unload", 1, nd_unload),
        MBJS_CFUNC_DEF("resolve", 2, nd_resolve),
        MBJS_CFUNC_DEF("callVoid", 2, nd_call_void),
        MBJS_CFUNC_DEF("callInt", 2, nd_call_int),
        MBJS_CFUNC_DEF("callStr", 2, nd_call_str),
    };
    const char *missing = mbjs_runtime_resolve(&rt);
    MBJSModuleDef *m;
    if (missing)
        return NULL; /* engine ABI incomplete in this host build */
    m = rt.NewCModule(ctx, name ? name : "mochinode", nd_mod_init);
    if (!m)
        return NULL;
    if (rt.AddModuleExportList(ctx, m, tab, 6) != 0)
        return NULL;
    return m;
}
