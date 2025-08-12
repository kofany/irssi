#!/bin/bash
git add -A
git commit -m "Tasks 6-8: Complete data integration and panel rendering

Task 6: Implement channel list data integration
- ChannelListItem structure with name, server, window, item, activity_level, is_active
- populate_channel_list() and clear_channel_list() functions
- Real-time updates via window signals: created/destroyed/changed/activity/refnum changed
- Activity level tracking from WI_ITEM_REC->data_level
- Integration with window item signals: new/remove/changed

Task 7: Implement nicklist data integration  
- NicklistItem structure with nick, prefix, level, is_away, nick_rec
- populate_nicklist() and clear_nicklist() functions
- Data collection from nicklist_getnicks() for active channel
- Proper mode prefix handling (@, %, +) and level assignment
- Away status indicators from nick->gone
- Real-time updates via nicklist changed and nick mode changed signals
- Channel-only display (not for queries) with IS_CHANNEL() check

Task 8: Create panel rendering system with direct term_* API
- sidepanels_redraw_left() and sidepanels_redraw_right() functions
- Direct term_* API usage: term_move(), term_addstr(), term_set_color2(), term_clrtoeol()
- Selection highlighting with ATTR_REVERSE and ATTR_BOLD for activity
- Flicker-free updates with term_refresh_freeze()/thaw()
- Content caching with dirty checking for performance
- Text truncation for long channel/nick names
- Integration with signal handlers for automatic redraw
- Tested: Panels are rendered and three-column layout is visible"