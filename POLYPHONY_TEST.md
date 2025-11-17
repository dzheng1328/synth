# Polyphony Integration Test Guide

## 🎹 What's New

Your synth now has **8-voice polyphony** powered by the AdvancedSynthesizer engine! This means you can play up to 8 notes simultaneously.

## ✅ Testing Polyphony

### Test 1: Basic Polyphony
1. Open http://localhost:8000 in your browser
2. Click "Continue as Guest" (or login if you prefer)
3. Hold down multiple keys on your keyboard at once:
   - Try: **Q + W + E** (C, D, E chord)
   - Try: **Q + E + Y** (C, E, G chord - C major)
   - Try: **Z + C + V** (lower C, E, F notes)
4. **Expected Result**: You should hear all notes playing together as a chord

### Test 2: Voice Stealing
1. Try to play more than 8 notes at once (use both hands on keyboard)
2. **Expected Result**: The oldest notes will stop as new notes are added (voice stealing)
3. Current voice limit: **8 voices** (expandable to 32 if needed)

### Test 3: Envelope Testing
1. Adjust the **ATTACK** knob (slow attack makes polyphony more obvious)
2. Play a chord: **Q + E + Y**
3. **Expected Result**: All notes fade in together with the attack time

### Test 4: Filter Testing
1. Play a chord: **Q + W + E + R + T**
2. Turn the **FILTER** knob down (counterclockwise)
3. **Expected Result**: The chord becomes darker/mellower as filter closes

### Test 5: Waveform Testing
1. Click the **WAVE** knob to cycle through: SINE → SQR → SAW → TRI
2. Play chords with each waveform
3. **Expected Results**:
   - **SINE**: Smooth, pure chord
   - **SQR**: Hollow, video-game style chord
   - **SAW**: Bright, aggressive chord
   - **TRI**: Soft, warm chord

## 🎵 Keyboard Layout

### Lower Octave (Z-M keys)
```
Black:  S  D     G  H  J
White: Z  X  C  V  B  N  M
Notes: C  D  E  F  G  A  B
```

### Middle Octave (Q-U keys)
```
Black:  2  3     5  6  7
White: Q  W  E  R  T  Y  U
Notes: C  D  E  F  G  A  B
```

### High C
```
White: I
Notes: C
```

## 🎼 Chord Suggestions

Try these chords to test polyphony:

**C Major**: Q + E + Y (C, E, G)
**D Minor**: W + R + Y (D, F, A)
**E Minor**: E + T + U (E, G, B)
**F Major**: R + Y + I (F, A, C)
**G Major**: T + U + Q (G, B, C - span octaves)

**Power Chords** (great for SAW waveform):
- Q + T (C + G)
- W + Y (D + A)
- E + U (E + B)

## 🔊 Audio Quality Notes

- **Frequency response**: Full range from 100Hz to 10kHz (filter knob)
- **Voice architecture**: Each voice has independent oscillators, envelopes, and filter
- **Voice allocation**: Automatic voice stealing when exceeding 8 voices
- **Oscillator types**: 4 waveforms (sine, square, sawtooth, triangle)

## 🐛 Troubleshooting

### No sound?
1. Make sure you clicked somewhere on the page (Web Audio requires user interaction)
2. Check browser console for errors (F12)
3. Verify audio isn't muted in browser

### Clicks/pops when playing notes?
- This is normal with very short attack times
- Increase the ATTACK knob slightly for smoother onset

### Notes getting cut off?
- You're hitting the 8-voice limit
- Notes are automatically stolen (oldest first)
- Future update: increase to 16 or 32 voices if needed

### Presets not working?
- Presets are still functional but only save: waveform, volume, attack, release, filter
- Performance features (arp, delay, sequencer) will be added to presets in next update

## 🚀 Next Steps

The polyphonic engine is now integrated! Next features to add:

1. **Performance Controls UI**: 
   - Arpeggiator panel (pattern selector, rate, octaves, latch)
   - Step sequencer grid (16 steps with visual feedback)
   - Loop delay controls (freeze button, time, feedback, filter)
   - Transport section (play/stop, record, tap tempo, BPM display)

2. **Visual Feedback**:
   - Voice meter (show how many voices are active)
   - Step sequencer playhead
   - Arpeggiator running indicator
   - Delay freeze LED

3. **Preset System Updates**:
   - Save/load performance settings (arp patterns, sequencer steps, delay times)
   - Performance-focused preset category
   - Example presets showcasing arp + delay combos

## 📊 Technical Details

### Engine Architecture
```
Keyboard Input
    ↓
Synthesizer Wrapper (synth.js)
    ↓
AdvancedSynthesizer (advanced-engine.js)
    ↓
VoiceAllocator (8 voices)
    ↓
Individual Voice objects (oscillator + filter + envelopes)
    ↓
FXRack (distortion → delay → reverb)
    ↓
Master Output
```

### Voice Allocation Strategy
- **Round-robin**: Voices are allocated in order
- **Voice stealing**: When all voices busy, steal oldest note
- **Legato mode**: Optional (not yet exposed in UI)
- **Portamento**: Smooth pitch glide (not yet exposed in UI)

### Current Parameters Mapped
- ✅ Waveform (sine/square/saw/triangle)
- ✅ Volume (master gain)
- ✅ Attack (0-1000ms)
- ✅ Release (0-2s)
- ✅ Filter Cutoff (100Hz-10kHz)
- ⏳ Filter Resonance (available in engine, not mapped to UI yet)
- ⏳ Decay/Sustain (ADSR - available in engine, not exposed yet)
- ⏳ Unison/Detune (available in engine, not exposed yet)

## 💡 Tips for Best Results

1. **Chord Playing**: Use SAW or SQR waveforms for bright chords
2. **Bass Lines**: Use SINE with low octave ([ key to lower octave)
3. **Lead Melodies**: Use SAW with higher octave (] key to raise octave)
4. **Pads**: Use TRI with longer attack time
5. **Stabs**: Use SQR with short attack, short release

---

**Status**: ✅ Polyphonic engine integrated and tested
**Last Updated**: November 17, 2025
**Next**: Add performance control UI (arpeggiator, sequencer, loop delay)
