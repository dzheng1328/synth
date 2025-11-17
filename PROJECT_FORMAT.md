# Project File Format Specification

## Overview

Synth projects use the `.synthproj` file format, which is a JSON-based format for storing synthesizer state, settings, and metadata.

## File Structure

### Version 1.0.0

```json
{
  "version": "1.0.0",
  "name": "My Project",
  "id": "proj_1700123456789_abc123def",
  "created": "2025-11-16T10:30:00.000Z",
  "modified": "2025-11-16T11:45:00.000Z",
  "author": "user@example.com",
  
  "patch": {
    "waveform": "sawtooth",
    "volume": 50,
    "attack": 5,
    "release": 30,
    "filter": 50,
    "octave": 3
  },
  
  "settings": {
    "tempo": 120,
    "masterVolume": 1.0,
    "sampleRate": 44100
  },
  
  "metadata": {
    "thumbnail": null,
    "tags": ["lead", "synth"],
    "notes": "Notes about this project"
  },
  
  "files": {
    "samples": [],
    "presets": []
  }
}
```

## Field Descriptions

### Top Level

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | Schema version (semver) |
| `name` | string | Human-readable project name |
| `id` | string | Unique project identifier |
| `created` | ISO8601 | Creation timestamp |
| `modified` | ISO8601 | Last modification timestamp |
| `author` | string | Creator email or "Guest" |

### Patch Object

Current synthesizer state.

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `waveform` | string | sine, square, sawtooth, triangle | Oscillator waveform |
| `volume` | number | 0-100 | Master volume percentage |
| `attack` | number | 0-100 | Attack time in milliseconds |
| `release` | number | 0-200 | Release time in 10ms units |
| `filter` | number | 0-100 | Filter cutoff percentage |
| `octave` | number | 0-7 | Current octave offset |

### Settings Object

Project-wide settings.

| Field | Type | Description |
|-------|------|-------------|
| `tempo` | number | BPM (beats per minute) |
| `masterVolume` | number | 0.0-1.0 master gain |
| `sampleRate` | number | Audio sample rate in Hz |

### Metadata Object

Optional project metadata.

| Field | Type | Description |
|-------|------|-------------|
| `thumbnail` | string\|null | Base64 encoded image or null |
| `tags` | array | Array of tag strings |
| `notes` | string | User notes/description |

### Files Object

References to external files (future use).

| Field | Type | Description |
|-------|------|-------------|
| `samples` | array | Audio sample references |
| `presets` | array | Saved preset patches |

## Project Templates

Built-in templates available:

### Empty
Clean slate with sine wave, minimal envelope.

### Synth Lead
Bright sawtooth lead with medium attack/release.

### Bass Patch
Deep square wave bass with short envelope, low octave.

### Pad Sound
Warm triangle pad with long attack/release.

### Ambient Texture
Soft sine wave with very long envelope for atmospherics.

## File Operations

### Save Project
Exports current synth state as `.synthproj` JSON file.

**Keyboard Shortcut:** `Ctrl+S` / `Cmd+S`

### Save As
Saves with a new name and generates new project ID.

**Keyboard Shortcut:** `Ctrl+Shift+S` / `Cmd+Shift+S`

### Open Project
Loads a `.synthproj` file and applies all settings.

**Keyboard Shortcut:** `Ctrl+O` / `Cmd+O`

**Supported Methods:**
- File menu → Open
- Drag & drop `.synthproj` file onto window (future)

### Export Bundle
Currently exports project JSON. Future: ZIP bundle with samples/presets.

### Recent Projects
Stores last 10 opened/saved projects in localStorage.

**Storage Key:** `synth_recent_projects`

## Autosave

Projects are automatically saved to browser localStorage every 30 seconds.

**Storage Key:** `synth_autosave`

**Recovery:** On app launch, prompts to restore if autosave found.

**Behavior:**
- Autosave runs in background
- Separate from manual saves
- Can be restored after browser crash
- Cleared on successful manual save

## Version Compatibility

### Forward Compatibility
Newer versions should gracefully handle older project files by:
1. Checking `version` field
2. Applying migrations if needed
3. Using sensible defaults for missing fields

### Backward Compatibility
Older app versions will:
- Ignore unknown fields
- Validate known fields
- Show warning if version mismatch

## Future Enhancements

### Planned for v1.1.0
- Binary blob support for embedded samples
- Project bundles (ZIP format)
- Cloud sync metadata
- Collaboration fields (authors, permissions)
- Version history within file

### Planned for v1.2.0
- MIDI mapping storage
- Automation/pattern data
- Multi-track support
- Effect chain settings

## Import/Export Formats

### Current Support
- `.synthproj` - Native format

### Planned Support
- `.zip` - Project bundle with samples
- `.wav/.mp3/.aiff` - Audio import
- Ableton/FL stems (metadata only)
- MIDI file import

## Migration Guide

When schema version changes:

```javascript
function migrateProject(project) {
  const version = project.version || '1.0.0';
  
  if (version === '1.0.0') {
    // Already current
    return project;
  }
  
  // Apply migrations...
  return project;
}
```

## Validation

Basic validation rules:

```javascript
function validateProject(json) {
  if (!json.version) throw new Error('Missing version');
  if (!json.name) throw new Error('Missing name');
  if (!json.patch) throw new Error('Missing patch data');
  
  // Validate ranges
  const p = json.patch;
  if (p.volume < 0 || p.volume > 100) throw new Error('Invalid volume');
  if (p.octave < 0 || p.octave > 7) throw new Error('Invalid octave');
  
  return true;
}
```

## Examples

### Minimal Valid Project
```json
{
  "version": "1.0.0",
  "name": "Test",
  "id": "proj_123",
  "created": "2025-11-16T00:00:00.000Z",
  "modified": "2025-11-16T00:00:00.000Z",
  "author": "Guest",
  "patch": {
    "waveform": "sine",
    "volume": 50,
    "attack": 5,
    "release": 30,
    "filter": 50,
    "octave": 3
  },
  "settings": {
    "tempo": 120,
    "masterVolume": 1.0,
    "sampleRate": 44100
  },
  "metadata": {
    "thumbnail": null,
    "tags": [],
    "notes": ""
  },
  "files": {
    "samples": [],
    "presets": []
  }
}
```

### Project with Metadata
```json
{
  "version": "1.0.0",
  "name": "Epic Lead",
  "metadata": {
    "thumbnail": "data:image/png;base64,...",
    "tags": ["lead", "bright", "dance"],
    "notes": "Main lead sound for track #3"
  }
}
```
