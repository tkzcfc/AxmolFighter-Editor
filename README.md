# axmol_editor

`tools/axmol_editor` is a minimal Axmol + ImGui editor shell. It keeps the
application framework, docking layout, document registry, and a few starter
panels so new editor features can be rebuilt from a clean base.

## Build

Run from `tools/axmol_editor`:

```bat
build.bat
```

The script uses the existing Axmol build flow:

```bat
axmol build -p win32
```

## Run

Run from `tools/axmol_editor`:

```bat
run.bat RelWithDebInfo
```

If no build config is provided, `run.bat` uses the cached value in
`run.bat.txt` or prompts for one.

## Current Scope

- Axmol application bootstrap.
- ImGui docking shell and resettable default layout.
- Main menu with basic file actions and layout reset.
- Document registry, close confirmation, and text document editing.
- Starter `Content`, `Scene Hierarchy`, `Inspector`, and `Assets` panels.
