# Hex Factory

A compact playable factory game on a pointy-top axial hex grid. Blend2D renders
the anti-aliased hexes, capsules, circles, rounded rectangles, and sprites into a
software framebuffer, which SDL3 presents to the window.

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
output links, and junction outputs dispatch items round-robin. Split and merge
hexes use a one-tile junction body, while machine connections use compact sockets.

The production chain is ore -> ingot -> gear -> depot. The starter factory is
already running, and the delivery score is shown in the window title.

## Build

Dependencies are declared through xmake.

```sh
xmake f -m debug
xmake
xmake run
```
