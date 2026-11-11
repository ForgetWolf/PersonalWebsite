# Independent, single-document carousel

Open http://127.0.0.1:8080/carousel . The C++ CGI serves `carousel.svg`, a new
combined SVG with English, Russian and Chinese in one repeating SMIL timeline.
There is no navigation, refresh, JavaScript, CSS or additional language fetch.
Legacy `/carousel/0`, `/carousel/1`, `/carousel/2` also serve this combined page.

The standalone language sources, paths, configurations and CGI executables are
untouched. `/en`, `/ru`, `/zh` and the original homepage retain their behavior.

To change order, rearrange `order.txt`, then run from the project directory:

    python carousel/generate.py

The generator reads the existing three SVG headers and writes only the new
`carousel/carousel.svg`. All IDs and references are namespaced. Every animated
property repeats on the same 26.88984-second clock, including visibility masks,
so later loops start with clean strokes. The original per-language writing,
hold and fade durations are preserved; only the initial 0.4-second waits are
omitted. Each next fade-in starts exactly when the preceding fade-out ends,
including Chinese back to English. There is no interval held blank; the fades
themselves still pass through zero opacity at the shared boundary.

Validation:

    python carousel/verify.py
    python carousel/render_preview.py

The first checks original geometry, timing, unique IDs, valid references and
three cycles of animation values. The second uses the existing offline renderer
to create `preview.png`; its optional dependencies are in the project `.tools`.
