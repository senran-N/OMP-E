<p align="center">
  <img src="assets/icons/omp_desktop_256.png" width="96" height="96" alt="OMP Desktop icon" />
</p>

<h1 align="center">OMP Desktop</h1>

<p align="center">
  Native C++20 GPU desktop client for <a href="https://github.com/can1357/oh-my-pi">oh-my-pi (<code>omp</code>)</a>.<br />
  Built on <a href="https://github.com/senran-N/Etch">Etch</a>, <a href="https://github.com/senran-N/Inlay">Inlay</a>, SDL3 GPU, HarfBuzz, and Slug.
</p>

<p align="center">
  <a href="https://github.com/senran-N/OMP-E/releases/latest">
    <img src="https://img.shields.io/github/v/release/senran-N/OMP-E?style=flat-square" alt="Latest Release" />
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" alt="MIT License" />
  </a>
</p>

---

## Highlights

- **Pure C++20 Native GPU UI**: Zero Electron, zero Chromium, zero WebView. Crisp vector typography via HarfBuzz + Slug GPU shaders, low memory footprint, and smooth native window chrome (including Windows 11 Snap Layouts & optional Mica backdrop).
- **Full OMP RPC v2 Client**: Real-time token streaming, chunked payload reassembly, slash commands, steering & follow-up queues, OAuth/custom provider management (`models.yml`), and model role/thinking effort (`Off` through `XHigh`) controls.
- **Split-Pane Workbench**:
  - **Changes & Terminal**: Structured diff ledger with line numbers, intra-line word emphasis, and ANSI terminal output.
  - **Agent Hub**: Real-time subagent progress, active tool status, token/cost metrics, live steering, and cancellation.
  - **Session Branch Tree**: Interactive DAG canvas for inspecting, switching, summarizing, and forking conversation branches.
  - **`/btw` Lane**: Side questions against active thread context without polluting main conversation history.
- **Typography & Mineral Palettes**: Four curated OKLCH themes (*Obsidian Brass*, *Alabaster Ink*, *Celadon Slate*, *Nordic Linen*), live UI/monospace font switcher (bundled + OS-installed fonts), and instant zoom (`Ctrl +` / `Ctrl -` / `Ctrl 0`).
- **Extensible Internationalization (i18n)**: Bundled `en-US` and `zh-CN` catalogs with BCP-47 locale negotiation and drop-in user language packs in `~/.omp/desktop/locales/`.
- **Built-in Software Updates**: Quiet GitHub Releases update checks with native installer download and SHA-256 verification.

## Installation

### Windows (Recommended)

Download and run the latest installer (`OMP-Desktop-<version>-windows-x64-setup.exe`) from the [GitHub Releases](https://github.com/senran-N/OMP-E/releases/latest) page.

> **Prerequisite**: Install [`omp`](https://github.com/can1357/oh-my-pi) and ensure `omp` is on your `PATH` (or configure its executable path under **Settings → Engine & Logs**).

## Building from Source

### Requirements

- CMake 3.24+
- Ninja (recommended)
- C++20 compiler (MSVC 2022 / Clang 16+ / GCC 13+)
- Git (CMake automatically fetches `Etch` and `Inlay` if sibling checkouts are not present)

### Build (Windows x64)

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

To build the Windows installer (requires [Inno Setup 6](https://jrsoftware.org/isinfo.php)):

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DOMPE_RELEASE_BUILD=ON
cmake --build build-release --target omp_desktop
iscc /DAppVersion=0.1.0 /DBuildDir=..\..\build-release packaging\windows\omp_desktop.iss
```

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl + K` | Open Command Palette |
| `Ctrl + N` | New Thread |
| `Ctrl + B` | Toggle Sidebar |
| `Ctrl + ,` | Open Settings |
| `Ctrl + Shift + T` | Cycle Thinking Effort |
| `Alt + W` / `A` / `T` / `B` | Toggle Workbench (Changes / Agents / Tree / `/btw`) |
| `Ctrl + +` / `-` / `0` | Increase / Decrease / Reset Text Size |
| `F11` | Toggle Fullscreen |

## Versioning

OMP Desktop follows [Semantic Versioning](https://semver.org/) and [Keep a Changelog](CHANGELOG.md). Releases are versioned conservatively: `0.x.y` during early milestones, with `v0.1.0` as the initial public release.

## License

MIT License — see [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
