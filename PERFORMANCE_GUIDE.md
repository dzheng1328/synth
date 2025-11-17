# Performance Controls Usage Guide 🎹

## 🎉 What's New!

Your synth now has **full performance controls**! You can now:
- ✅ **Play chords** (8-voice polyphony)
- ✅ **Arpeggiate notes** with 5 different patterns
- ✅ **Loop delays** with real-time parameter changes
- ✅ **Freeze delay** to hold repeats infinitely
- ✅ **Tap tempo** to sync everything to your rhythm
- ✅ **Latch mode** for hands-free arpeggiation

## 🚀 Quick Start

1. **Open**: http://localhost:8000
2. **Skip login** or login if you want to save projects
3. **Click anywhere** on the page to activate audio
4. **Press SPACE** to start the clock
5. **Hold some keys** (Q, W, E) and watch them arpeggiate!

## 🎮 Control Sections

### 1. TRANSPORT SECTION (Top Left)

**PLAY Button** (▶)
- Click to start/stop the master clock
- When playing, button turns orange and shows ⏸
- Keyboard shortcut: **SPACE**

**TAP Button** (⊙)
- Tap repeatedly to set tempo by tapping
- Minimum 2 taps to calculate BPM
- BPM display updates automatically
- Keyboard shortcut: **T**

**BPM Display** (Green Numbers)
- Shows current tempo (40-240 BPM)
- Default: 120 BPM
- Changes color when tapping

**Clock Indicator** (Circle)
- Flashes green on every beat
- Visual metronome for timing
- Pulses every quarter note

### 2. ARPEGGIATOR SECTION

**ON Button** (Top Right)
- Enable/disable arpeggiator
- LED glows green when active
- Keyboard shortcut: **A**

**Pattern Buttons**
- **UP**: C → D → E → F (ascending)
- **DOWN**: F → E → D → C (descending)
- **UP/DN**: C → D → E → F → E → D (up then down)
- **RAND**: Random note from held keys
- **PLAY**: Order you pressed keys

**RATE Knob**
- Controls arpeggio speed
- Options: 1/32, 1/16, 1/8, 1/4 notes
- Default: 1/16 (fast)
- Synced to master clock BPM

**OCTAVES Knob**
- Octave range: 1-4 octaves
- At 2 octaves: plays pattern twice (octave higher second time)
- Default: 1 octave

**GATE Knob**
- Note length as % of step time
- 0%: Very short, staccato notes
- 100%: Full length, legato notes
- Default: 75% (slightly detached)

**LATCH Button**
- LED glows when active
- When ON: Notes continue after you release keys
- When OFF: Stops when you release keys
- Keyboard shortcut: **L**

### 3. LOOP DELAY SECTION

**ON Button** (Top Right)
- Enable/disable delay effect
- LED glows green when active
- Default: ON

**TIME Knob**
- Delay time: 50ms to 2000ms
- Default: 500ms (half second)
- Display shows milliseconds
- When SYNC is on, shows note division (1/8, 1/4, etc)

**FEEDBACK Knob**
- Amount of delay fed back into itself
- 0%: Single echo
- 100%: Infinite repeats
- Default: 60% (musical sweet spot)
- Careful above 80% - can get loud!

**FILTER Knob**
- Filters the delay feedback (tone control)
- Low settings: Dark, warm repeats
- High settings: Bright, crisp repeats
- Range: 200Hz to 20kHz
- Default: 20kHz (no filtering)

**MIX Knob**
- Wet/dry balance
- 0%: No delay (dry signal only)
- 100%: Only delay (wet signal)
- Default: 30% (subtle delay)

**FREEZE Button** 🔥
- **THE MAGIC BUTTON**
- Holds current delay buffer infinitely
- LED glows orange when frozen
- Sets feedback to 100% temporarily
- Creates infinite sustained textures
- Keyboard shortcut: **F**
- **Try it**: Play notes, let them delay, then freeze!

**SYNC Button**
- Tempo-sync delay time to BPM
- LED glows when active
- Time knob switches to note divisions
- Example: At 120 BPM, 1/8 = 250ms

**CLEAR Button**
- Instantly clears all delay repeats
- Kills feedback momentarily
- Useful for cleaning up muddy delays
- Visual feedback: button dims briefly

## 🎹 Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **SPACE** | Play/Stop clock |
| **T** | Tap Tempo |
| **A** | Toggle Arpeggiator |
| **F** | Freeze Delay |
| **L** | Toggle Latch Mode |
| **[** | Octave Down |
| **]** | Octave Up |

## 🎵 Example Workflows

### Workflow 1: Basic Arpeggio
1. Press **SPACE** to start clock
2. Click **ON** in Arpeggiator section
3. Hold **Q + E + Y** (C major chord)
4. Listen to automatic arpeggiation!
5. Try different **PATTERN** buttons

### Workflow 2: Delay Freeze Texture
1. Set **FILTER** knob on delay to 50% (darker repeats)
2. Set **FEEDBACK** to 70%
3. Play a melody slowly (Q, W, E, R, T)
4. Let it repeat a few times
5. Press **FREEZE** button
6. The delay holds forever!
7. Play new notes on top of the frozen texture
8. Press **FREEZE** again to unfreeze

### Workflow 3: Tempo-Synced Arpeggiated Delay
1. Press **T** 4 times to your desired tempo (or use default 120 BPM)
2. Press **SPACE** to start clock
3. Enable **Arpeggiator** (A key)
4. Set pattern to **UP/DN**
5. Set **RATE** to 1/16
6. Enable delay **SYNC** button
7. Set delay time to correspond with arp rate
8. Set delay **FEEDBACK** to 50%
9. Hold a chord and enjoy synced groove!

### Workflow 4: Random Ambient Texture
1. Start clock (**SPACE**)
2. Enable **Arpeggiator**
3. Set pattern to **RANDOM**
4. Set **OCTAVES** to 3
5. Set **RATE** to 1/8 (slower)
6. Enable delay **SYNC**
7. Set delay **FEEDBACK** to 65%
8. Set delay **FILTER** to 40% (darker)
9. Set delay **MIX** to 50%
10. Hold any chord and let it evolve!

### Workflow 5: Latch Mode Performance
1. Start clock
2. Enable **Arpeggiator**
3. Press **LATCH** button (L key)
4. Tap a chord: **Q + E + Y**
5. Release keys - arp continues!
6. Adjust **RATE**, **GATE**, **PATTERN** in real-time
7. Tweak delay **TIME** and **FEEDBACK**
8. Tap another chord to change the arp
9. Press **LATCH** again to disable when done

## 🎚️ Sound Design Tips

### For Bass Lines
- Waveform: **SAW** or **SQR**
- Octave: **Low** ([ key)
- Attack: **Short** (5-10ms)
- Filter: **Medium** (40-50%)
- Arp Pattern: **PLAY** or **UP**
- Arp Rate: **1/16** or **1/8**
- Delay: **Subtle** (10-20% mix)

### For Lead Melodies
- Waveform: **SAW**
- Attack: **Medium** (50-100ms)
- Release: **Long** (1-2s)
- Filter: **Open** (80-100%)
- Arp Pattern: **UP/DN** or **RANDOM**
- Delay: **Short time** (100-250ms), **40% mix**

### For Pads/Textures
- Waveform: **TRI** or **SINE**
- Attack: **Long** (500ms+)
- Release: **Very long** (2s)
- Filter: **Medium** (50-60%)
- Arp Pattern: **RANDOM** or **UP/DN**
- Arp Rate: **Slow** (1/4)
- Arp Octaves: **3-4**
- Delay: **Long time** (500ms+), **FREEZE** often!

### For Rhythmic Sequences
- Waveform: **SQR**
- Attack: **Very short** (1ms)
- Release: **Short** (100ms)
- Arp Pattern: **UP** or **PLAY**
- Arp Rate: **1/32** or **1/16**
- Arp Gate: **50%** (staccato)
- Delay: **SYNC'd**, **60% feedback**, **30% mix**

## 🐛 Troubleshooting

### Arpeggiator not working?
- ✅ Check **ON** button is lit (green LED)
- ✅ Press **SPACE** to start clock
- ✅ Make sure you're holding keys down (or latch is on)

### Delay sounds muddy?
- Turn down **FEEDBACK** knob
- Lower the delay **FILTER** knob (darker repeats)
- Click **CLEAR** button to reset
- Reduce **MIX** percentage

### Can't hear delay?
- Check **ON** button in delay section
- Increase **MIX** knob
- Make sure **MIX** isn't at 0%

### Freeze button not working?
- Make sure delay **ON** is enabled
- Play some notes first (delay needs audio to freeze)
- Check that you can hear delay before freezing

### Clock not syncing?
- Press **SPACE** to start clock
- Check BPM display shows a number
- For tap tempo: tap at least 2 times

### BPM won't change?
- Use **TAP** button repeatedly (at least 2-3 taps)
- Or manually edit (future feature - for now use tap tempo)

## 📊 Current Status

✅ **Completed**:
- [x] 8-voice polyphony
- [x] Transport controls (play/stop, tap tempo)
- [x] Arpeggiator (5 patterns, rate, octaves, gate, latch)
- [x] Loop delay (time, feedback, filter, mix)
- [x] Delay freeze function
- [x] Tempo sync for delay
- [x] Clock pulse visualization
- [x] Keyboard shortcuts
- [x] All visual feedback (LEDs, active states)

⏳ **Coming Next**:
- [ ] 16-step sequencer with grid
- [ ] Voice meter (show active voices)
- [ ] Performance recording/playback
- [ ] Preset system updates (save arp/delay settings)
- [ ] Additional waveforms (wavetable, sample playback)
- [ ] More envelope controls (decay, sustain visible)

## 💡 Pro Tips

1. **Layering**: Use freeze to layer multiple textures
   - Play phrase → Freeze → Play new phrase on top → Freeze again

2. **Feedback Sweeps**: Slowly turn feedback knob while delay is running
   - Creates evolving, dynamic textures

3. **Filter Modulation**: Adjust delay filter in real-time
   - Makes repeats evolve from bright to dark

4. **Pattern Morphing**: Change arp patterns while playing
   - Creates interesting rhythmic variations

5. **Octave Exploration**: Adjust arp octaves while latched
   - Expands harmonic range dynamically

6. **Tempo Experimentation**: Use tap tempo to match external music
   - Play along with songs or loops

7. **Gate Variation**: Adjust gate for different feels
   - Low gate (30%): Choppy, percussive
   - High gate (90%): Smooth, legato

## 🎯 Advanced Techniques

### Infinite Evolving Texture
1. Long attack/release on synth
2. Random arp pattern, 3-4 octaves, slow rate (1/4)
3. Delay with 65% feedback, 60% mix
4. Play chord → Freeze delay
5. Play new chord (original still frozen in delay)
6. Repeat to layer multiple frozen layers

### Rhythmic Delay Sequencing
1. Enable delay SYNC
2. Set feedback to 70%
3. Play single staccato notes in rhythm
4. Adjust TIME knob (1/16, 1/8, 1/4) to create polyrhythms
5. Use CLEAR to reset pattern

### Arpeggio Doubling
1. Set arp to UP pattern
2. Set octaves to 2
3. Set gate to 100% (legato)
4. Add delay with short time (100ms)
5. Creates thick, doubled arp sound

---

## 🎮 Test Checklist

Try these to verify everything works:

- [ ] Press SPACE - clock starts
- [ ] Hold Q+W+E - hear 3 notes
- [ ] Enable arp (A key) - notes arpeggiate
- [ ] Change pattern to RANDOM - random order
- [ ] Press LATCH (L key) - notes continue after release
- [ ] Tap T key 4 times - BPM changes
- [ ] Play notes - hear delay repeats
- [ ] Press FREEZE (F key) - delay holds infinitely
- [ ] Adjust TIME knob - delay time changes
- [ ] Adjust FEEDBACK - more/less repeats
- [ ] Click CLEAR - delay stops immediately
- [ ] Enable SYNC - time shows note divisions
- [ ] Change RATE knob - arp speed changes
- [ ] Change OCTAVES - arp range changes
- [ ] Adjust GATE - note length changes

---

**Server**: http://localhost:8000
**Last Updated**: November 17, 2025
**Status**: ✅ All performance controls fully functional!

🎹 **Have fun creating!** 🎵
