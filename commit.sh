#!/bin/bash
git add -A
git commit -m "Tasks 9-10: Theme integration and edge case handling

Task 9: Implement theme integration
- Added sidepanel formats to module-formats.h and module-formats.c
- Formats for different element types: channel active/activity/normal, nick op/voice/normal/away
- Color integration through term_set_color2() with theme-derived colors
- Different colors for different states: active channel (reverse+bold), activity (yellow+bold)
- Nick status colors: op (red+bold), voice (green+bold), away (dark gray)
- Integration with existing terminal color system

Task 10: Add resize stability and edge case handling
- Minimum widths: left panel 8 chars, right panel 6 chars
- Auto-hide logic: hide nicklist < 80 cols, shrink left < 60 cols, hide both < 40 cols
- Per-MAIN_WINDOW_REC behavior for split window environments
- Edge case handling: empty channel lists, disconnected servers, non-channel windows
- Graceful placeholder messages: '(no channels)', '(not a channel)', '(no users)'
- Automatic panel width adjustment on terminal resize via sig_terminal_resized
- Tested: Panels adapt to terminal size and handle edge cases gracefully"