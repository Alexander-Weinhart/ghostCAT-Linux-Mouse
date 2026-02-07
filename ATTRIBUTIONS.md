# 🙏 Attributions

GhostCAT is a derivative work of **libratbag** and **Piper**, two exceptional open-source projects that made Linux gaming mouse configuration possible.

---

## libratbag (MIT License)

**Original Authors & Copyright Holders:**
- Copyright © 2015-2017 Red Hat, Inc.
- Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
- Copyright © 2021 Kyoken <kyoken@kyoken.ninja>
- Additional contributors from the libratbag community

**Project:** https://github.com/libratbag/libratbag
**License:** MIT (see LICENSE.MIT)

libratbag provides the core C library and `ratbagd` daemon that communicates with gaming mice via HID++ 2.0 and other protocols. Without their foundational work, GhostCAT would not exist.

---

## Piper (GPL-2.0-or-later)

**Original Authors & Copyright Holders:**
- Copyright © 2016-2019 Red Hat, Inc.
- Additional contributors from the Piper community

**Project:** https://github.com/libratbag/piper
**License:** GPL-2.0-or-later (see LICENSE)

Piper provided the GTK3 graphical interface and D-Bus client implementation. The elegant SVG-based mouse visualization system and UI architecture form the foundation of GhostCAT's interface.

---

## GhostCAT Modifications

**Author:** Alexander Weinhart
**Copyright:** © 2026 Alexander Weinhart
**License:** GPL-2.0-or-later (see LICENSE)

All GhostCAT modifications, enhancements, and new features are licensed under GPL-2.0-or-later in compliance with the original Piper license.

**Major Modifications Include:**
- Complete DPI shift target implementation (C library, D-Bus, Python client, UI)
- Unified 5-slot DPI resolution interface
- Real-time active DPI polling system
- UI overhaul with new button states and labels
- Rebranding to GhostCAT (Ghost Cursor Assist Toolchain)
- Future: GTK4 migration, animated UI, LED visualization, modular mouse support

See CHANGELOG.md for detailed modification history.

---

## Special Thanks

To the entire libratbag and Piper communities for building robust, well-documented open-source tools. Your commitment to free software enables projects like GhostCAT to exist and flourish.

To the GPL and MIT licenses for ensuring this software remains free and modifiable for everyone.

---

**Full License Texts:**
- GPL-2.0-or-later: See LICENSE
- MIT License: See LICENSE.MIT
