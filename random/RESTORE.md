# Before Cover Flow

Before adding the static Cover Flow preview, the six generated SVG pages,
the existing Python generation/verification sources, the random README and
message, and `random.cpp` were copied beside themselves with `.bak` appended.
All 14 backup hashes matched their original files before editing.

To restore the displayed interface, replace each of the six `random/*.svg`
files with its corresponding `.svg.bak` copy. No server rebuild is necessary.
To restore generation too, restore `random/generate.py.bak` as `generate.py`.
That version does not import the new `coverflow.py` module.

The backups are not overwritten by the generator. Fonts are unchanged and
continue to live in `random/fonts`.
