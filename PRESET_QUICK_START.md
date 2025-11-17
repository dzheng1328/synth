# Preset System - Quick Start Guide

## 🎨 What You Can Do

The preset system lets you:
- **Browse 30 professional presets** organized by category
- **Save your own custom sounds** with tags and descriptions
- **Star your favorites** for quick access
- **Search by name or tags** to find sounds fast
- **Morph between two presets** for performance mode
- **Share presets** via export/import

---

## 🚀 Quick Start: 3 Steps to Your First Preset

### 1. Browse Factory Presets
```
Click "🎨 Presets" button (or press Ctrl+P)
→ Browse categories: Bass, Lead, Pad, Keys, FX
→ Click ▶️ to preview a sound
→ Click "Load" to apply it to your synth
```

### 2. Customize the Sound
```
Adjust knobs:
→ Wave (waveform type)
→ Volume
→ Attack (fade-in time)
→ Release (fade-out time)
→ Cutoff (filter frequency)
```

### 3. Save Your Custom Preset
```
Click "💾 Save Preset" button
→ Enter name: "My Awesome Bass"
→ Select category: Bass
→ Add tags: "deep, dark, heavy"
→ Click "Save Preset"
```

**Done!** Your preset is now in your library forever.

---

## 📚 Common Workflows

### Finding the Perfect Sound

**Method 1: Browse by Category**
```
1. Open preset browser (Ctrl+P)
2. Click category tab: Bass, Lead, Pad, Keys, or FX
3. Scroll through cards
4. Click preview (▶️) to hear each one
5. Load your favorite
```

**Method 2: Search**
```
1. Open preset browser
2. Type in search bar: "wobble" or "bright" or "808"
3. Results filter in real-time
4. Preview and load
```

**Method 3: Favorites**
```
1. Star presets you like (click ☆)
2. Click "⭐ Favorites" button
3. See only your starred presets
4. Quick access to your go-to sounds
```

### Creating a Preset Collection

**Build Your Library:**
```
1. Start with factory preset (e.g., "Bright Lead")
2. Duplicate it (📋 button)
3. Modify the copy
4. Save with descriptive name
5. Add tags for organization
6. Repeat for variations
```

**Result:** A personal library organized your way!

### Sharing Presets with Others

**Export:**
```
1. Open preset browser
2. Find your preset
3. Click 💾 export button
4. File downloads: "My_Awesome_Bass.synthpreset"
5. Share via email, Slack, GitHub, etc.
```

**Import (when implemented):**
```
1. Receive .synthpreset file
2. Drag onto browser window
3. Preset appears in your library
4. Ready to use!
```

### Live Performance: Preset Morphing

**Setup:**
```
1. Enable morph mode (from preset browser or dedicated button)
2. Slot A: Select first preset (e.g., "Deep Bass")
3. Slot B: Select second preset (e.g., "Wobble Bass")
4. Move slider: 0% = full A, 100% = full B
```

**Perform:**
```
→ Start at 0% (Deep Bass)
→ Slowly slide to 100% (transitions to Wobble Bass)
→ Sound morphs smoothly between the two
→ Record or perform live with morphing
```

**Disable:**
```
Click ✕ to exit morph mode and return to normal
```

---

## 🎹 Factory Preset Recommendations

### For EDM/Electronic
- **Lead**: Bright Lead, Sync Lead, Screamer
- **Bass**: Wobble Bass, Reese Bass
- **Pad**: Sweep Pad, Bright Pad

### For Hip-Hop/Trap
- **Bass**: 808 Bass, Deep Bass
- **Lead**: Pluck Lead, Stab Lead
- **Keys**: Electric Piano

### For Ambient/Cinematic
- **Pad**: Space Pad, Dark Pad, Warm Pad
- **Lead**: Soft Lead, Smooth Lead
- **FX**: Wind, Noise Sweep

### For Funk/Disco
- **Bass**: Funk Bass
- **Keys**: Clavinet, Electric Piano, Organ
- **Lead**: Pluck Lead

### For Sound Design/FX
- **FX**: All 5 (Noise Sweep, Impact, Laser, Wind, Alarm)
- **Lead**: Screamer, Sync Lead
- **Bass**: Wobble Bass (with high resonance)

---

## 💡 Pro Tips

### Organizing Your Presets

**Use Descriptive Names:**
```
❌ "Lead 1", "Bass 2", "Test"
✅ "Aggressive Saw Lead", "Deep Sub Bass", "Ethereal Pad"
```

**Tag Strategically:**
```
Genre tags: edm, ambient, hiphop, trap, dnb
Sonic tags: bright, dark, warm, cold, aggressive, soft
Use tags: drop, buildup, breakdown, intro, verse
```

**Star Your Favorites:**
```
Star presets you use frequently
Use favorites filter for quick access
Re-organize periodically
```

### Creating Better Presets

**Capture Context:**
```
BPM: Set if tempo-specific (e.g., 140 for DnB bass)
Key: Note if tuned to specific key (e.g., "A minor")
Description: Write how/when to use it
```

**Build Variations:**
```
Save multiple versions:
→ "Bright Lead" (original)
→ "Bright Lead (Soft)" (lower volume)
→ "Bright Lead (Short)" (fast release)
→ "Bright Lead (Aggressive)" (high resonance)
```

### Performance Mode

**Morph Planning:**
```
Choose contrasting presets:
→ Dark ↔ Bright
→ Short ↔ Long
→ Low ↔ High
→ Sine ↔ Sawtooth

Creates dramatic morphs!
```

**Practice Transitions:**
```
Morph slowly for buildups
Morph quickly for drops
Snap between A and B (0% or 100%)
Use morph slider as performance control
```

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+P` or `Cmd+P` | Open preset browser |
| `Escape` | Close preset browser |
| `Enter` | Load selected preset (future) |
| Arrow keys | Navigate presets (future) |

---

## 🔧 Troubleshooting

### "My preset didn't save!"
**Solution:** Check localStorage isn't full
```
1. Open browser console (F12)
2. Type: localStorage.getItem('synth_user_presets')
3. If null or error, localStorage may be disabled
4. Check browser privacy settings
5. Try exporting presets as files instead
```

### "Factory presets are missing!"
**Solution:** Hard refresh
```
1. Press Ctrl+Shift+R (Windows) or Cmd+Shift+R (Mac)
2. Or clear browser cache
3. Factory presets reload from code
```

### "Search isn't finding my preset"
**Solution:** Check tags and description
```
1. Search looks in: name, tags, description
2. Make sure preset has tags added
3. Try searching part of the name
4. Check spelling
```

### "Morph isn't working"
**Solution:** Select both A and B
```
1. Ensure slot A has a preset selected
2. Ensure slot B has a preset selected
3. Move slider to see effect
4. Try with very different presets first
```

---

## 📊 Preset Browser UI Guide

```
┌─────────────────────────────────────────────┐
│ 🎨 Preset Browser                        ✕  │
├─────────────────────────────────────────────┤
│ [Search presets...]  [⭐ Favorites]         │
│ [All] [Bass] [Lead] [Pad] [Keys] [FX]      │
├─────────────────────────────────────────────┤
│ ┌────────┐ ┌────────┐ ┌────────┐           │
│ │ ☆ Deep │ │ ⭐ 808  │ │ ☆ Reese│           │
│ │   🔊   │ │   🔊   │ │   🔊   │           │
│ │  Bass  │ │  Bass  │ │  Bass  │           │
│ │        │ │        │ │        │           │
│ │ bass,  │ │ trap,  │ │ dnb,   │           │
│ │ deep,  │ │ 808,   │ │ dark   │           │
│ │ sub    │ │ hiphop │ │        │           │
│ │        │ │        │ │        │           │
│ │▶️ Load │ │▶️ Load │ │▶️ Load │           │
│ │📋 💾   │ │📋 💾   │ │🗑️ 💾   │           │
│ └────────┘ └────────┘ └────────┘           │
│                                             │
│            [More presets...]                │
├─────────────────────────────────────────────┤
│                            [Close]          │
└─────────────────────────────────────────────┘

Legend:
☆/⭐ = Favorite star (click to toggle)
▶️ = Preview (plays 1-second demo)
Load = Apply preset to synth
📋 = Duplicate (factory presets)
🗑️ = Delete (user presets)
💾 = Export to file
```

---

## 🎯 Next Steps

### Beginner Path
1. ✅ Browse factory presets
2. ✅ Load a few to hear different sounds
3. ✅ Star your favorites
4. ✅ Tweak one and save it
5. ✅ Build your first 5-10 custom presets

### Intermediate Path
1. ✅ Create presets for different genres
2. ✅ Add detailed tags and descriptions
3. ✅ Build preset variations
4. ✅ Organize with stars and categories
5. ✅ Export presets as backup

### Advanced Path
1. ✅ Master preset morphing
2. ✅ Create preset packs for projects
3. ✅ Share presets with community
4. ✅ Build comprehensive sound library
5. ✅ Use presets in live performance

---

## 📖 Full Documentation

For complete technical details, see:
- **PRESET_SYSTEM.md** - Full API reference and architecture
- **README.md** - Project overview with preset features
- **PRESET_IMPLEMENTATION_SUMMARY.md** - Development summary

---

## 🎵 Have Fun!

The preset system is designed to:
- **Speed up your workflow** (don't start from scratch)
- **Inspire creativity** (browse and discover sounds)
- **Organize your library** (tags, favorites, search)
- **Share with others** (export/import presets)

Start by browsing the 30 factory presets, then build your own library!

**Press `Ctrl+P` to open the preset browser and get started!** 🚀
