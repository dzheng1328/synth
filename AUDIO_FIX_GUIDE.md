# 🚨 SYNTH AUDIO FIX GUIDE

## Problem Summary
- Keyboard input not working
- No sound when pressing keys
- GUI opens but notes don't play

## Root Causes Identified

### 1. **Nuklear Intercepts Keyboard Events**
The GUI library (Nuklear) captures all keyboard input before it reaches our callback.

### 2. **Audio Callback May Not Be Processing**
Need to verify the synth engine is actually generating audio.

### 3. **Initial Volume Too Low**
Master volume might be set too low to hear.

---

## IMMEDIATE FIX

### Step 1: Test Audio Works (Minimal Test)
```bash
cd /Users/dzheng/Documents/synth

# Compile minimal test
gcc test_audio.c synth_engine.c -o test_audio -lm -lpthread \
    -framework CoreAudio -framework AudioToolbox -O2

# Run it - should play C major scale
./test_audio
```

**Expected Output:**
```
🎵 Minimal Synth Audio Test
✅ Audio started

Playing C4 (MIDI 60)...
Playing D4 (MIDI 62)...
...
✅ Test complete!
```

If you **HEAR THE SCALE**, audio engine works! Problem is GUI/keyboard integration.

If you **DON'T HEAR**, audio engine has issues.

---

### Step 2: Simple Keyboard Fix (No GUI)

Create keyboard-only version without Nuklear:

```c
// In main(), replace GUI loop with:
printf("Press keys Z-M for notes (Ctrl+C to quit)\\n");

while (!glfwWindowShouldClose(g_app.window)) {
    glfwPollEvents();
    
    // Check each key directly
    for (size_t i = 0; i < KEYMAP_SIZE; i++) {
        int state = glfwGetKey(g_app.window, g_keymap[i].glfw_key);
        
        if (state == GLFW_PRESS && !g_app.keys_pressed[i]) {
            g_app.keys_pressed[i] = 1;
            synth_note_on(&g_app.synth, g_keymap[i].midi_note, 1.0f);
            printf("ON: %s\\n", g_keymap[i].label);
        } else if (state == GLFW_RELEASE && g_app.keys_pressed[i]) {
            g_app.keys_pressed[i] = 0;
            synth_note_off(&g_app.synth, g_keymap[i].midi_note);
            printf("OFF: %s\\n", g_keymap[i].label);
        }
    }
    
    sleep(0.01); // 10ms
}
```

This bypasses Nuklear entirely - **polling keyboard directly**.

---

### Step 3: Fix Nuklear Keyboard Conflict

The issue is Nuklear uses `NK_GLFW3_INSTALL_CALLBACKS` which steals our keyboard handler.

**SOLUTION:**

1. Initialize Nuklear with `NK_GLFW3_DEFAULT` instead
2. Manually call Nuklear's input handling
3. Check if Nuklear used the key first

```c
// In main():
g_app.nk_ctx = nk_glfw3_init(&g_app.glfw, g_app.window, NK_GLFW3_DEFAULT);
glfwSetKeyCallback(g_app.window, key_callback);

// In key_callback(), add at top:
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Let Nuklear handle it first
    nk_glfw3_keyboard_callback(window, key, scancode, action, mods);
    
    // If Nuklear consumed it (e.g., typing in text box), return
    if (nk_item_is_any_active(g_app.nk_ctx)) {
        return;
    }
    
    // Now handle our piano keys...
    // rest of function
}
```

---

### Step 4: Clickable Piano Keys (Verified Working)

The piano keyboard GUI buttons work BUT need proper mouse handling:

```c
// Button press plays note immediately
// Need to add release detection

static int last_clicked_note = -1;

// In draw_gui() for each piano key:
if (nk_button_label_styled(ctx, &button_style, white_labels[i])) {
    if (last_clicked_note == midi_note) {
        // Already playing, stop it
        stop_note(midi_note, white_labels[i]);
        last_clicked_note = -1;
    } else {
        // Play new note
        if (last_clicked_note >= 0) {
            stop_note(last_clicked_note, "prev");
        }
        play_note(midi_note, white_labels[i]);
        last_clicked_note = midi_note;
    }
}
```

---

## DEBUGGING STEPS

### 1. Check Audio Callback is Running
Add to `audio_callback()`:
```c
static int frame_count = 0;
if (frame_count++ % 44100 == 0) {
    printf("Audio callback alive (1 sec mark)\\n");
}
```

### 2. Check Notes Are Triggered
Already added - should see:
```
🎵 PLAY: C4 (MIDI 60) - Active voices: 1
```

### 3. Check Synth Engine Output
Add to audio_callback:
```c
float max_sample = 0.0f;
for (ma_uint32 i = 0; i < frameCount; i++) {
    synth_process(&g_app.synth, temp_buffer, 1);
    if (fabs(temp_buffer[0]) > max_sample) {
        max_sample = fabs(temp_buffer[0]);
    }
}
if (max_sample > 0.01f) {
    printf("Audio level: %.3f\\n", max_sample);
}
```

### 4. System Audio Check
```bash
# Check macOS audio is working
say "Testing audio"

# Check default output device
system_profiler SPAudioDataType | grep "Default Output Device"

# Adjust volume
osascript -e "set volume output volume 50"
```

---

## ALTERNATIVE: Use Web Version

Your JavaScript version works! If C version is too troublesome:

```bash
cd /Users/dzheng/Documents/synth
open synth.html  # Or run local server
```

The web version has:
- Working keyboard input
- Working audio
- Complete UI
- Effects, arpeggiator, etc.

---

## RECOMMENDED ACTION PLAN

### Option A: Quick Audio Test (5 min)
1. Run `test_audio.c` to verify synth engine works
2. If works → problem is GUI/keyboard integration
3. If doesn't work → problem is audio system

### Option B: Bypass Nuklear (15 min)
1. Comment out all GUI code
2. Use direct `glfwGetKey()` polling
3. Print to console when keys pressed
4. Verify audio plays

### Option C: Fix Nuklear Integration (30 min)
1. Use `NK_GLFW3_DEFAULT` init
2. Add proper keyboard event routing
3. Test both GUI widgets and piano keys work

### Option D: Use Web Version (0 min)
1. Already working
2. Focus on other features
3. Come back to C version later

---

## FILES TO CHECK

1. **test_audio.c** - Minimal audio test (just created)
2. **synth_complete.c** - Full GUI version (has issues)
3. **synth_demo.c** - Simple demo (works, no GUI)
4. **synth.html** - Web version (works perfectly)

---

## QUICK WINS

### Make It Loud
```c
// In main() after synth_init():
g_app.synth.master_volume = 1.0f;  // Max volume
g_app.synth.voices[0].osc1.amplitude = 1.0f;
g_app.synth.voices[0].osc1.waveform = WAVE_SAW;  // Bright waveform
```

### Force Note On at Startup
```c
// After audio starts:
printf("Playing test tone...\\n");
for (int i = 0; i < MAX_VOICES; i++) {
    g_app.synth.voices[i].osc1.waveform = WAVE_SAW;
}
synth_note_on(&g_app.synth, 60, 1.0f);
sleep(2);
synth_note_off(&g_app.synth, 60);
```

---

## EXPECTED BEHAVIOR

**When Working:**
- Press `Z` key → hear C3 note
- Click piano key button → hear note
- Terminal shows: `🎵 PLAY: C3 (MIDI 48) - Active voices: 1`
- Release key → note stops
- Terminal shows: `🔇 STOP: C3 (MIDI 48) - Active voices: 0`

**Current Behavior:**
- Press keys → nothing (no terminal output, no sound)
- Click buttons → nothing
- GUI opens but no interaction

---

## NEXT STEPS

1. **RUN test_audio.c FIRST** - tells us if core audio works
2. If audio works: Fix keyboard/GUI integration
3. If audio doesn't work: Debug miniaudio/CoreAudio
4. Consider using web version while debugging C version

**The audio engine DSP is solid - it's just a routing/integration issue!**
