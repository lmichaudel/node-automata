# Hex Factory

A compact playable factory game on a pointy-top axial hex grid, rendered with
SDL3's `SDL_gpu` API. All geometric rendering is instanced and the anti-aliased
hex, capsule, circle, and rounded-rectangle shapes are evaluated in HLSL.

## Play

- `1`: belt tool. Drag from a building or belt to another building/belt.
- `2`: place a one-hex miner.
- `3`: place a three-hex smelter.
- `4`: place a four-hex assembler.
- `5`: place a three-hex depot.
- `6` or `X`: erase buildings and belts.
- `WASD` / arrows: pan; mouse wheel: zoom; middle/right drag: pan.

Routes are directed in drag order. Drawing away from an existing belt creates a
split; drawing into one creates a merge. A belt hex may carry all six input and
output links, and junction outputs dispatch items round-robin. Belt connections
to machines automatically create animated inserter sockets at the machine edge.

The production chain is ore -> ingot -> gear -> depot. The starter factory is
already running, and the delivery score is shown in the window title.

## Build

Dependencies are declared through xmake. `shadercross` and `msdf-atlas-gen` must
be on `PATH`.

```sh
xmake f -m debug
xmake
xmake run
```

HLSL is compiled to MSL on macOS, DXIL on Windows, and SPIR-V on Linux.
