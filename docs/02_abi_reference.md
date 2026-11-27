# 02 — ABI Reference (mochinode)

- Native entry point: `mbjs_init_module()`
- ABI version: `MBC1`
- Header ABI:
- (none)

## Runtime exports (0 — from manifest.json)

- (no exports registered)

Symbols above are exposed by `mochi_modules/src/mochinode.c` through
`MBJS_AddModuleExportList()` inside `mbjs_init_module()`; the values are
recorded by `MBJS_SetModuleExportList()` in the module init func.

## Build variables (no absolute host paths)

- `ENGINE_INC`: directory holding `mbjs.h` + `mbjs_runtime.h` (default
  `../libmochiberry`; override at build time).
- Windows link libs: (none additional).
