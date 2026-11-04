# Animation timing

## Original baseline

Saved before adjustment in `timing-baseline/` (generator and generated header).

- Initial wait: 0.400 s.
- 31 strokes; 30 pen lifts of 0.025 s each.
- Actual drawing: 5.250 s; drawing and lifts together: 6.000 s.
- Completion from SVG timeline start: 6.400 s.
- Path speed: about 6113.649 font units/s.
- The first `Hello` alone: about 1.515 s, excluding initial wait.

## Reference and adjustment

Public device demonstration by FavoriteTechX, published 2021-06-08:
https://www.youtube.com/watch?v=jRQV6i7yIs8

The reference is the actual phone footage, not the colorful opening graphic.
Inspection of frames sampled every 0.2 s around video 45–49 s shows English
`hello` beginning at approximately 46.1–46.2 s and completing around 48.4–48.6 s.
Use approximately 2.3 s of writing, with roughly +/-0.2 s visual uncertainty.
This is a measurement of this recording, not an official Apple specification;
other OS versions and language greetings may differ.

Because our font, capital H, size, and sentence differ, matching absolute
pixels/second would be misleading. Calibrate our initial `Hello` to 2.3 s,
then use that path-length speed for the whole sentence. This preserves original
paths, linear per-stroke motion, 0.025 s lifts, and the 0.400 s initial wait.

- New path speed: about 3907.896 font units/s (63.92% of the original speed).
- New sentence writing duration: about 8.963 s.
- New completion from SVG timeline start: about 9.363 s.
- This reference adjustment originally had no looping or fading.

To restore the baseline, copy `timing-baseline/generate_greeting.py` over the
project generator, run it, and recompile `hello.cpp`. Update README accordingly.

## Fade timing in the English page

The pre-fade English generator and header are saved in `timing-before-fades/`.
English timing is now configured in `languages/en.json`.
The total remains 9.363284 s, including the initial 0.4 s wait.
Writing completes at 8.463284 s, followed by 0.5 s of fully visible text.
The last 0.4 s fades out. The 0.3 s fade-in overlaps the first strokes.
Writing including pen lifts is now 8.063284 s, shortened to reserve hold/fade
time inside the existing total. Paths, directions and pen lift intervals stay
unchanged; the initial Hello no longer takes exactly 2.3 s after this adjustment.

Russian order audit: all 37 strokes preserve the original GOST coordinates,
point sequence, path direction and moveto pen-lift boundaries. Playback follows
these strokes sequentially, character by character. No order changes made.
This is source-preserving plotter order, not an optimization for shortest travel.
