import argparse
from pathlib import Path

from PIL import Image, ImageDraw


def build_icon(size):
    scale = size / 512
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    drawing = ImageDraw.Draw(image)

    def box(values):
        return tuple(round(value * scale) for value in values)

    drawing.rounded_rectangle(
        box((16, 16, 496, 496)), radius=round(112 * scale), fill="#171717"
    )
    drawing.polygon(
        [
            (round(300 * scale), round(96 * scale)),
            (round(408 * scale), round(96 * scale)),
            (round(212 * scale), round(416 * scale)),
            (round(104 * scale), round(416 * scale)),
        ],
        fill="#dc2626",
    )
    drawing.rectangle(box((104, 104, 184, 408)), fill="#fafafa")
    drawing.rectangle(box((104, 104, 280, 176)), fill="#fafafa")
    drawing.rectangle(box((104, 228, 260, 296)), fill="#fafafa")
    return image


def main():
    parser = argparse.ArgumentParser(
        description="Génère les icônes natives datafuckerr."
    )
    parser.add_argument("output_directory")
    args = parser.parse_args()
    output = Path(args.output_directory).resolve()
    output.mkdir(parents=True, exist_ok=True)
    icon = build_icon(1024)
    icon.resize((512, 512), Image.Resampling.LANCZOS).save(output / "datafuckerr.png")
    icon.save(
        output / "datafuckerr.ico",
        format="ICO",
        sizes=[
            (16, 16),
            (24, 24),
            (32, 32),
            (48, 48),
            (64, 64),
            (128, 128),
            (256, 256),
        ],
    )
    icon.save(output / "datafuckerr.icns", format="ICNS")


if __name__ == "__main__":
    main()
