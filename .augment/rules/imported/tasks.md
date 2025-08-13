---
type: "always_apply"
---

# Implementation Plan

- [x] 1. Implement native column reservation API
  - Add public `mainwindow_set_statusbar_columns(MAIN_WINDOW_REC *mw, int left, int right)` function
  - Extend `MAIN_WINDOW_REC` with `left_panel_win` and `right_panel_win` fields
  - Implement TERM_WINDOW creation/move/destroy logic with `mw->size_dirty = TRUE` marking
  - Integrate with existing `mainwindow_create_screen()` and `mainwindow_set_screen_size()` macros
  - _Requirements: 7.1, 7.2, 7.4_

- [x] 2. Create TERM_WINDOW panel management system
  - Implement panel TERM_WINDOW creation in `mainwindow_create()` and resize handlers
  - Add `term_window_create()` calls for left/right panels in reserved column areas
  - Implement `term_window_move()` and `term_window_destroy()` for panel lifecycle
  - Integrate panel creation/destruction with existing mainwindow lifecycle
  - _Requirements: 7.1, 7.4_

- [x] 3. Implement core panel context management system
  - Create SP_MAINWIN_CTX structure for selection and scroll state per main window
  - Implement hash table mapping between main windows and panel contexts
  - Add content caching system for dirty checking and efficient redraws
  - Write context creation, destruction, and cleanup functions
  - _Requirements: 7.1, 7.4_

- [x] 4. Integrate with Irssi's signal system for updates (avoid dirty_check integration)
  - Connect to `"terminal resized"`, `"mainwindow resized/moved"` signals for panel updates
  - Register handlers for `"window changed"`, `"window created"`, `"window destroyed"` signals
  - Add handlers for `"window activity"` and `"window refnum changed"` signals
  - Implement signal handlers for nicklist changes: `"nicklist changed"`, `"nick mode changed"`
  - Ensure per-MAIN_WINDOW_REC context handling for split window support
  - _Requirements: 1.2, 1.4, 3.1, 3.2, 7.2_

- [ ] 5. Implement settings system integration
  - Add configuration settings: `sidepanel_left_width`, `sidepanel_right_width`, `sidepanels_enabled`
  - Implement `read_settings()` function with sensible defaults (left: 14, right: 16)
  - Connect settings changes to column reservation and panel recreation
  - Add `/panel` commands: `/panel left <n>`, `/panel right <n>`, `/panel on/off`
  - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [x] 6. Implement channel list data integration
  - Create channel list population from active server channels
  - Implement real-time updates when channels are joined/parted
  - Add activity level tracking from WI_ITEM_REC data_level
  - Implement channel list rendering with proper formatting
  - _Requirements: 1.1, 1.4_

- [x] 7. Implement nicklist data integration
  - Create nicklist population from active channel members
  - Add proper mode prefix handling (@, +, etc.)
  - Implement real-time updates for nick changes and mode changes
  - Add away status indicators for nicknames
  - _Requirements: 1.1, 1.4_

- [x] 8. Create panel rendering system with direct term_* API (no textbuffer_view)
  - Implement `sidepanels_redraw_left()` using `term_move()`, `term_addstr()`, `term_set_color2()`
  - Implement `sidepanels_redraw_right()` with proper TERM_WINDOW targeting
  - Draw directly to panel TERM_WINDOW objects, avoid textbuffer_view complexity
  - Use `term_clrtoeol()` for proper line clearing and `term_refresh()` for updates
  - Implement content caching for dirty checking to avoid unnecessary redraws
  - _Requirements: 1.1, 3.2, 3.3_

- [ ] 9. Implement theme integration
  - Connect panel rendering to Irssi's theme system through `gui_printtext_get_colors()`
  - Add format definitions for panel elements in `module-formats.c` and `module-formats.h`
  - Implement themed colors using existing `term_set_color2()` with theme-derived colors
  - Integrate with `themes.h` system for consistent appearance with current theme
  - Use `gui_printtext_internal()` for formatted text rendering where appropriate
  - _Requirements: 4.1, 4.2, 4.3, 4.4_

- [ ] 10. Add resize stability and edge case handling
  - Implement minimum widths: left panel 8 chars, right panel 6 chars
  - Add auto-hide logic: hide nicklist < 80 cols, shrink left < 60 cols, hide both < 40 cols
  - Handle edge cases like empty channel lists and disconnected servers
  - Ensure per-MAIN_WINDOW_REC behavior for split window environments
  - _Requirements: 3.1, 3.2, 6.1, 6.2, 6.3, 6.4_

- [ ] 11. Implement selection and scroll state management
  - Add selection tracking for both left and right panels in SP_MAINWIN_CTX
  - Implement scroll offset management for long channel/nick lists
  - Preserve selection state across panel refreshes and resizes
  - Add visual selection highlighting in panel rendering
  - _Requirements: 2.1, 2.2_

- [ ] 12. Create comprehensive error handling
  - Add fallback mechanisms for terminal window creation failures
  - Implement proper cleanup for partial initialization states
  - Add validation for mouse sequence parsing
  - Handle stale server/channel references gracefully
  - _Requirements: 6.1, 6.2, 6.3, 6.4_

- [ ] 13. Write integration tests for resize behavior
  - Test panel layout under various terminal dimensions
  - Verify proper column reservation and TERM_WINDOW positioning
  - Test resize signal handling and panel repositioning
  - Validate UI stability during resize operations
  - _Requirements: 1.2, 3.1, 3.2_

- [ ] 14. Write integration tests for mouse interaction
  - Test click-to-switch functionality across different panel elements
  - Verify coordinate mapping accuracy for various terminal sizes
  - Test click-to-query functionality with proper query window creation
  - Validate mouse sequence parsing with different terminal types
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 15. Write integration tests for data updates
  - Test real-time channel list updates during join/part events
  - Verify nicklist updates during nick changes and mode changes
  - Test activity level tracking and visual indicators
  - Validate panel content refresh without UI corruption
  - _Requirements: 1.4, 3.3_

- [ ] 16. Implement configuration persistence through settings
  - Add validation for panel width settings within reasonable bounds (min/max limits)
  - Implement sensible defaults: left 14 chars, right 16 chars, enabled by default
  - Use settings system for persistence (defer windows-layout integration to later phase)
  - Test configuration persistence across application restarts
  - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [ ] 17. Integrate with main TUI lifecycle and initialization
  - Add sidepanels initialization to `textui_finish_init()` sequence
  - Ensure proper integration with `dirty_check()` redraw cycle
  - Connect to `"irssi init finished"` signal for final setup
  - Implement proper cleanup in `textui_deinit()` sequence
  - _Requirements: 7.1, 7.2, 7.4_

- [ ] 18. Implement mouse interaction system
  - Add SGR 1006 mouse tracking support to `term-terminfo.c` with `term_mouse` setting
  - Implement mouse sequence parsing and new `"gui mouse event"` signal emission
  - Add coordinate hit-testing for panel elements using TERM_WINDOW geometry
  - Implement click-to-switch functionality for channel list using existing window APIs
  - Add click-to-query functionality for nicklist through signal emission
  - Add mouse wheel scrolling support for long channel/nick lists
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 19. Add final polish and optimization
  - Optimize rendering performance for large channel/nick lists using content caching
  - Implement smooth scrolling and selection animations using terminal capabilities
  - Add accessibility features and proper focus management
  - Create comprehensive documentation for the feature
  - _Requirements: 1.1, 2.1, 3.2, 4.1_