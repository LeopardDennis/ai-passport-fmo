<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

`fonts/NotoSansCJKsc-Regular.otf` is the unmodified Noto CJK regular font from
[Noto CJK](https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF/SimplifiedChinese),
licensed under SIL OFL 1.1 (`fonts/OFL.txt`). `fonts/fmo_channel_font.c` is the
derived 16px, 2bpp compressed LVGL font, covering ASCII, CJK punctuation and basic
Han characters U+4E00–U+9FFF. Extension-block characters and emoji are not covered.
Only the generated C file is linked into the firmware; the original is retained
for regeneration. Use `lv_font_conv@1.5.3` with the exact command in its header.
LVGL font compression and large glyph offsets must be enabled. This font is for
channel names, not the original FMO callsign typeface.

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

## FMO channel font

`fonts/NotoSansCJKsc-Regular.otf` and `fonts/fmo_channel_font.c` retain the licensed
Noto source and canonical 16 px, 2 bpp compressed glyphs (see `fonts/OFL.txt`).
`tools/compact_font.py` losslessly splits the generated font at build time into
fallback fonts whose bitmaps each fit below 1 MiB. Firmware and native preview
both compile that generated source with compact glyph descriptors. No existing
characters or antialiasing are removed, and no new resource partition is required.

## FMO callsign fonts

`fonts/Montserrat-Bold.ttf` is the unmodified Montserrat Bold source bundled in
LVGL v9.5.0 (`tests/src/test_files/fonts/`), from the
[Montserrat project](https://github.com/JulietaUla/Montserrat), licensed under
SIL OFL 1.1 (`fonts/Montserrat-OFL.txt`). `fonts/fmo_callsign_bold_20.c` and
`fonts/fmo_callsign_bold_32.c` contain 20/32 px, 4 bpp, uncompressed printable
ASCII glyphs (U+0020-U+007E), including callsign suffixes and punctuation.
Only these C files are linked, in both firmware and native preview. Setup
passwords keep the regular built-in fonts; Chinese labels keep the existing font.
Regenerate from the repository root with `lv_font_conv@1.5.3`:

```bash
lv_font_conv --size 20 --bpp 4 --format lvgl --lv-include lvgl.h --font assets/fonts/Montserrat-Bold.ttf -r 0x20-0x7e --no-compress --no-prefilter --force-fast-kern-format -o assets/fonts/fmo_callsign_bold_20.c
lv_font_conv --size 32 --bpp 4 --format lvgl --lv-include lvgl.h --font assets/fonts/Montserrat-Bold.ttf -r 0x20-0x7e --no-compress --no-prefilter --force-fast-kern-format -o assets/fonts/fmo_callsign_bold_32.c
```
