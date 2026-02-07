# 📝 Changelog

All notable changes to GhostCAT since forking from libratbag and Piper.

---

## [Unreleased] - 2026-02-07

### 🎯 Initial Fork - DPI Shift Target Implementation

This represents the complete state of modifications before the GhostCAT rebrand.

---

## Phase 1: DPI Shift Target Infrastructure (libratbag C Stack)

### Added - Core Library Features
- **`is_dpi_shift_target` field** added to `ratbag_resolution` struct for tracking which DPI slot is the shift target
- **`ratbag_resolution_is_dpi_shift_target()`** - Public getter function for shift target status
- **`ratbag_resolution_set_dpi_shift_target()`** - Public setter that atomically unsets all other resolutions' shift target flags and marks device dirty
- **`ratbag_device_refresh_active_resolution()`** - New public API for polling active DPI from hardware in real-time
- **`ratbag_action_is_single_modifier_key()`** - Helper function to identify single modifier key actions

### Added - HID++ 2.0 Driver Implementation
- **`hidpp20drv_refresh_active_resolution()`** - Driver implementation that queries hardware via `hidpp20_onboard_profiles_get_current_dpi_index()` and updates `is_active` flags across all resolutions
- **DPI index fallback handling** - Improved `switched_dpi` validation with bounds checking
- **Shift target initialization** - `hidpp20drv_read_profile_8100()` now reads `p->switched_dpi` field to set initial `is_dpi_shift_target` on correct resolution during device enumeration

### Added - D-Bus Exposure (ratbagd)
- **`IsDpiShiftTarget` property** on `org.freedesktop.ratbag1.Resolution` interface
- **`SetDpiShiftTarget` method** on `org.freedesktop.ratbag1.Resolution` interface
- **`ratbagd_device_poll_active_resolution()`** - Device-level polling function
- **2-second polling timer** - Background timer in `ratbagd.c` calls `ratbag_device_poll_active_resolution()` on all connected devices and emits `PropertiesChanged` signals when active DPI changes
- **Idle exit disabled** - Daemon now stays running to maintain polling loop

### Changed - Modified Files (libratbag)
- `src/driver-hidpp20.c` (+54 lines, -5 lines) - DPI index validation, shift target init, refresh implementation, driver vtable entry
- `src/libratbag.c` (+74 lines) - Shift target getter/setter, device refresh, modifier key helper, modifier key bugfix
- `src/libratbag.h` (+38 lines) - Public API declarations
- `src/libratbag-private.h` (+9 lines) - Private struct field, driver callback definition
- `ratbagd/ratbagd-resolution.c` (+49 lines) - D-Bus property/method implementation, vtable entries
- `ratbagd/ratbagd-device.c` (+16 lines) - Device polling function
- `ratbagd/ratbagd.c` (+38 lines, -2 lines) - Idle exit disable, polling timer setup
- `ratbagd/ratbagd.h` (+1 line) - Function declaration

**Total:** 8 files changed, +269 lines, -10 lines

---

## Phase 2: Piper D-Bus Client (ratbagd.py)

### Added - Python Client Features
- **`_dpi_shift_target` property** tracking in `RatbagdResolution.__init__()`
- **`is_dpi_shift_target` GObject property** exposed to UI layer
- **`set_dpi_shift_target()` method** calling D-Bus `SetDpiShiftTarget` method
- **`IsDpiShiftTarget` property change handler** in `_on_properties_changed()`
- **`self.notify("special")` fix** in `RatbagdButton._on_properties_changed()` - was missing, prevented button label updates from triggering UI refreshes

### Changed - Modified Files (Piper)
- `piper/ratbagd.py` (+23 lines) - D-Bus client implementation

**Total:** 1 file changed, +23 lines

---

## Phase 3: Resolution UI Overhaul (Piper GUI)

### Added - ResolutionRow.ui Template
- **`shift_label`** - "Shift" text indicator (visible=False by default, position 3)
- **`apply_label`** - "Click Apply" prompt (visible=False by default, position 4)
- **`shift_button`** - GtkButton for setting shift target (NEW feature)
- **`active_label`** repositioned to position 2 for consistent layout

### Added - resolutionrow.py Controller
- **`shift_button` template child** binding
- **`shift_label` template child** binding
- **`_on_shift_button_clicked()` handler** - Calls `set_dpi_shift_target()` on resolution, triggers `resolutions_page._update_shift_target()`
- **Enhanced `_on_disable_button_toggled()`** - Shows `apply_label` after status change when enabling a resolution
- **Improved `_on_status_changed()`** - Delegates button/label state management to parent page's centralized `_apply_button_states()` method

### Added - ResolutionsPage.ui Template
- **`shift_target_frame`** with `shift_listbox` (present in XML but hidden at runtime - reserved for future use)
- **Removed `add_resolution_row`** - The defunct "+" row that did nothing

### Added - resolutionspage.py Controller (COMPLETE REWRITE)
- **Unified single listbox** - All 5 DPI resolutions in ONE list (no separate shift listbox)
- **`_active_index` tracking** - Tracks which resolution is currently active on hardware
- **`_shift_target_index` tracking** - Tracks which resolution is designated as shift target
- **`_apply_button_states()` method** - Centralized button/label state controller implementing complex state machine:
  - Active profile: `active_label` visible on active, `active_button` greyed on active/disabled, `disable_button` greyed on active, `shift_button` greyed on shift/disabled
  - Inactive profile: `active_label` hidden, `active_button` greyed, `disable_button` enabled, `shift_button` enabled per shift status
  - `shift_label` visible only on shift target resolution
- **`_poll_active_from_dbus()` method** - Bypasses cache by making fresh D-Bus `Get` calls to all 5 resolutions, updates `_active_index` when hardware state changes
- **`_on_destroy()` cleanup handler** - Stops polling timer to prevent memory leaks
- **`_populate_listboxes()` enhancement** - Inserts rows at position 0 (reverses order for visual consistency), calls `show_all()` then manually re-hides `apply_label`
- **Dynamic button label updates** - Listens to `notify::action-type` and `notify::special` signals on all mouse buttons, updates mousemap labels when button actions change in real-time

### Added - mousemap.py Rendering Fix
- **Label position clamping** - Clamps leader line X coordinates to SVG bounds to prevent labels from extending off-screen

### Changed - Modified Files (Piper)
- `piper/resolutionspage.py` (+215 lines, -6 lines) - Complete architectural rewrite
- `piper/resolutionrow.py` (+62 lines, -25 lines) - New handlers, refactored status logic
- `data/ui/ResolutionRow.ui` (+55 lines, -2 lines) - New UI elements
- `data/ui/ResolutionsPage.ui` (+78 lines, -26 lines) - Layout restructure
- `piper/mousemap.py` (+8 lines, -1 line) - Rendering bugfix

**Total:** 5 files changed, +357 lines, -84 lines

---

## Phase 4: Modifier Key Bugfix (libratbag.c)

### Fixed
- **Single modifier key mapping** - Fixed `ratbag_action_keycode_from_macro()` to properly handle single modifier keys (Ctrl, Alt, Shift alone) that were incorrectly rejected by `num_keys != 1` validation check
- Added `ratbag_action_is_single_modifier_key()` helper to identify modifier-only button mappings

### Changed - Modified Files
- `src/libratbag.c` (included in Phase 1 totals)

**Total:** 1 file changed (already counted above)

---

## 📊 Overall Statistics

### libratbag Changes
- **8 files modified**
- **+269 lines added**
- **-10 lines removed**

### Piper Changes
- **6 files modified**
- **+357 lines added**
- **-84 lines removed**

### Grand Total
- **14 files modified**
- **+626 lines added**
- **-94 lines removed**
- **Net: +532 lines**

---

## 🐛 Bugs Fixed During Development

### Bug: Active label disappearing on DPI button press
- **Cause:** Firmware cycled to shift target DPI slot which was hidden in earlier code
- **Fix:** Merged all 5 DPI slots into unified list, added "Set Shift" button
- **Lesson:** Don't fight hardware behavior - expose it to the user

### Bug: `show_all()` making hidden labels visible
- **Cause:** `row.show_all()` overrides `visible=False` on template widgets
- **Fix:** Manually re-hide widgets after `show_all()` call
- **Lesson:** Always re-hide intentionally hidden widgets after `show_all()`

### Bug: "Click Apply" label not appearing when enabling
- **Cause:** `_on_disable_button_toggled()` showed label, then `_apply_button_states()` immediately hid it
- **Fix:** Show `apply_label` AFTER status update; `_apply_button_states()` does NOT touch `apply_label`
- **Lesson:** Separate transient UI hints from state-driven UI elements

### Bug: Button labels not updating when actions changed
- **Cause:** `RatbagdButton._on_properties_changed()` missing `self.notify("special")` call
- **Fix:** Added the notify call to propagate property changes to UI
- **Lesson:** Always emit property change notifications in GObject properties

### Bug: ModuleNotFoundError when running from builddir
- **Fix:** Set `PYTHONPATH=. python3 builddir/piper`

### Bug: Snap library conflict
- **Fix:** `unset LD_LIBRARY_PATH` before running Piper

### Bug: Stale gresource after UI changes
- **Fix:** `rm builddir/data/piper.gresource && ninja -C builddir`, then copy to `/usr/local/share/piper/`

---

## 🔮 Planned Changes (Not Yet Implemented)

These changes are planned for future commits:

### Rebrand Phase
- [ ] Complete rename: `ratbag` → `ghostcat`, `ratbagd` → `ghostcatd`, `Piper` → `GhostCAT`
- [ ] D-Bus interface: `org.freedesktop.ratbag1` → `org.freedesktop.ghostcat1`
- [ ] Python package: `piper` → `ghostcat`
- [ ] All C function prefixes: `ratbag_` → `ghostcat_`
- [ ] systemd service: `ratbagd.service` → `ghostcatd.service`

### Feature Additions
- [ ] GTK4 migration
- [ ] Animated UI elements (page transitions slide in/out)
- [ ] Light/dark/system theme toggle
- [ ] Animation on/off toggle (in-app + CLI flag)
- [ ] LED color visualization on mouse SVGs
- [ ] Real-time LED preview in color settings
- [ ] Per-mouse LED zone research and implementation
- [ ] Modular mouse support (downloadable .zip configs)
- [ ] Remove double-click from UI (hide from ButtonDialog, keep enum)
- [ ] Consolidated monorepo structure

---

## 📜 License

All modifications are licensed under **GPL-2.0-or-later** in compliance with Piper's original license.

See [ATTRIBUTIONS.md](ATTRIBUTIONS.md) for full copyright information.
