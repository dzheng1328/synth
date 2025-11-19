# Test Harnesses

This folder holds standalone C harnesses that exercise portions of the audio engine outside the GUI builds.

## `audio_checklist_test.c`

Automates the entire “To Verify Audio Engine” checklist from `AUDIO_ENGINE_STATUS.md` without launching the GUI builds. The harness renders buffers directly through `synth_engine` and applies lightweight models of the FX rack so every item can be validated headlessly.

### Coverage (All Headless ✅)
1. **Single note (Z key)** – trigger middle C and log RMS.
2. **Waveform switch to saw** – confirm output diverges from sine.
3. **Unison stack** – enable 5-voice unison and compare buffers.
4. **Filter sweep** – render bright vs dark cutoff cases and compare RMS.
5. **ADSR changes** – re-render with slow attack / long release and log ramp durations.
6. **Polyphony** – schedule 8 overlapping notes and ensure non-zero output across all voices.
7. **Master volume** – change volume and confirm near-linear scaling.
8. **Delay** – enable the modeled delay line and confirm late-buffer energy.
9. **Reverb** – enable the modeled comb reverb and measure tail energy.
10. **Distortion** – enable distortion and compare clipped vs unclipped crest factors.

### Implementation Notes
- Uses only `synth_engine.c` plus small, inline replicas of the production FX processors (tanh distortion, feedback delay, feedback comb reverb).
- Generates short buffers per test (44.1 kHz) and records summary metrics (RMS, peak, crest, segment RMS) for PASS/FAIL decisions.
- Runs in well under a second, so it can be wired into CI or executed manually after DSP changes.

### Build & Run

```sh
cd /Users/dzheng/Documents/synth
gcc tests/audio_checklist_test.c synth_engine.c -o audio_checklist_test -lm
./audio_checklist_test
```

The binary prints a checklist-style report and returns a non-zero exit code if any item fails its thresholds.
