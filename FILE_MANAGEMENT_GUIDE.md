# Project & File Management Guide

## Quick Start

### Creating a New Project

1. Click **📄 New** in the file menu (or press `Ctrl+N` / `Cmd+N`)
2. Enter a project name
3. Choose a template:
   - **📄 Empty Project** - Start from scratch
   - **🎹 Synth Lead** - Bright lead synthesizer
   - **🔊 Bass Patch** - Deep bass sound
   - **☁️ Pad Sound** - Warm ambient pad
   - **🌌 Ambient Texture** - Atmospheric soundscape
4. Click **Create**

### Saving Your Work

**Manual Save:**
- Click **💾 Save** (or `Ctrl+S` / `Cmd+S`)
- Downloads a `.synthproj` file to your computer

**Save As:**
- Click **💾 Save As** (or `Ctrl+Shift+S` / `Cmd+Shift+S`)
- Creates a new copy with a different name

**Autosave:**
- Automatically saves every 30 seconds to browser storage
- Recovers your work if browser crashes
- Prompts to restore on next visit

### Opening Projects

**From File:**
1. Click **📂 Open** (or `Ctrl+O` / `Cmd+O`)
2. Select a `.synthproj` file
3. All settings will be restored

**Recent Projects:**
1. Click **🕐 Recent**
2. See your last 10 projects with timestamps
3. Click to view details (full open coming soon)

### Exporting

Click **📦 Export** to download a project bundle (currently just JSON, ZIP bundles coming soon).

## File Menu Overview

### Menu Bar

Located at the top of the screen:

```
[📄 New] [📂 Open] [💾 Save] [💾 Save As] [🕐 Recent] [📦 Export]    Current Project Name *
```

The `*` indicates unsaved changes.

## Keyboard Shortcuts

| Action | Windows/Linux | macOS |
|--------|---------------|-------|
| New Project | `Ctrl+N` | `Cmd+N` |
| Open Project | `Ctrl+O` | `Cmd+O` |
| Save Project | `Ctrl+S` | `Cmd+S` |
| Save As | `Ctrl+Shift+S` | `Cmd+Shift+S` |

## Project Templates Explained

### Empty Project
- **Waveform:** Sine (purest tone)
- **Volume:** 50%
- **Attack:** 5ms (instant)
- **Release:** 30ms (short)
- **Filter:** 50% (neutral)
- **Octave:** 3 (middle)
- **Use for:** Clean slate, learning

### Synth Lead
- **Waveform:** Sawtooth (bright, rich)
- **Volume:** 60%
- **Attack:** 10ms (slightly soft)
- **Release:** 40ms (medium)
- **Filter:** 70% (bright)
- **Octave:** 4 (high)
- **Use for:** Melodies, dance leads

### Bass Patch
- **Waveform:** Square (aggressive)
- **Volume:** 70%
- **Attack:** 5ms (punchy)
- **Release:** 20ms (tight)
- **Filter:** 30% (dark)
- **Octave:** 2 (low)
- **Use for:** Basslines, sub bass

### Pad Sound
- **Waveform:** Triangle (warm, mellow)
- **Volume:** 40%
- **Attack:** 80ms (slow fade in)
- **Release:** 100ms (gentle fade out)
- **Filter:** 50% (balanced)
- **Octave:** 3 (middle)
- **Use for:** Background textures, chords

### Ambient Texture
- **Waveform:** Sine (soft, smooth)
- **Volume:** 35%
- **Attack:** 100ms (very slow)
- **Release:** 150ms (very gradual)
- **Filter:** 40% (slightly dark)
- **Octave:** 3 (middle)
- **Use for:** Atmospheres, drones, soundscapes

## Autosave Features

### How It Works

1. **Automatic:** Saves every 30 seconds
2. **Non-intrusive:** Happens in background
3. **Browser-based:** Stored in localStorage
4. **Crash Recovery:** Prompts on restart

### Autosave Dialog

When you reopen the app and an autosave is found:

```
┌─────────────────────────────────┐
│  Recover Autosave?              │
│                                 │
│  Found: "My Project"            │
│  Would you like to restore it?  │
│                                 │
│  [Discard]        [Restore]    │
└─────────────────────────────────┘
```

**Restore:** Loads the autosaved project
**Discard:** Clears autosave and starts fresh

### Autosave vs Manual Save

| Feature | Autosave | Manual Save |
|---------|----------|-------------|
| Frequency | Every 30s | On demand |
| Storage | localStorage | Downloaded file |
| Survives | Browser crash | Everything |
| Shareable | No | Yes |
| Backup | Temporary | Permanent |

**Best Practice:** Use autosave as a safety net, manual save for permanent backups.

## Recent Projects

### What's Stored

- Last 10 opened/saved projects
- Project name
- Last modified date
- Author email (if logged in)
- Patch preview

### Recent Projects List

Shows projects with:
- **Thumbnail:** 📁 icon (visual preview coming soon)
- **Name:** Project title
- **Time:** "5m ago", "2h ago", "3d ago"
- **Author:** Your email or "Guest"

### Metadata

Each recent entry includes:
```json
{
  "name": "My Project",
  "id": "proj_...",
  "modified": "2025-11-16T...",
  "author": "user@example.com",
  "patch": { ... }
}
```

## Project File Format

### File Extension
`.synthproj` (JSON format)

### File Contents
```json
{
  "version": "1.0.0",
  "name": "My Project",
  "patch": { ... },
  "settings": { ... },
  "metadata": { ... }
}
```

See [PROJECT_FORMAT.md](./PROJECT_FORMAT.md) for complete specification.

### Compatibility

- **Human-readable:** Open in any text editor
- **Git-friendly:** Text-based, easy to diff
- **Versioned:** Schema version for future compatibility
- **Extensible:** Can add new fields without breaking

## Import/Export

### Currently Supported

**Import:**
- `.synthproj` files (native format)

**Export:**
- `.synthproj` files (save/save as)
- Project bundles (export button)

### Coming Soon

**Import:**
- Drag & drop `.synthproj` files
- WAV/MP3/AIFF audio samples
- Ableton/FL exported stems (metadata)
- Preset files

**Export:**
- ZIP bundles with samples
- Preset packs
- Share links (cloud sync)

## File Management Best Practices

### Naming Convention

Good names:
- ✅ `bass_lead_v2.synthproj`
- ✅ `ambient_pad_final.synthproj`
- ✅ `2025_11_16_track1.synthproj`

Bad names:
- ❌ `untitled.synthproj`
- ❌ `project (1).synthproj`
- ❌ `asdf.synthproj`

### Organization Tips

**By Project:**
```
my-music/
  track-01/
    bass.synthproj
    lead.synthproj
    pad.synthproj
  track-02/
    ...
```

**By Type:**
```
synth-patches/
  basses/
  leads/
  pads/
  fx/
```

**By Date:**
```
2025-11-16_session1.synthproj
2025-11-16_session2.synthproj
```

### Version Control

**Manual Versioning:**
```
my_project_v1.synthproj
my_project_v2.synthproj
my_project_final.synthproj
my_project_final_final.synthproj  ← Don't do this!
```

**Git Versioning (Recommended):**
```bash
git init my-synth-project
git add *.synthproj
git commit -m "Initial bass patch"
```

Benefits:
- Full history
- Easy rollback
- Collaboration ready
- Branching for experiments

### Backup Strategy

**3-2-1 Rule:**
- **3** copies of your projects
- **2** different storage types
- **1** offsite backup

Example:
1. Working copy (local disk)
2. Time Machine / Backblaze (local backup)
3. GitHub / Dropbox (cloud backup)

## Troubleshooting

### "Failed to load project"

**Cause:** Invalid JSON or corrupted file

**Fix:**
1. Open file in text editor
2. Check for syntax errors
3. Validate JSON at jsonlint.com
4. Restore from backup if corrupted

### "No recent projects"

**Cause:** Browser data cleared or private mode

**Fix:**
- Save projects as files, not just autosave
- Don't rely on recent list for permanent storage

### "Autosave not working"

**Cause:** localStorage disabled or full

**Fix:**
1. Check browser settings (allow localStorage)
2. Clear old autosaves
3. Use manual save instead

### Lost unsaved changes

**Prevention:**
- Enable autosave (on by default)
- Save frequently (`Ctrl+S`)
- Check for `*` in project name (indicates unsaved)

### File won't download

**Cause:** Browser blocking downloads

**Fix:**
1. Check browser popup blocker
2. Allow downloads from site
3. Try Save As with different name

## Advanced Tips

### Preset Workflow

1. Create base project
2. Save as `bass_base.synthproj`
3. Tweak settings
4. Save as `bass_bright.synthproj`, `bass_dark.synthproj`, etc.
5. Build a library of variations

### Project Metadata

Edit `.synthproj` files to add:
```json
"metadata": {
  "tags": ["bass", "dark", "aggressive"],
  "notes": "Use this for drop sections",
  "bpm": 140,
  "key": "Am"
}
```

### Batch Operations

Use command-line tools:
```bash
# Find all projects
find . -name "*.synthproj"

# Search project contents
grep "sawtooth" *.synthproj

# Backup all projects
zip -r projects_backup.zip *.synthproj
```

## Future Features

### Planned v1.1
- [ ] Drag & drop file import
- [ ] Visual thumbnails in recent list
- [ ] Project tags and search
- [ ] Undo/redo with history

### Planned v1.2
- [ ] ZIP bundle export with samples
- [ ] Cloud project sync
- [ ] Shared presets library
- [ ] Collaboration features

### Planned v2.0
- [ ] Multi-track projects
- [ ] Pattern/sequence storage
- [ ] MIDI mapping save/load
- [ ] Audio clip integration

## Getting Help

**Issues:**
- Check [GitHub Issues](https://github.com/dzheng1328/synth/issues)
- File a bug report with `.synthproj` file

**Questions:**
- Read [README.md](./README.md)
- Check [PROJECT_FORMAT.md](./PROJECT_FORMAT.md)

**Feedback:**
- Suggest features in GitHub Discussions
- Share your workflow improvements
