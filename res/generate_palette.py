#!/usr/bin/env python3
"""Generate a named game palette and 16 soft pastels as a single SVG image."""

from __future__ import annotations

import argparse
import colorsys
import html
import random
from dataclasses import dataclass
from pathlib import Path


GOLDEN_RATIO_CONJUGATE = 0.618033988749895
PASTEL_COUNT = 16


@dataclass(frozen=True)
class Color:
    name: str
    red: int
    green: int
    blue: int

    @property
    def css(self) -> str:
        return f"rgb({self.red}, {self.green}, {self.blue})"

    @property
    def label(self) -> str:
        return f"RGB({self.red}, {self.green}, {self.blue})"

    @property
    def text_color(self) -> str:
        luminance = (
            self.red * 0.2126 + self.green * 0.7152 + self.blue * 0.0722
        )
        return "rgb(247, 248, 250)" if luminance < 145 else "rgb(24, 28, 36)"


# Stable semantic colors for common game elements. Keep these names aligned with
# renderer concepts so the generated image can act as a compact style reference.
GAME_COLORS = [
    Color("Background", 232, 225, 213),
    Color("Grid", 200, 194, 183),
    Color("Super Grid", 167, 162, 153),
    Color("Miner Machine", 31, 135, 230),
    Color("Smelter Machine", 230, 56, 54),
    Color("Assembler Machine", 66, 161, 71),
    Color("Machine Socket", 63, 62, 66),
    Color("Belt", 99, 99, 99),
    Color("Belt Rail", 29, 29, 32),
    Color("Belt Chevron", 151, 148, 143),
    Color("Item Token", 99, 99, 99),
    Color("World Text", 245, 246, 248),
    Color("Selection", 126, 193, 255),
    Color("Success", 137, 218, 166),
    Color("Warning", 245, 196, 108),
    Color("Error", 242, 126, 134),
]


def generate_pastels(seed: int) -> list[Color]:
    """Generate evenly distributed, deterministic soft-pastel colors."""
    rng = random.Random(seed)
    hue = rng.random()
    colors: list[Color] = []

    for index in range(PASTEL_COUNT):
        hue = (hue + GOLDEN_RATIO_CONJUGATE + rng.uniform(-0.02, 0.02)) % 1.0
        saturation = rng.uniform(0.44, 0.62)
        lightness = rng.uniform(0.72, 0.84)
        red, green, blue = colorsys.hls_to_rgb(hue, lightness, saturation)
        colors.append(
            Color(
                name=f"Pastel {index + 1:02d}",
                red=round(red * 255),
                green=round(green * 255),
                blue=round(blue * 255),
            )
        )

    return colors


def swatch(color: Color, index: int, columns: int, y_offset: int) -> str:
    card_width = 250
    card_height = 112
    gap = 14
    column = index % columns
    row = index // columns
    x = 24 + column * (card_width + gap)
    y = y_offset + row * (card_height + gap)
    name = html.escape(color.name)

    return f"""  <g>
    <rect x="{x}" y="{y}" width="{card_width}" height="{card_height}" rx="12"
          fill="{color.css}"/>
    <text x="{x + 16}" y="{y + 48}" fill="{color.text_color}"
          font-family="system-ui, sans-serif" font-size="17" font-weight="650">{name}</text>
    <text x="{x + 16}" y="{y + 78}" fill="{color.text_color}"
          font-family="ui-monospace, monospace" font-size="14">{color.label}</text>
  </g>"""


def write_svg(path: Path, pastels: list[Color]) -> None:
    columns = 4
    card_height = 112
    gap = 14
    section_gap = 78
    game_rows = (len(GAME_COLORS) + columns - 1) // columns
    pastel_rows = (len(pastels) + columns - 1) // columns
    game_y = 96
    pastel_heading_y = game_y + game_rows * (card_height + gap) + section_gap
    pastel_y = pastel_heading_y + 38
    width = 24 * 2 + columns * 250 + (columns - 1) * gap
    height = pastel_y + pastel_rows * (card_height + gap) + 24

    content = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '  <rect width="100%" height="100%" fill="rgb(232, 225, 213)"/>',
        '  <text x="24" y="44" fill="rgb(43, 40, 37)" '
        'font-family="system-ui, sans-serif" font-size="26" font-weight="700">'
        "Node Automata Palette</text>",
        '  <text x="24" y="76" fill="rgb(96, 89, 82)" '
        'font-family="system-ui, sans-serif" font-size="16">Regular elements</text>',
    ]
    content.extend(
        swatch(color, index, columns, game_y) for index, color in enumerate(GAME_COLORS)
    )
    content.append(
        f'  <text x="24" y="{pastel_heading_y}" fill="rgb(96, 89, 82)" '
        'font-family="system-ui, sans-serif" font-size="16">16 generated pastels</text>'
    )
    content.extend(
        swatch(color, index, columns, pastel_y) for index, color in enumerate(pastels)
    )
    content.append("</svg>")

    path.write_text("\n".join(content) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    script_directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=2026, help="deterministic pastel seed")
    parser.add_argument(
        "--output",
        type=Path,
        default=script_directory / "soft_pastel_palette.svg",
        help="SVG image output path",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_svg(args.output, generate_pastels(args.seed))
    print(f"Palette image written to {args.output}")


if __name__ == "__main__":
    main()
