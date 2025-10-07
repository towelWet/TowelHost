# TowelHost - Simple Audio Plugin Host

## ⚠️ CRITICAL WARNING ⚠️
**NEVER DELETE YOUR AUDIO UNIT CACHE**
- Location: `~/Library/Caches/AudioUnitCache/`
- Deleting this breaks plugin validation and causes failures
- If a plugin won't load, the issue is with the plugin itself, NOT the cache

---

TowelHost is a lightweight, single-plugin host application for macOS Audio Units (AU). It provides a simple way to test and use Audio Unit plugins without a full DAW.

**SAVIHOST-style workflow**: Place the `.component` file next to the renamed app for portable setups!

## Features

- Load and run a single Audio Unit plugin
- Display the plugin's native editor interface
- Handle dynamic GUI resizing
- Process audio through the plugin
- Simple, zero-configuration setup
- **SAVIHOST-style loading**: Component file can be placed next to the app

## How to Use

TowelHost searches for plugins in this priority order:

### Method 1: Local Component (SAVIHOST-style)

1. Rename TowelHost.app to match your plugin name (e.g., "MyPlugin.app")
2. Place the matching .component file **in the same folder** as the renamed app
3. Launch the app - it will automatically load the component from the same directory

Example:
```
/Applications/
  ├── MyPlugin.app
  └── MyPlugin.component
```

**Note:** Some plugins may not work with this method due to macOS security restrictions. If the plugin fails to load, try Method 2.

### Method 2: System-Wide Installation (Required for some plugins)

Some plugins **require** installation to the system-wide location to work properly.

1. Copy the .component to: **`/Library/Audio/Plug-Ins/Components/`** (system location)
   - **NOT** `~/Library/Audio/Plug-Ins/Components/` (user location)
   - Use system location for maximum compatibility
2. Rename TowelHost.app to match the plugin name
3. Launch the app - it will find the component in the system location

**Why system location?** macOS's AudioUnit validation and security mechanisms work more reliably with system-installed plugins. If a plugin won't load from the local directory or user directory, the system directory often resolves the issue.

---

## Troubleshooting: Plugin Won't Load

If you get "No valid Audio Unit found in component", try these solutions in order:

### Solution 1: Install to System Location (Most Effective)

Move the plugin to the system-wide location:
```bash
sudo cp -R /path/to/YourPlugin.component /Library/Audio/Plug-Ins/Components/
```

This resolves most loading issues because macOS's AudioUnit framework validates system plugins more reliably.

### Solution 2: Remove Quarantine Flag

If the plugin was downloaded from the internet:
```bash
sudo xattr -r -d com.apple.quarantine /path/to/YourPlugin.component
```

### Solution 3: Manual Gatekeeper Approval

1. Right-click the .component file → Open
2. Click "Open" in the security dialog
3. Check System Settings > Privacy & Security for any blocks
4. Restart TowelHost

---

## Debug Logs

TowelHost automatically creates a log file next to the app when it runs:
- `YourPlugin_log.txt` - Contains detailed scanning and loading information

If a plugin fails to load, check this log file for specific error messages and paths checked.
