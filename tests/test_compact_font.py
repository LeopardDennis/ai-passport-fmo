"""Verify lossless glyph coverage, metrics, bitmap bytes and compact bounds."""
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('compact_font',ROOT/'tools/compact_font.py')
font=importlib.util.module_from_spec(spec);spec.loader.exec_module(font)
# Sparse entries use their list index as glyph ID, not their Unicode offset.
fixture = """
static const uint16_t unicode_list_0[] = {0x0, 0x8, 0x9, 0x14};
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {.range_start=100, .range_length=21, .glyph_id_start=1,
     .unicode_list=unicode_list_0, .list_length=4, .type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}
};
"""
assert font.read_cmaps(fixture) == [
    dict(range_start=100, range_length=1, glyph_id_start=1),
    dict(range_start=108, range_length=2, glyph_id_start=2),
    dict(range_start=120, range_length=1, glyph_id_start=4)]
try:
    font.read_cmaps(fixture.replace('0x14', '0x15'))
    raise AssertionError('Out-of-range sparse offset accepted')
except ValueError:
    pass
bitmap,glyphs,cmaps=font.read_font(ROOT/'assets/fonts/fmo_channel_font.c')
parts=font.split_font(bitmap,glyphs,cmaps)
assert len(parts)>1
assert b''.join(part[0] for part in parts)==bitmap
seen={}
for data,descriptors,maps in parts:
    assert len(data)<1024*1024
    for cmap in maps:
        for offset in range(cmap['range_length']):
            code=cmap['range_start']+offset
            index=cmap['glyph_id_start']+offset
            glyph=descriptors[index]
            end=descriptors[index+1]['bitmap_index'] if index+1<len(descriptors) else len(data)
            seen[code]=(dict(glyph,bitmap_index=0),data[glyph['bitmap_index']:end])
for cmap in cmaps:
    for offset in range(cmap['range_length']):
        code=cmap['range_start']+offset
        index=cmap['glyph_id_start']+offset
        glyph=glyphs[index]
        end=glyphs[index+1]['bitmap_index'] if index+1<len(glyphs) else len(bitmap)
        assert seen.pop(code)==(dict(glyph,bitmap_index=0),bitmap[glyph['bitmap_index']:end])
assert not seen
assert all(any(m['range_start']<=code<m['range_start']+m['range_length'] for _,_,maps in parts for m in maps)
           for code in range(0x4e00,0x9ff0))
assert all(any(m['range_start']<=code<m['range_start']+m['range_length'] for _,_,maps in parts for m in maps)
           for code in range(0xff01,0xff5f)), 'Missing fullwidth channel-name glyphs'
for code in map(ord, '“”‘’‐‑–—―…‥·•※‰′″°℃℉±×÷≤≥≠≈¥￥€£①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩ←↑→↓↔↕↖↗↘↙★☆○●◎◇◆□■△▲▽▼'):
    assert any(m['range_start']<=code<m['range_start']+m['range_length']
               for _,_,maps in parts for m in maps), f'Missing symbol U+{code:04X}'
print('Compact font: PASS (all codepoints, exact metrics/bitmap, 1 MiB bounds)')
