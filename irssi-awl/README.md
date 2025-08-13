# irssi-awl (native)

Minimal native Irssi module that provides a statusbar item `awl` rendering an advanced window list (single line).

Build

- Prereqs: glib-2.0 and gmodule-2.0 development packages installed; Irssi source checked out at `/workspace/ext/irssi` (adjust `IRSSI_SRC` in Makefile if needed). This build includes Irssi headers directly from the source tree; building against system-wide installed Irssi headers is not yet wired up.
- Build:

```
make
```

Install

```
make install PREFIX=$HOME/.irssi/modules
```

Usage in Irssi

- Load the module:

```
/load awl
```

- Add the item to a statusbar (example adds to "window" bar on the right):

```
/statusbar window add -after act -priority 10 awl
```

Settings (subset)

- `awl_hide_data` (int, default 0): hide windows with data_level below this value (1=text, 2=msg, 3=hilight).
- `awl_hide_empty` (int, default 0): when >0, hides visible windows without items.
- `awl_prefer_name` (bool, default OFF): prefer window name over active item name.
- `awl_sort` (str, default "refnum"): one of `refnum`, `-data_level`, `-last_line`.
- `awl_separator` (str, default " "): string between entries.

Notes

- This is a minimal starting point. The original Perl script supports multi-line layout, external viewer mode, mouse interactions, and theme formats. Those can be incrementally ported on top of this.