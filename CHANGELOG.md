# Changelog

All notable changes to OMP Desktop are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.1] - 2026-11-22

### Fixed
- **Small Text Solid-Block Rendering Under Long Threads**: Fixed an `Etch` render-batcher fallback (`optimize_render_batches`) where frame-arena exhaustion routed small hinted `PrimKind::Glyph` primitives into the `Uber` pipeline (sampling the solid-white fallback texture instead of the ClearType subpixel atlas), turning `<= 24px` text into solid color blocks in long conversations.
- **Missing Random Characters in Long Conversations**: Fixed transient frame-arena exhaustion in `Etch` (`get_or_cache_hinted_glyph` and `slug_get_glyph`) permanently caching empty/failed glyph entries, fixed an un-rewound per-CJK-cluster `frame_arena` allocation in `pick_fallback_slot_for_cluster`, and fixed a subpixel atlas boundary-growth bug in `ensure_subpixel_atlas`.
- **Headroom for Massive Conversations**: Increased virtual `frame_arena_bytes` reservation to 16 MB (demand-paged with zero extra idle RAM) and initial primitive capacity to 32,768.

## [0.1.0] - 2026-11-22

Initial public release of OMP Desktop — a native C++20 GPU desktop client for `oh-my-pi` (`omp`).

### Added
- **Native GPU Desktop Shell**: Pure C++20 application built on Etch, Inlay, and SDL3 GPU with custom frameless titlebar, Windows Snap Layouts (`HTMAXBUTTON`), Mica backdrop support, and crisp multi-resolution astrolabe crest icon.
- **Full OMP RPC v2 Integration**: Real-time streaming chat, delta updates, chunk reassembly, slash commands, prompt steering & follow-up queues, session switching/forking, and live voice toggle.
- **Live Subagent Hub**: Real-time tracking of background subagent lifecycle, progress metrics, active tool indicators, steering, and cancellation (`subagent_lifecycle`, `subagent_progress`, `subagent_event`, and synchronous `task` progress).
- **Split-Pane Workbench**: Side-by-side structured diff inspector (with line numbers and intra-line emphasis), ANSI terminal output viewer, interactive Session Branch Tree DAG with jump/fork/summarize actions, and `/btw` side-question lane.
- **Thoughtful Reading & Thinking Trace Ergonomics**: Independent per-message disclosure state, bounded scrollable container for long thinking traces with one-click bottom collapse and full-expansion toggles, and smart stick-to-bottom auto-scroll that yields immediately on user scroll-up with a floating "Jump to latest" pill.
- **Curated Themes & Live Font Selection**: Four OKLCH mineral palettes (*Obsidian Brass*, *Alabaster Ink*, *Celadon Slate*, *Nordic Linen*), live font-size scaling (`Ctrl +` / `Ctrl -` / `Ctrl 0`), and live UI/monospace font switching across bundled and OS-installed fonts.
- **Extensible Internationalization (i18n)**: External JSON locale catalogs (`en-US` and `zh-CN` bundled) with BCP-47 locale negotiation, automatic fallback chains, and drop-in user language packs under `~/.omp/desktop/locales/`.
- **Settings Center**: Unified configuration for General, OAuth & custom `models.yml` Providers, Model Roles & Thinking Effort (`Off` through `XHigh`), Appearance & Typography, Tool Approval Safety Policy, MCP/Skills ecosystem, and Engine Wire Diagnostics.
- **Built-in Software Updates**: Quiet startup check and manual GitHub Releases update flow with native installer download, SHA-256 verification via Windows CNG (`BCrypt`), and one-click installer launch.
- **Windows Installer**: Standard Inno Setup installer (`OMP-Desktop-0.1.0-windows-x64-setup.exe`) with Start Menu/Desktop shortcuts, clean uninstallation, and `SHA256SUMS.txt` integrity manifest.
