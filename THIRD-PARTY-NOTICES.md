# Third-Party Notices

KuroCat bundles no third-party bitmaps, fonts, or models at runtime. This file
records the design references used by the optional **水豚噜噜 / Lulu** pet.

## 水豚噜噜 (Lulu) — design reference

- Source: https://github.com/czy666chen/lulu
- Author: czy666chen
- License: MIT (Copyright (c) 2026 czy666chen)
- What we used: the right to derive from the published artwork. The Lulu
  renderer in `windows/src/CapybaraRenderer.hpp` draws **flat vector polygons**
  whose outlines were traced from one idle frame of the reference sprite sheet
  by `scripts/trace-lulu-shapes.py` (output: `windows/src/LuluShapes.hpp`).
  No image file from that repository is redistributed or loaded at runtime.
  Colors were sampled from the same artwork.

The MIT text of the upstream project is reproduced below.

```
MIT License

Copyright (c) 2026 czy666chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The "水豚噜噜" character itself remains the creation of its author; KuroCat only
ships an original vector re-drawing in the spirit of that design. If you plan a
commercial release, please credit the author and confirm trademark/character
usage with them.
