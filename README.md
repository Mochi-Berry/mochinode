# mochinode

MochiBerry module mochinode

**MochiBerry module** — one package, one repo: JS layer (`node_modules/`) +
native layer (`mochi_modules/`) + `manifest.json` at root and in both layers
for the engine loader. Status: `unknown` · version `0.1.0` · native entry
`mbjs_init_module()`.

```
mochinode/
├── package.json · package-lock.json · manifest.json
├── CMakeLists.txt · Makefile · LICENSE · README.md
├── mochiberry/   asset diagrams (svg)
├── docs/         01_architecture · 02_abi_reference · 03_sandbox_security
├── node_modules/ JS face: manifest.json + src/index.js + native/ + lib/
└── mochi_modules/ native: manifest.json + include/ + src/ + native/ + script/ + lib/
```

## Build

```sh
make ENGINE_INC=/path/to/engine/native-dir
# -> mochi_modules/lib/mochinode_x86_64.dll   (60316 B)
```

## Use (JS)

```js
import * as mochinode from 'mochinode';    // -> node_modules/src/index.js
```

`node_modules/src/index.js` (original JS face)

## Docs

[docs/01_architecture.md](docs/01_architecture.md) ·
[docs/02_abi_reference.md](docs/02_abi_reference.md) ·
[docs/03_sandbox_security.md](docs/03_sandbox_security.md)

## License

MIT — see `LICENSE`.
