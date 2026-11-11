# Mobile preview

Mobile welcome lines: `Hello, / ForgetWolf / here.`,
`Здравствуйте, / здесь / ФоргетВолк.`, `你好， / 我是 / ForgetWolf`.
`welcome_layout.py` repositions the existing paths into larger centered lines;
stroke order, shapes, timing, masks and desktop assets remain unchanged.

`/mobile` is an independent random-order greeting page, followed by the same
terminal landing. The top-right button opens a full-screen navigation overlay:
About, Blog, Friends, Contact. Text remains upright. No JS/CSS or auto-detection.
Output lettering and line spacing are 4/3 of desktop, with a taller five-row
output box. The output and tape share a viewport centered on the page, as on
desktop. The height of the mobile shared layout is 306 design units.
The six mobile SVG files are generated from the current desktop assets using
`python mobile/generate.py`; the desktop SVGs are never modified.
The timing, rebound, and explicitly bounded Safari toggle intervals are kept.
Navigation destinations remain placeholders, as in the desktop prototype.
