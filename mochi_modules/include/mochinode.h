#ifndef MB_MOCHINODE_H
#define MB_MOCHINODE_H
/*
 * mochinode — MochiBerry native-first module (0.1.0).
 *
 * ABI contract:
 *   - exports the single entry point mbjs_init_module(MBJSContext*, const char*)
 *   - registers C functions via NewCModule / AddModuleExportList / SetModuleExportList
 *   - C functions that throw MUST return MBJS_EXCEPTION (never MBJS_UNDEFINED)
 *   - Runtime MBJS_* symbols come from mbjs_runtime_resolve(); do NOT link
 *     against mbjs.exe (error 126 on Windows).
 *
 * Runtime exports (measured): load, unload, resolve, callVoid, callInt, callStr
 */
#include <stdint.h>

typedef struct MBJSContext MBJSContext;
typedef struct MBJSModuleDef MBJSModuleDef;

MBJSModuleDef *mbjs_init_module(MBJSContext *ctx, const char *name);

#endif /* MB_MOCHINODE_H */
