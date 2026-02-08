<div align="center">

![GhostCAT Logo](ghostcat.svg)

# 👻 GhostCAT
### **G**host **C**ursor **A**ssist **T**oolchain

*A Linux gaming mouse configuration tool forked from libratbag and Piper*

---

### 🚧 **UNDER CONSTRUCTION** 🚧
*This project is in active development. Features, documentation, and installation instructions are being added regularly.*

---

</div>

## 🙏 Special Thanks to the GitHub Community!

GhostCAT would not exist without the incredible work of the **libratbag** and **Piper** communities. This project is a fork and enhancement of their exceptional open-source tools.

**Thank you to:**
- **Red Hat, Inc.** - creators of libratbag and Piper
- **David Herrmann** - original libratbag author
- All libratbag and Piper contributors

See [ATTRIBUTIONS.md](ATTRIBUTIONS.md) for full copyright and attribution details.

---

## 🐱 The Ghost Cat Story

In early 2024, I (Alexander Weinhart) accidentally ran over a cat. When I started building this mouse configuration tool, I wanted to honor that cat. The name serves as a memorial.

The acronym **C**ursor **A**ssist **T**oolchain came later, but the ghost cat was always meant to be the heart of this project.

RIP. The mice aren't safe in Heaven.

---

## 🎯 What is GhostCAT?

GhostCAT is a **unified gaming mouse configuration application** for Linux that combines:

- **libratbag** - The C library and daemon that communicates with gaming mice via HID++ 2.0 and other protocols
- **Piper** - The GTK graphical interface for configuring buttons, DPI, LEDs, and more

### Key Features

✨ **Complete DPI Control**
- Configure all 5 DPI resolution slots
- Set active DPI and DPI shift target
- Real-time active DPI monitoring

🖱️ **Button Remapping**
- Remap any button to keyboard keys, macros, special functions
- Support for modifier keys (Ctrl, Alt, Shift)
- Profile-based configurations

💡 **LED Customization** *(Coming Soon)*
- Configure RGB lighting zones
- Real-time color preview
- Per-zone control for supported mice

🎨 **Modern Interface** *(Coming Soon)*
- GTK4-based UI with smooth animations
- Light/dark/system theme support
- Visual mouse diagrams for all supported mice

🔌 **Modular Mouse Support** *(Coming Soon)*
- Downloadable mouse configuration modules
- Community-contributed mouse profiles
- Easy integration of new mice

---

## 🖥️ Supported Mice

GhostCAT supports 100+ gaming mice out of the box, including:

### Logitech
- G502 (HERO, Proteus Core, X, X Wireless)
- G Pro / G Pro Wireless / G Pro X Superlight
- G903 / G900 (HERO)
- G403 / G703 (HERO)
- G305 / G304
- G600 / G602 / G603 / G604
- MX Master (2S, 3, 3S)
- MX Anywhere (2, 2S, 3, 3S)
- And many more...

### ASUS ROG
- Chakram / Chakram X / Chakram Core
- Keris Wireless / Keris Wireless AimPoint
- Gladius II / Gladius II Origin
- Strix Impact / Strix Impact II
- And more...

### Other Brands
- Glorious Model O / Model D
- Roccat Kone series
- SteelSeries Rival series
- Sinowealth generic mice
- And many more...

**Full list:** See `libratbag/data/devices/` directory

---

## 📦 Installation

### 🚧 Coming Soon

Installation instructions for the following distributions will be added in a future commit:

- Ubuntu / Debian
- Fedora / RHEL
- Arch Linux
- openSUSE
- Manual build from source

**Current Status:** You can build from source following the standard libratbag + Piper build process, but this will change as we complete the GhostCAT rebrand.

---

## 🛠️ Building from Source *(Current State)*

**Note:** These instructions are for the pre-rebrand state. They will change significantly.

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt install meson ninja-build libglib2.0-dev libudev-dev libevdev-dev \
                 python3-gi python3-gi-cairo gir1.2-gtk-3.0 gir1.2-rsvg-2.0

# Fedora
sudo dnf install meson ninja-build glib2-devel systemd-devel libevdev-devel \
                 python3-gobject gtk3 librsvg2
```

### Build libratbag
```bash
cd libratbag
meson setup builddir
ninja -C builddir
sudo ninja -C builddir install
sudo systemctl restart ghostcatd
```

### Build GhostCAT
```bash
cd ghostcat
meson setup builddir
ninja -C builddir
sudo cp builddir/data/ghostcat.gresource /usr/local/share/ghostcat/ghostcat.gresource
```

### Run
```bash
cd ghostcat
unset LD_LIBRARY_PATH
PYTHONPATH=. python3 builddir/ghostcat
```

---

## 🎨 What Makes GhostCAT Different?

### Enhancements Over Upstream

🔹 **Complete DPI Shift Target Support**
- Full implementation from C library through D-Bus to UI
- Real-time hardware polling every 2 seconds
- Visual indicators for active DPI and shift target

🔹 **Unified DPI Interface**
- All 5 DPI slots in a single intuitive list
- No more hidden shift target confusion
- Clear "Set Active" and "Set Shift" buttons

🔹 **Improved Button Mapping**
- Fixed single modifier key support (Ctrl/Alt/Shift alone)
- Dynamic button label updates
- Better visual feedback

🔹 **Better Architecture**
- Centralized state management
- Proper D-Bus cache bypassing for real data
- Memory leak prevention

🔹 **Future-Ready** *(Coming Soon)*
- GTK4 migration for modern UI
- Animated page transitions
- LED color visualization
- Modular mouse support system

---

## 🗺️ Roadmap

### Phase 1: Foundation ✅ *COMPLETE*
- [x] DPI shift target implementation (C stack)
- [x] D-Bus exposure and polling
- [x] Python client integration
- [x] UI overhaul with unified DPI interface
- [x] Modifier key bugfix

### Phase 2: Rebrand 🚧 *IN PROGRESS*
- [x] License compliance and attribution
- [ ] Complete code rename (ratbag → ghostcat)
- [ ] Repository restructure
- [ ] Documentation updates
- [ ] Build system updates

### Phase 3: Features 📋 *PLANNED*
- [ ] GTK4 migration
- [ ] Animated UI elements
- [ ] Theme support (light/dark/system)
- [ ] LED color visualization
- [ ] Real-time LED preview
- [ ] Remove double-click from UI

### Phase 4: Modularity 🔮 *FUTURE*
- [ ] Mouse module system (.gcmod format)
- [ ] Community module repository
- [ ] In-app module browser
- [ ] Third-party mouse support

---

## 🤝 Contributing

GhostCAT is open source and welcomes contributions!

**How to contribute:**
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

**Areas we need help:**
- 🐛 Bug testing on various mice
- 🎨 UI/UX improvements
- 📝 Documentation
- 🖱️ Adding support for new mice
- 🌍 Translations

---

## 📜 License

**GhostCAT** is licensed under **GPL-2.0-or-later** in compliance with Piper's original license.

- **libratbag portions:** MIT License
- **Piper portions:** GPL-2.0-or-later
- **GhostCAT modifications:** GPL-2.0-or-later

See [LICENSE](LICENSE), [LICENSE.MIT](LICENSE.MIT), and [ATTRIBUTIONS.md](ATTRIBUTIONS.md) for full details.

---

## 📚 Documentation

- [CHANGELOG.md](CHANGELOG.md) - Complete modification history
- [ATTRIBUTIONS.md](ATTRIBUTIONS.md) - Copyright and attribution details
- [libratbag Documentation](libratbag/doc/) - C library API docs
- [D-Bus Interface Documentation](libratbag/doc/dbus.rst) - D-Bus API reference

---

## 🐛 Bug Reports & Support

**Found a bug?** [Open an issue](https://github.com/your-username/ghostCAT-Linux-Mouse/issues) *(link coming soon)*

**Need help?** Check the documentation first, then open an issue.

---

## 🌟 Acknowledgments

Special thanks to:
- **The libratbag team** for creating the robust HID++ 2.0 implementation
- **The Piper team** for the elegant GTK interface and SVG visualization system
- **The Linux gaming community** for supporting open-source peripheral tools
- **All contributors** who improve FOSS software

---

<div align="center">

**Made with ❤️ by Alexander Weinhart**

*In memory of a ghost cat*

---

**GPL-2.0-or-later** | [Report Issues](https://github.com/your-username/ghostCAT-Linux-Mouse/issues) | [Contribute](CONTRIBUTING.md)

</div>
