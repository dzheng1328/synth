# Synth 🎹

A browser-based digital synthesizer with retro aesthetic, user authentication, and cloud-ready architecture.

## High-Level Goals

- **Low audio latency** and rock-solid timing
- **Fast, fluid UI** with tactile controls (knobs, sliders, keyboard)
- **Predictable file management** (project files, presets, versioning)
- **Everything keyboard/mouse/MIDI friendly**; full accessibility
- **Clear onboarding** and helpful defaults so new users make music fast

## Features

### Audio Engine
- **2+ Octave Keyboard**: Visual keyboard spanning C3 to C5 (25 keys)
- **Multiple Waveforms**: Sine, Square, Sawtooth, Triangle
- **Octave Transpose**: 8-octave range (0-7) for 200+ playable notes
- **Real-time Controls**:
  - Rotary knobs for tactile feel (Wave, Volume, Attack, Release, Filter)
  - ADSR envelope control
  - Low-pass filter with adjustable cutoff
- **Web Audio API**: Professional-quality audio synthesis

### User System
- **Authentication**: Email/password signup and login
- **Guest Mode**: Use synth without creating account
- **Session Management**: JWT-based auth with 7-day tokens
- **User Dashboard**: Shows logged-in email, logout button

### Design
- **Retro Aesthetic**: Dark metal panel with vintage synth styling
- **Physical Controls**: CSS-based rotary knobs with LED indicators
- **Proper Piano Layout**: Black keys only between C-D, D-E, F-G, G-A, A-B

## Quick Start

### Local Development (No Backend)

1. Open `index.html` in your browser
2. Click "Continue as Guest" or create an account (requires backend)
3. Start playing!

**Keyboard Controls:**
- **Lower octave**: Z X C V B N M (white), S D G H J (black)
- **Upper octave**: Q W E R T Y U I (white), 2 3 5 6 7 (black)
- **Octave shift**: `[` (down), `]` (up)

**Mouse Controls:**
- Click piano keys to play
- Drag knobs up/down to adjust parameters

### Backend Setup (Optional)

For user authentication and future cloud features:

See [BACKEND_SETUP.md](./BACKEND_SETUP.md) for detailed instructions.

Quick version:
```bash
npm install
npm run dev
```

## Technology Stack

**Frontend:**
- Vanilla JavaScript (ES6+)
- Web Audio API for synthesis
- CSS3 for retro UI styling
- localStorage for guest-mode state

**Backend (Optional):**
- Vercel Serverless Functions (Node.js)
- Vercel Postgres (or any Postgres DB)
- JWT authentication
- bcrypt for password hashing

## Project Structure

```
synth/
├── index.html          # Main app
├── style.css           # Retro synth styling
├── synth.js            # Audio engine & keyboard
├── auth.js             # Auth client & UI
├── api/                # Backend API endpoints
│   ├── auth/
│   │   ├── signup.js
│   │   ├── login.js
│   │   └── verify.js
│   └── init-db.js
├── lib/
│   └── auth.js         # Auth utilities
├── package.json
├── vercel.json         # Vercel config
└── BACKEND_SETUP.md    # Backend setup guide
```

## Roadmap

- [ ] Web MIDI input support
- [ ] Preset save/load system
- [ ] Project file export/import
- [ ] AudioWorklet for lower latency
- [ ] Accessibility improvements (ARIA, keyboard nav)
- [ ] First-run tutorial
- [ ] OAuth (Google/Apple)
- [ ] Cloud project sync
- [ ] Collaboration features

## Contributing

This is a learning project. Feel free to fork and experiment!

## License

MIT
