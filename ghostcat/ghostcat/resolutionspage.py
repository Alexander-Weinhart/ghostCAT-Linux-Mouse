# SPDX-License-Identifier: GPL-2.0-or-later

from gettext import gettext as _
from typing import Dict, Optional

from .mousemap import MouseMap
from .ghostcatd import RatbagdButton, RatbagdDevice, RatbagdProfile, RatbagdResolution
from .resolutionrow import ResolutionRow
from .util.gobject import connect_signal_with_weak_ref

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gio, GLib, Gtk  # noqa


@Gtk.Template(resource_path="/org/freedesktop/GhostCAT/ui/ResolutionsPage.ui")
class ResolutionsPage(Gtk.Box):
    """The first stack page, exposing the resolution configuration with its
    report rate buttons and resolutions list."""

    __gtype_name__ = "ResolutionsPage"

    _resolution_labels = [
        RatbagdButton.ActionSpecial.RESOLUTION_CYCLE_UP,
        RatbagdButton.ActionSpecial.RESOLUTION_CYCLE_DOWN,
        RatbagdButton.ActionSpecial.RESOLUTION_UP,
        RatbagdButton.ActionSpecial.RESOLUTION_DOWN,
        RatbagdButton.ActionSpecial.RESOLUTION_ALTERNATE,
        RatbagdButton.ActionSpecial.RESOLUTION_DEFAULT,
    ]

    listbox: Gtk.ListBox = Gtk.Template.Child()  # type: ignore
    shift_listbox: Gtk.ListBox = Gtk.Template.Child()  # type: ignore
    shift_target_frame: Gtk.Frame = Gtk.Template.Child()  # type: ignore

    def __init__(
        self, ghostcatd_device: RatbagdDevice, profile: RatbagdProfile, *args, **kwargs
    ) -> None:
        """Instantiates a new ResolutionsPage.

        @param ratbag_device The ratbag device to configure, as
                             ghostcatd.RatbagdDevice
        """
        Gtk.Box.__init__(self, *args, **kwargs)

        self._device = ghostcatd_device
        self._last_activated_row: Optional[ResolutionRow] = None
        self._profile = profile
        self._button_labels: Dict[int, Gtk.Label] = {}
        self._resolution_rows: Dict[int, ResolutionRow] = {}
        self._mousemap = MouseMap("#Buttons", self._device, spacing=20, border_width=20)
        self.pack_start(self._mousemap, True, True, 0)
        # Place the MouseMap on the left
        self.reorder_child(self._mousemap, 0)

        # Add labels for resolution-related buttons and listen for changes
        for button in profile.buttons:
            connect_signal_with_weak_ref(
                self, button, "notify::action-type", self._on_button_changed
            )
            connect_signal_with_weak_ref(
                self, button, "notify::special", self._on_button_changed
            )
            self._update_button_label(button)
        self._mousemap.show_all()

        self._resolutions = profile.resolutions

        # Determine initial active resolution
        self._active_index = 0
        for resolution in self._resolutions:
            if resolution.is_active:
                self._active_index = resolution.index
                break

        # Determine initial shift target
        self._shift_target_index = -1
        for resolution in self._resolutions:
            if resolution.is_dpi_shift_target:
                self._shift_target_index = resolution.index
                break

        # Listen for profile active changes
        connect_signal_with_weak_ref(
            self, profile, "notify::is-active", self._on_profile_active_changed
        )

        # Listen for changes on each resolution
        for resolution in profile.resolutions:
            connect_signal_with_weak_ref(
                self, resolution, "notify::is-dpi-shift-target", self._on_shift_target_changed
            )
            connect_signal_with_weak_ref(
                self, resolution, "notify::is-active", self._on_active_changed
            )

        # Populate the listbox (all 5 resolutions in one list)
        self._populate_listboxes()

        # Hide the separate shift target frame — all resolutions are in one list now
        self.shift_target_frame.set_visible(False)

        # Poll active resolution from D-Bus every 2 seconds to detect
        # physical DPI button presses on the mouse
        self._poll_timer_id = GLib.timeout_add_seconds(2, self._poll_active_from_dbus)
        self.connect("destroy", self._on_destroy)

    def _on_button_changed(self, button: RatbagdButton, _pspec) -> None:
        """Called when a button's action changes."""
        self._update_button_label(button)
        self._mousemap.show_all()

    def _update_button_label(self, button: RatbagdButton) -> None:
        """Update or remove the label for a button based on its current action."""
        # Remove existing label if any
        if button.index in self._button_labels:
            old_label = self._button_labels.pop(button.index)
            self._mousemap.remove(old_label)

        # Add new label if button has a resolution-related action
        if (
            button.action_type == RatbagdButton.ActionType.SPECIAL
            and button.special in self._resolution_labels
        ):
            label = Gtk.Label(
                label=_(RatbagdButton.SPECIAL_DESCRIPTION[button.special])
            )
            self._mousemap.add(label, f"#button{button.index}")
            self._button_labels[button.index] = label

    @Gtk.Template.Callback("on_row_activated")
    def on_row_activated(self, _listbox: Gtk.ListBox, row: ResolutionRow) -> None:
        if row is self._last_activated_row:
            self._last_activated_row = None
            row.toggle_revealer()
        else:
            if self._last_activated_row is not None:
                self._last_activated_row.toggle_revealer()

            self._last_activated_row = row
            row.toggle_revealer()

    def _populate_listboxes(self) -> None:
        """Populate the listbox with all resolutions."""
        # Clear existing rows
        self.listbox.foreach(Gtk.Widget.destroy)
        self.shift_listbox.foreach(Gtk.Widget.destroy)

        # Clear row references
        self._resolution_rows.clear()

        # All resolutions go into one listbox (reversed so "up" moves up visually)
        for resolution in self._resolutions:
            row = ResolutionRow(resolution, self)
            self._resolution_rows[resolution.index] = row
            self.listbox.insert(row, 0)
            row.show_all()
            # show_all() makes apply_label visible — hide it
            row.apply_label.set_visible(False)

        # Apply active and shift target states
        self._apply_button_states()

    def _on_shift_target_changed(self, resolution: RatbagdResolution, _pspec) -> None:
        """Called when a resolution's shift target status changes."""
        if resolution.is_dpi_shift_target:
            self._shift_target_index = resolution.index
        self._apply_button_states()

    def _on_profile_active_changed(self, profile: RatbagdProfile, _pspec) -> None:
        """Called when the profile's active status changes."""
        self._apply_button_states()

    def _on_active_changed(self, resolution: RatbagdResolution, _pspec) -> None:
        """Called when a resolution's active status changes via ghostcatd signal."""
        if resolution.is_active:
            self._active_index = resolution.index
        self._apply_button_states()

    def _update_active_labels(self, active_index: int) -> None:
        """Update active labels when Set Active is clicked."""
        self._active_index = active_index
        self._apply_button_states()

    def _update_shift_target(self, shift_index: int) -> None:
        """Update shift target when Set Shift is clicked."""
        self._shift_target_index = shift_index
        self._apply_button_states()

    def _apply_button_states(self) -> None:
        """Apply active label, Set Active, Disable, and Set Shift button states."""
        profile_is_active = self._profile.is_active

        for res in self._resolutions:
            if res.index not in self._resolution_rows:
                continue
            row = self._resolution_rows[res.index]

            is_active = (res.index == self._active_index)
            is_shift = (res.index == self._shift_target_index)
            is_disabled = res.is_disabled

            if profile_is_active:
                row.active_label.set_visible(is_active)
                row.active_button.set_sensitive(not is_active and not is_disabled)
                row.disable_button.set_sensitive(not is_active)
                row.shift_button.set_sensitive(not is_shift and not is_disabled)
            else:
                row.active_label.set_visible(False)
                row.active_button.set_sensitive(False)
                row.disable_button.set_sensitive(True)
                row.shift_button.set_sensitive(not is_shift and not is_disabled)

            # Show shift label on current shift target
            row.shift_label.set_visible(is_shift)

    def _poll_active_from_dbus(self) -> bool:
        """Poll D-Bus for active resolution changes from physical button presses."""
        if not self._profile.is_active:
            return True  # Keep polling but skip work for inactive profiles

        try:
            for resolution in self._resolutions:
                result = resolution._proxy.call_sync(
                    "org.freedesktop.DBus.Properties.Get",
                    GLib.Variant("(ss)", (resolution._interface, "IsActive")),
                    Gio.DBusCallFlags.NO_AUTO_START,
                    500,
                    None,
                )
                is_active = result.unpack()[0]
                if is_active and resolution.index != self._active_index:
                    self._active_index = resolution.index
                    self._apply_button_states()
                    break
        except GLib.Error:
            pass  # Silently ignore D-Bus errors during polling

        return True  # Keep the timer running

    def _on_destroy(self, _widget) -> None:
        """Clean up the polling timer when the widget is destroyed."""
        if self._poll_timer_id:
            GLib.source_remove(self._poll_timer_id)
            self._poll_timer_id = 0
