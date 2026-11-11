# Random greeting entry

Open http://127.0.0.1:8080/random . C++ shuffles the three languages on each
request. Each appears exactly once, with a uniformly random first language.
The SVG plays the original writing/hold/fade durations without initial waits.
After all three (~26.89 seconds), it switches to the `ForgetWolf` landing in the same
document. Clicking anywhere switches immediately. There are no links, tooltips,
buttons, hover styling, JavaScript, CSS or additional browser requests.

The transition uses SVG SMIL `set` events to hide the greetings and reveal
SVG outlines in Alte DIN 1451 Mittelschrift and a static 15-cell Brainfuck tape,
centered at approximately 3/8 of the viewport height. The tape reserves stable
`bf-cell-0` through `bf-cell-14` and `bf-pointer` IDs. The output box and tape play an offline trace of the user-supplied
`message.bf`, checked against the body of `source/main.md` (477 characters).
Playback takes about 43.43 seconds including lead-in and hold. Real memory
states are sampled between output characters; this compresses execution time
rather than displaying every instruction. The 15-cell viewport stays on a fixed page while the pointer moves within it.
Crossing a page boundary switches to the next or previous 15-cell page.
Absolute memory addresses remain shown above the cells.
A fixed `Output >` prefix and printable ASCII characters accompany the tape.
Output uses the available width, reserving the prefix only on the first line.
Subsequent lines start at the left padding. The box grows to show the full text
without scrolling: currently 9 desktop rows and 12 mobile rows. Word boundaries
and the original paragraph breaks are preserved.
There are no row separators. Desktop and mobile use the same execution trace.
The heading, output and tape are 1.2x their previous size; the tape is moved down
to accommodate the two additional output rows and preserve a gap.
The output uses IBM VGA 8x16 outline glyphs, while the heading/tape retain DIN.
A golden-orange (#FFB000) vertical cursor with SVG Gaussian glow follows the output and blinks once
per second, continuing after output finishes. Both use native SVG animations.
triggered by `landingEnter.begin`. Further clicks do not restart it. There is no
runtime interpreter, JavaScript, CSS, external fetch or playback controls.
`message.bf` stores the supplied program; `source/main.md` stores the expected
text. `message.md` is refreshed with verified output at build time.
`brainfuck_animation.py` interprets 8-bit wrapping cells offline with an
instruction limit, then generates static SVG animation tracks. For a different
message update both program and expected text, then regenerate desktop and mobile:

    python random/generate.py
    python mobile/generate.py

The URL stays `/random`; `/helloworld` is also
available as a standalone minimal C++ CGI page. No click-to-accelerate behavior
is included, per the preference to skip directly without JavaScript.

`generate.py` builds six static orders from the existing SVG source headers;
it writes only files in this directory. Regenerate after changing handwriting:

    python random/generate.py

The commented interface in `order_policy.hpp` is disabled and loads no database:

- Mainland CN / Jiangsu 32 or Jiangxi 36: ru, zh, en.
- Other mainland CN provinces: zh first, shuffle en/ru.
- Outside mainland CN: en first, shuffle zh/ru.
- Unknown lookup result: the existing fully random order.

Future lookup code should normalize country/province codes before calling it.
The old standalone language pages and `/carousel` are unchanged.
