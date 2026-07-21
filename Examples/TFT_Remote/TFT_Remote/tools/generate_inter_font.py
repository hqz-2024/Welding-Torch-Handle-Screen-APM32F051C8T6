from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = Path(r"C:\Users\bestarc\Desktop\Inter-VariableFont_opsz,wght.ttf")
OUT_PATH = ROOT / "HARDWARE" / "LCD" / "ui_font_inter.h"

FONTS = [
    ("ROW", 10, "Medium", " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZsT/-"),
    ("TITLE", 16, "SemiBold", " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZsT/-"),
    ("TITLE_SMALL", 14, "SemiBold", " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZsT/-"),
    ("MAIN_NUM", 32, "SemiBold", "0123456789ATs"),
    ("MAIN_TYPE", 20, "SemiBold", " ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
]


def load_font(size, variation):
    font = ImageFont.truetype(str(FONT_PATH), size)
    try:
        font.set_variation_by_name(variation)
    except Exception:
        pass
    return font


def glyph_image(font, ch, top, bottom):
    width = max(1, int(math.ceil(font.getlength(ch))))
    height = bottom - top
    image = Image.new("L", (width, height), 0)

    if ch != " ":
        bbox = font.getbbox(ch)
        draw = ImageDraw.Draw(image)
        draw.text((-bbox[0], -top), ch, font=font, fill=255)

    return image


def c_char(ch):
    if ch == "'":
        return "'\\''"
    if ch == "\\":
        return "'\\\\'"
    return f"'{ch}'"


def image_bytes(image):
    if hasattr(image, "get_flattened_data"):
        return list(image.get_flattened_data())
    return list(image.getdata())


def byte_lines(data, indent="    "):
    vals = [f"0x{byte:02X}" for byte in data]
    return "\n".join(indent + ",".join(vals[i:i + 16]) + "," for i in range(0, len(vals), 16))


def main():
    parts = [
        "/*!",
        " * @file        ui_font_inter.h",
        " *",
        " * @brief       Generated from Inter-VariableFont_opsz,wght.ttf",
        " *              Do not hand edit glyph data; regenerate from the TTF.",
        " */",
        "",
        "#ifndef __UI_FONT_INTER_H",
        "#define __UI_FONT_INTER_H",
        "",
        "typedef struct {",
        "    char code;",
        "    uint8_t width;",
        "    const uint8_t *bitmap;",
        "} UiGlyph_T;",
        "",
        "typedef struct {",
        "    uint8_t height;",
        "    uint8_t count;",
        "    const UiGlyph_T *glyphs;",
        "} UiFont_T;",
        "",
    ]

    for name, size, variation, chars in FONTS:
        font = load_font(size, variation)
        unique_chars = "".join(dict.fromkeys(chars))
        boxes = [font.getbbox(ch) for ch in unique_chars if ch != " "]
        top = min(bbox[1] for bbox in boxes)
        bottom = max(bbox[3] for bbox in boxes)
        height = bottom - top
        glyph_entries = []

        parts.append(f"/* Inter {variation}, {size}px, height {height}px */")
        for ch in unique_chars:
            image = glyph_image(font, ch, top, bottom)
            array_name = f"inter_{name.lower()}_{ord(ch):02x}"
            parts.append(f"static const uint8_t {array_name}[] = {{ /* {ch if ch != ' ' else 'space'}, {image.width}x{image.height} */")
            parts.append(byte_lines(image_bytes(image)))
            parts.append("};")
            glyph_entries.append((ch, image.width, array_name))

        parts.append(f"static const UiGlyph_T inter_{name.lower()}_glyphs[] = {{")
        for ch, width, array_name in glyph_entries:
            parts.append(f"    {{{c_char(ch)}, {width}, {array_name}}},")
        parts.append("};")
        parts.append(f"static const UiFont_T UI_FONT_{name} = {{{height}, {len(glyph_entries)}, inter_{name.lower()}_glyphs}};")
        parts.append("")

    parts.append("#endif /* __UI_FONT_INTER_H */")
    parts.append("")

    OUT_PATH.write_text("\n".join(parts), encoding="utf-8")
    print(OUT_PATH)


if __name__ == "__main__":
    main()
