#!/usr/bin/env python3
"""Split a generated LVGL font losslessly to use 8-byte glyph descriptors.

Every compressed glyph and metric is preserved. Each fallback font has a
bitmap below 1 MiB so LV_FONT_FMT_TXT_LARGE can stay disabled globally.
"""
import re
import sys
from pathlib import Path

FIELDS = ("bitmap_index", "adv_w", "box_w", "box_h", "ofs_x", "ofs_y")


def read_font(path):
    source = Path(path).read_text()
    # Fail explicitly if regeneration changes the pinned font's rendering ABI.
    expected = {"bpp": 2, "bitmap_format": 1, "line_height": 31, "base_line": 9,
                "underline_position": -2, "underline_thickness": 1}
    for field, value in expected.items():
        match = re.search(r"\." + field + r"\s*=\s*(-?\d+)", source)
        if not match or int(match[1]) != value:
            raise ValueError("Unexpected source font metric: " + field)
    raw = re.search(r"glyph_bitmap\[\]\s*=\s*\{(.*?)\n\};", source, re.S)[1]
    raw = re.sub(r"/\*.*?\*/", "", raw, flags=re.S)
    bitmap = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", raw))
    raw = re.search(r"glyph_dsc\[\]\s*=\s*\{(.*?)\n\};", source, re.S)[1]
    glyphs = [dict((key, int(value)) for key, value in
                   re.findall(r"\.(\w+)\s*=\s*(-?\d+)", record))
              for record in re.findall(r"\{([^{}]+)\}", raw)]
    raw = re.search(r"cmaps\[\]\s*=\s*\{(.*?)\n\};", source, re.S)[1]
    maps = [dict((key, int(value)) for key, value in
                 re.findall(r"\.(range_start|range_length|glyph_id_start)\s*=\s*(\d+)", record))
            for record in re.findall(r"\{([^{}]+)\}", raw)]
    if "CMAP_SPARSE" in raw or "CMAP_FORMAT0_FULL" in raw:
        raise ValueError("Only contiguous tiny cmaps are supported")
    for glyph in glyphs:
        if set(glyph) != set(FIELDS) or not (0 <= glyph["adv_w"] < 4096 and
                0 <= glyph["box_w"] < 256 and 0 <= glyph["box_h"] < 256 and
                -128 <= glyph["ofs_x"] < 128 and -128 <= glyph["ofs_y"] < 128):
            raise ValueError("Glyph does not fit the compact descriptor")
    return bitmap, glyphs, maps


def split_font(bitmap, glyphs, maps):
    groups = []
    group = []
    for cmap in maps:
        end = cmap["glyph_id_start"] + cmap["range_length"]
        stop = glyphs[end]["bitmap_index"] if end < len(glyphs) else len(bitmap)
        start = glyphs[group[0]["glyph_id_start"]]["bitmap_index"] if group else 0
        if group and stop - start >= 1024 * 1024:
            groups.append(group)
            group = []
        group.append(cmap)
    groups.append(group)
    parts = []
    for group in groups:
        first = group[0]["glyph_id_start"]
        last = group[-1]["glyph_id_start"] + group[-1]["range_length"]
        begin = glyphs[first]["bitmap_index"]
        end = glyphs[last]["bitmap_index"] if last < len(glyphs) else len(bitmap)
        descriptors = [dict.fromkeys(FIELDS, 0)]
        for glyph in glyphs[first:last]:
            descriptors.append(dict(glyph, bitmap_index=glyph["bitmap_index"] - begin))
        cmaps = [dict(cmap, glyph_id_start=cmap["glyph_id_start"] - first + 1) for cmap in group]
        part = bitmap[begin:end]
        if len(part) >= 1024 * 1024:
            raise ValueError("A cmap exceeds the compact bitmap limit")
        parts.append((part, descriptors, cmaps))
    return parts


def generate(source, destination):
    bitmap, glyphs, cmaps = read_font(source)
    parts = split_font(bitmap, glyphs, cmaps)
    lines = ['/* Generated losslessly by tools/compact_font.py; edit the source font. */',
             '#include "lvgl.h"', '#if LV_FONT_FMT_TXT_LARGE',
             '#error "Compact font requires LV_FONT_FMT_TXT_LARGE=0"', '#endif']
    for i, (data, descriptors, maps) in enumerate(parts):
        lines.append(f"static const uint8_t bitmap_{i}[] = {{")
        lines.extend(",".join(f"0x{b:02x}" for b in data[j:j + 24]) + "," for j in range(0, len(data), 24))
        lines.append("};")
        lines.append(f"static const lv_font_fmt_txt_glyph_dsc_t glyphs_{i}[] = {{")
        lines.extend("{" + ",".join(f".{key}={glyph[key]}" for key in FIELDS) + "}," for glyph in descriptors)
        lines.append("};")
        lines.append(f"static const lv_font_fmt_txt_cmap_t cmaps_{i}[] = {{")
        lines.extend("{" + ",".join(f".{key}={value}" for key, value in cmap.items()) +
                     ",.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}," for cmap in maps)
        lines.append("};")
        lines.append(f"static const lv_font_fmt_txt_dsc_t dsc_{i} = {{.glyph_bitmap=bitmap_{i},"
                     f".glyph_dsc=glyphs_{i},.cmaps=cmaps_{i},.cmap_num={len(maps)},.bpp=2,.bitmap_format=1}};")
    for i in reversed(range(len(parts))):
        name = "fmo_channel_font" if i == 0 else f"font_{i}"
        qualifier = "" if i == 0 else "static "
        fallback = f"&font_{i + 1}" if i + 1 < len(parts) else "NULL"
        lines.append(f"{qualifier}const lv_font_t {name} = {{"
                     ".get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,"
                     ".line_height=31,.base_line=9,.subpx=LV_FONT_SUBPX_NONE,"
                     f".underline_position=-2,.underline_thickness=1,.dsc=&dsc_{i},.fallback={fallback}}};")
    Path(destination).write_text("\n".join(lines) + "\n")
    print(f"Compact font: {len(glyphs) - 1} glyphs preserved in {len(parts)} fallback fonts")


if __name__ == "__main__":
    generate(*sys.argv[1:])
