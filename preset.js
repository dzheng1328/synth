/**
 * Preset & Patch System for Synthesizer
 * 
 * Features:
 * - Factory presets (read-only) and user presets
 * - Categories: bass, lead, pad, keys, fx
 * - Tags and search
 * - Favorites/starred presets
 * - Preset morphing (crossfade)
 * - Rich metadata (author, BPM, key, tags, screenshot)
 */

// Preset categories
const PRESET_CATEGORIES = {
  BASS: 'bass',
  LEAD: 'lead',
  PAD: 'pad',
  KEYS: 'keys',
  FX: 'fx',
  ALL: 'all'
};

// Preset class with metadata
class PresetPatch {
  constructor(options = {}) {
    this.id = options.id || this.generateId();
    this.name = options.name || 'Untitled Preset';
    this.category = options.category || PRESET_CATEGORIES.LEAD;
    this.isFactory = options.isFactory || false;
    this.isFavorite = options.isFavorite || false;
    
    // Sound parameters (from knobControllers state)
    this.patch = options.patch || {
      waveform: 'sine',
      volume: 0.5,
      attack: 0.01,
      release: 0.3,
      cutoff: 1000,
      resonance: 1,
      octave: 3
    };
    
    // Metadata
    this.metadata = {
      author: options.metadata?.author || 'Anonymous',
      bpm: options.metadata?.bpm || null,
      key: options.metadata?.key || null,
      tags: options.metadata?.tags || [],
      description: options.metadata?.description || '',
      screenshot: options.metadata?.screenshot || null,
      created: options.metadata?.created || new Date().toISOString(),
      modified: options.metadata?.modified || new Date().toISOString()
    };
  }
  
  generateId() {
    return 'preset_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
  }
  
  toJSON() {
    return {
      id: this.id,
      name: this.name,
      category: this.category,
      isFactory: this.isFactory,
      isFavorite: this.isFavorite,
      patch: this.patch,
      metadata: this.metadata
    };
  }
  
  static fromJSON(json) {
    return new PresetPatch(json);
  }
  
  // Create deep copy
  duplicate(newName) {
    const copy = PresetPatch.fromJSON(this.toJSON());
    copy.id = copy.generateId();
    copy.name = newName || `${this.name} (Copy)`;
    copy.isFactory = false; // User copies are never factory
    copy.isFavorite = false;
    copy.metadata.created = new Date().toISOString();
    copy.metadata.modified = new Date().toISOString();
    copy.metadata.author = 'User'; // Reset author for copies
    return copy;
  }
}

// Preset Manager - handles all preset operations
class PresetManager {
  constructor() {
    this.factoryPresets = [];
    this.userPresets = [];
    this.currentPreset = null;
    this.morphState = {
      enabled: false,
      presetA: null,
      presetB: null,
      mixAmount: 0.5 // 0 = full A, 1 = full B
    };
    
    this.loadFactoryPresets();
    this.loadUserPresets();
  }
  
  // Load factory presets (bundled with app)
  loadFactoryPresets() {
    this.factoryPresets = FACTORY_PRESETS.map(preset => 
      PresetPatch.fromJSON({ ...preset, isFactory: true })
    );
  }
  
  // Load user presets from localStorage
  loadUserPresets() {
    try {
      const stored = localStorage.getItem('synth_user_presets');
      if (stored) {
        const presets = JSON.parse(stored);
        this.userPresets = presets.map(p => PresetPatch.fromJSON(p));
      }
    } catch (error) {
      console.error('Failed to load user presets:', error);
      this.userPresets = [];
    }
  }
  
  // Save user presets to localStorage
  saveUserPresets() {
    try {
      const json = this.userPresets.map(p => p.toJSON());
      localStorage.setItem('synth_user_presets', JSON.stringify(json));
    } catch (error) {
      console.error('Failed to save user presets:', error);
    }
  }
  
  // Get all presets (factory + user)
  getAllPresets() {
    return [...this.factoryPresets, ...this.userPresets];
  }
  
  // Get presets by category
  getPresetsByCategory(category) {
    if (category === PRESET_CATEGORIES.ALL) {
      return this.getAllPresets();
    }
    return this.getAllPresets().filter(p => p.category === category);
  }
  
  // Get favorite presets
  getFavoritePresets() {
    return this.getAllPresets().filter(p => p.isFavorite);
  }
  
  // Search presets by name or tags
  searchPresets(query) {
    const lowerQuery = query.toLowerCase();
    return this.getAllPresets().filter(preset => {
      return preset.name.toLowerCase().includes(lowerQuery) ||
             preset.metadata.tags.some(tag => tag.toLowerCase().includes(lowerQuery)) ||
             preset.metadata.description.toLowerCase().includes(lowerQuery);
    });
  }
  
  // Get preset by ID
  getPresetById(id) {
    return this.getAllPresets().find(p => p.id === id);
  }
  
  // Save current synth state as user preset
  saveCurrentAsPreset(name, category, metadata = {}) {
    const patch = this.captureCurrentPatch();
    
    const preset = new PresetPatch({
      name,
      category,
      patch,
      metadata: {
        ...metadata,
        author: 'User',
        created: new Date().toISOString(),
        modified: new Date().toISOString()
      }
    });
    
    this.userPresets.push(preset);
    this.saveUserPresets();
    this.currentPreset = preset;
    
    return preset;
  }
  
  // Update existing user preset
  updatePreset(presetId, updates) {
    const preset = this.userPresets.find(p => p.id === presetId);
    if (!preset) {
      throw new Error('Preset not found or is read-only');
    }
    
    Object.assign(preset, updates);
    preset.metadata.modified = new Date().toISOString();
    this.saveUserPresets();
    
    return preset;
  }
  
  // Delete user preset
  deletePreset(presetId) {
    const index = this.userPresets.findIndex(p => p.id === presetId);
    if (index === -1) {
      throw new Error('Preset not found or cannot be deleted');
    }
    
    this.userPresets.splice(index, 1);
    this.saveUserPresets();
    
    if (this.currentPreset?.id === presetId) {
      this.currentPreset = null;
    }
  }
  
  // Toggle favorite status
  toggleFavorite(presetId) {
    const preset = this.getPresetById(presetId);
    if (preset) {
      preset.isFavorite = !preset.isFavorite;
      
      // Save to appropriate storage
      if (preset.isFactory) {
        // Store factory favorites separately (can't modify factory presets)
        this.saveFactoryFavorites();
      } else {
        this.saveUserPresets();
      }
      
      return preset.isFavorite;
    }
    return false;
  }
  
  // Save factory favorites to localStorage
  saveFactoryFavorites() {
    try {
      const favorites = this.factoryPresets
        .filter(p => p.isFavorite)
        .map(p => p.id);
      localStorage.setItem('synth_factory_favorites', JSON.stringify(favorites));
    } catch (error) {
      console.error('Failed to save factory favorites:', error);
    }
  }
  
  // Load factory favorites from localStorage
  loadFactoryFavorites() {
    try {
      const stored = localStorage.getItem('synth_factory_favorites');
      if (stored) {
        const favoriteIds = JSON.parse(stored);
        this.factoryPresets.forEach(preset => {
          preset.isFavorite = favoriteIds.includes(preset.id);
        });
      }
    } catch (error) {
      console.error('Failed to load factory favorites:', error);
    }
  }
  
  // Capture current synth state
  captureCurrentPatch() {
    const patch = {
      waveform: 'sine',
      volume: 0.5,
      attack: 0.01,
      release: 0.3,
      cutoff: 1000,
      resonance: 1,
      octave: typeof currentOctave !== 'undefined' ? currentOctave : 3
    };
    
    // Read from knobControllers if available
    if (typeof knobControllers !== 'undefined' && knobControllers) {
      if (knobControllers.volume) {
        patch.volume = parseFloat(knobControllers.volume.element.dataset.value);
      }
      if (knobControllers.attack) {
        patch.attack = parseFloat(knobControllers.attack.element.dataset.value);
      }
      if (knobControllers.release) {
        patch.release = parseFloat(knobControllers.release.element.dataset.value);
      }
      if (knobControllers.cutoff) {
        patch.cutoff = parseFloat(knobControllers.cutoff.element.dataset.value);
      }
      if (knobControllers.resonance) {
        patch.resonance = parseFloat(knobControllers.resonance.element.dataset.value);
      }
    }
    
    // Read waveform from synth
    if (typeof synth !== 'undefined' && synth && synth.waveform) {
      patch.waveform = synth.waveform;
    }
    
    return patch;
  }
  
  // Apply preset to synth
  applyPreset(presetId) {
    const preset = this.getPresetById(presetId);
    if (!preset) {
      throw new Error('Preset not found');
    }
    
    this.applyPatch(preset.patch);
    this.currentPreset = preset;
    
    return preset;
  }
  
  // Apply patch data to synth
  applyPatch(patch) {
    // Apply to knobControllers
    if (typeof knobControllers !== 'undefined' && knobControllers) {
      if (knobControllers.volume && patch.volume !== undefined) {
        knobControllers.volume.setValue(patch.volume);
      }
      if (knobControllers.attack && patch.attack !== undefined) {
        knobControllers.attack.setValue(patch.attack);
      }
      if (knobControllers.release && patch.release !== undefined) {
        knobControllers.release.setValue(patch.release);
      }
      if (knobControllers.cutoff && patch.cutoff !== undefined) {
        knobControllers.cutoff.setValue(patch.cutoff);
      }
      if (knobControllers.resonance && patch.resonance !== undefined) {
        knobControllers.resonance.setValue(patch.resonance);
      }
    }
    
    // Apply waveform
    if (typeof synth !== 'undefined' && synth && patch.waveform) {
      synth.waveform = patch.waveform;
      // Update waveform buttons
      document.querySelectorAll('.waveform-button').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.waveform === patch.waveform);
      });
    }
    
    // Apply octave
    if (patch.octave !== undefined && typeof currentOctave !== 'undefined') {
      window.currentOctave = patch.octave;
      const octaveDisplay = document.getElementById('octave-display');
      if (octaveDisplay) {
        octaveDisplay.textContent = patch.octave;
      }
    }
  }
  
  // Preset morphing - interpolate between two presets
  enableMorph(presetAId, presetBId) {
    const presetA = this.getPresetById(presetAId);
    const presetB = this.getPresetById(presetBId);
    
    if (!presetA || !presetB) {
      throw new Error('Both presets required for morphing');
    }
    
    this.morphState.enabled = true;
    this.morphState.presetA = presetA;
    this.morphState.presetB = presetB;
    this.morphState.mixAmount = 0.5;
    
    this.applyMorph();
  }
  
  // Disable morphing
  disableMorph() {
    this.morphState.enabled = false;
    this.morphState.presetA = null;
    this.morphState.presetB = null;
  }
  
  // Set morph mix amount (0-1)
  setMorphAmount(amount) {
    this.morphState.mixAmount = Math.max(0, Math.min(1, amount));
    if (this.morphState.enabled) {
      this.applyMorph();
    }
  }
  
  // Apply morphed patch
  applyMorph() {
    if (!this.morphState.enabled || !this.morphState.presetA || !this.morphState.presetB) {
      return;
    }
    
    const patchA = this.morphState.presetA.patch;
    const patchB = this.morphState.presetB.patch;
    const mix = this.morphState.mixAmount;
    
    // Interpolate numeric values
    const morphedPatch = {
      volume: this.lerp(patchA.volume, patchB.volume, mix),
      attack: this.lerp(patchA.attack, patchB.attack, mix),
      release: this.lerp(patchA.release, patchB.release, mix),
      cutoff: this.lerp(patchA.cutoff, patchB.cutoff, mix),
      resonance: this.lerp(patchA.resonance, patchB.resonance, mix),
      octave: Math.round(this.lerp(patchA.octave, patchB.octave, mix)),
      // For waveform, switch at 50% threshold
      waveform: mix < 0.5 ? patchA.waveform : patchB.waveform
    };
    
    this.applyPatch(morphedPatch);
  }
  
  // Linear interpolation helper
  lerp(a, b, t) {
    return a + (b - a) * t;
  }
  
  // Export preset to file
  exportPreset(presetId) {
    const preset = this.getPresetById(presetId);
    if (!preset) {
      throw new Error('Preset not found');
    }
    
    const json = JSON.stringify(preset.toJSON(), null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `${preset.name}.synthpreset`;
    a.click();
    
    URL.revokeObjectURL(url);
  }
  
  // Import preset from file
  async importPreset(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      
      reader.onload = (e) => {
        try {
          const json = JSON.parse(e.target.result);
          const preset = PresetPatch.fromJSON(json);
          
          // Ensure it's saved as user preset
          preset.isFactory = false;
          preset.id = preset.generateId(); // New ID to avoid conflicts
          
          this.userPresets.push(preset);
          this.saveUserPresets();
          
          resolve(preset);
        } catch (error) {
          reject(new Error('Invalid preset file: ' + error.message));
        }
      };
      
      reader.onerror = () => reject(new Error('Failed to read file'));
      reader.readAsText(file);
    });
  }
}

// Factory Presets Library
const FACTORY_PRESETS = [
  // BASS presets
  {
    id: 'factory_bass_deep',
    name: 'Deep Bass',
    category: 'bass',
    patch: {
      waveform: 'square',
      volume: 0.7,
      attack: 0.001,
      release: 0.3,
      cutoff: 300,
      resonance: 2,
      octave: 1
    },
    metadata: {
      author: 'Factory',
      tags: ['bass', 'deep', 'sub'],
      description: 'Deep sub bass with tight envelope',
      bpm: null,
      key: null
    }
  },
  {
    id: 'factory_bass_wobble',
    name: 'Wobble Bass',
    category: 'bass',
    patch: {
      waveform: 'sawtooth',
      volume: 0.65,
      attack: 0.01,
      release: 0.2,
      cutoff: 500,
      resonance: 5,
      octave: 2
    },
    metadata: {
      author: 'Factory',
      tags: ['bass', 'wobble', 'dubstep'],
      description: 'High resonance bass for wobble effects'
    }
  },
  {
    id: 'factory_bass_808',
    name: '808 Bass',
    category: 'bass',
    patch: {
      waveform: 'sine',
      volume: 0.8,
      attack: 0.001,
      release: 0.5,
      cutoff: 200,
      resonance: 1,
      octave: 1
    },
    metadata: {
      author: 'Factory',
      tags: ['bass', '808', 'trap', 'hiphop'],
      description: 'Classic 808-style bass'
    }
  },
  {
    id: 'factory_bass_reese',
    name: 'Reese Bass',
    category: 'bass',
    patch: {
      waveform: 'sawtooth',
      volume: 0.6,
      attack: 0.01,
      release: 0.4,
      cutoff: 400,
      resonance: 3,
      octave: 2
    },
    metadata: {
      author: 'Factory',
      tags: ['bass', 'reese', 'dnb', 'dark'],
      description: 'Dark detuned bass'
    }
  },
  {
    id: 'factory_bass_funk',
    name: 'Funk Bass',
    category: 'bass',
    patch: {
      waveform: 'square',
      volume: 0.65,
      attack: 0.005,
      release: 0.15,
      cutoff: 600,
      resonance: 2,
      octave: 2
    },
    metadata: {
      author: 'Factory',
      tags: ['bass', 'funk', 'punchy'],
      description: 'Punchy bass for funk grooves'
    }
  },
  
  // LEAD presets
  {
    id: 'factory_lead_bright',
    name: 'Bright Lead',
    category: 'lead',
    patch: {
      waveform: 'sawtooth',
      volume: 0.6,
      attack: 0.01,
      release: 0.3,
      cutoff: 2000,
      resonance: 2,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'bright', 'cutting'],
      description: 'Bright sawtooth lead that cuts through mix'
    }
  },
  {
    id: 'factory_lead_pluck',
    name: 'Pluck Lead',
    category: 'lead',
    patch: {
      waveform: 'square',
      volume: 0.55,
      attack: 0.001,
      release: 0.15,
      cutoff: 1500,
      resonance: 1.5,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'pluck', 'short'],
      description: 'Short plucky lead for fast melodies'
    }
  },
  {
    id: 'factory_lead_sync',
    name: 'Sync Lead',
    category: 'lead',
    patch: {
      waveform: 'sawtooth',
      volume: 0.5,
      attack: 0.02,
      release: 0.25,
      cutoff: 3000,
      resonance: 4,
      octave: 5
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'sync', 'aggressive', 'edm'],
      description: 'Aggressive high-resonance lead'
    }
  },
  {
    id: 'factory_lead_smooth',
    name: 'Smooth Lead',
    category: 'lead',
    patch: {
      waveform: 'triangle',
      volume: 0.6,
      attack: 0.05,
      release: 0.4,
      cutoff: 1200,
      resonance: 1,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'smooth', 'mellow'],
      description: 'Smooth triangle lead for melodies'
    }
  },
  {
    id: 'factory_lead_screamer',
    name: 'Screamer',
    category: 'lead',
    patch: {
      waveform: 'square',
      volume: 0.65,
      attack: 0.001,
      release: 0.5,
      cutoff: 4000,
      resonance: 6,
      octave: 5
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'aggressive', 'scream', 'trance'],
      description: 'High-pitched screaming lead'
    }
  },
  {
    id: 'factory_lead_soft',
    name: 'Soft Lead',
    category: 'lead',
    patch: {
      waveform: 'sine',
      volume: 0.5,
      attack: 0.08,
      release: 0.35,
      cutoff: 800,
      resonance: 0.5,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'soft', 'gentle'],
      description: 'Gentle sine lead for quiet passages'
    }
  },
  {
    id: 'factory_lead_stab',
    name: 'Stab Lead',
    category: 'lead',
    patch: {
      waveform: 'sawtooth',
      volume: 0.7,
      attack: 0.001,
      release: 0.1,
      cutoff: 2500,
      resonance: 3,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['lead', 'stab', 'short', 'punchy'],
      description: 'Short punchy stab lead'
    }
  },
  
  // PAD presets
  {
    id: 'factory_pad_warm',
    name: 'Warm Pad',
    category: 'pad',
    patch: {
      waveform: 'triangle',
      volume: 0.4,
      attack: 0.5,
      release: 1.5,
      cutoff: 800,
      resonance: 0.5,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'warm', 'ambient'],
      description: 'Warm evolving pad'
    }
  },
  {
    id: 'factory_pad_bright',
    name: 'Bright Pad',
    category: 'pad',
    patch: {
      waveform: 'sawtooth',
      volume: 0.35,
      attack: 0.8,
      release: 2.0,
      cutoff: 1500,
      resonance: 1,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'bright', 'lush'],
      description: 'Bright lush pad'
    }
  },
  {
    id: 'factory_pad_dark',
    name: 'Dark Pad',
    category: 'pad',
    patch: {
      waveform: 'sine',
      volume: 0.45,
      attack: 1.0,
      release: 2.5,
      cutoff: 400,
      resonance: 0.3,
      octave: 2
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'dark', 'cinematic'],
      description: 'Dark cinematic pad'
    }
  },
  {
    id: 'factory_pad_strings',
    name: 'String Pad',
    category: 'pad',
    patch: {
      waveform: 'sawtooth',
      volume: 0.4,
      attack: 0.6,
      release: 1.8,
      cutoff: 1200,
      resonance: 1.5,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'strings', 'orchestral'],
      description: 'String ensemble pad'
    }
  },
  {
    id: 'factory_pad_sweep',
    name: 'Sweep Pad',
    category: 'pad',
    patch: {
      waveform: 'square',
      volume: 0.35,
      attack: 1.2,
      release: 2.0,
      cutoff: 2000,
      resonance: 3,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'sweep', 'evolving'],
      description: 'Sweeping evolving pad'
    }
  },
  {
    id: 'factory_pad_space',
    name: 'Space Pad',
    category: 'pad',
    patch: {
      waveform: 'sine',
      volume: 0.3,
      attack: 2.0,
      release: 3.0,
      cutoff: 600,
      resonance: 0.8,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['pad', 'space', 'atmospheric', 'ambient'],
      description: 'Atmospheric space pad'
    }
  },
  
  // KEYS presets
  {
    id: 'factory_keys_piano',
    name: 'Piano Keys',
    category: 'keys',
    patch: {
      waveform: 'triangle',
      volume: 0.6,
      attack: 0.01,
      release: 0.5,
      cutoff: 2000,
      resonance: 0.5,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['keys', 'piano', 'acoustic'],
      description: 'Piano-like keys'
    }
  },
  {
    id: 'factory_keys_electric',
    name: 'Electric Piano',
    category: 'keys',
    patch: {
      waveform: 'sine',
      volume: 0.55,
      attack: 0.005,
      release: 0.8,
      cutoff: 1500,
      resonance: 1,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['keys', 'electric', 'rhodes'],
      description: 'Electric piano sound'
    }
  },
  {
    id: 'factory_keys_organ',
    name: 'Organ',
    category: 'keys',
    patch: {
      waveform: 'square',
      volume: 0.5,
      attack: 0.001,
      release: 0.1,
      cutoff: 3000,
      resonance: 0.3,
      octave: 3
    },
    metadata: {
      author: 'Factory',
      tags: ['keys', 'organ', 'church'],
      description: 'Organ-style keys'
    }
  },
  {
    id: 'factory_keys_clav',
    name: 'Clavinet',
    category: 'keys',
    patch: {
      waveform: 'square',
      volume: 0.6,
      attack: 0.001,
      release: 0.05,
      cutoff: 2500,
      resonance: 4,
      octave: 4
    },
    metadata: {
      author: 'Factory',
      tags: ['keys', 'clav', 'funk'],
      description: 'Funky clavinet keys'
    }
  },
  {
    id: 'factory_keys_bell',
    name: 'Bell Keys',
    category: 'keys',
    patch: {
      waveform: 'sine',
      volume: 0.5,
      attack: 0.001,
      release: 1.5,
      cutoff: 4000,
      resonance: 2,
      octave: 5
    },
    metadata: {
      author: 'Factory',
      tags: ['keys', 'bell', 'bright'],
      description: 'Bright bell-like keys'
    }
  },
  
  // FX presets
  {
    id: 'factory_fx_noise',
    name: 'Noise Sweep',
    category: 'fx',
    patch: {
      waveform: 'sawtooth',
      volume: 0.4,
      attack: 0.5,
      release: 1.0,
      cutoff: 5000,
      resonance: 8,
      octave: 6
    },
    metadata: {
      author: 'Factory',
      tags: ['fx', 'noise', 'sweep', 'riser'],
      description: 'Noise sweep FX'
    }
  },
  {
    id: 'factory_fx_impact',
    name: 'Impact',
    category: 'fx',
    patch: {
      waveform: 'square',
      volume: 0.8,
      attack: 0.001,
      release: 0.3,
      cutoff: 100,
      resonance: 10,
      octave: 0
    },
    metadata: {
      author: 'Factory',
      tags: ['fx', 'impact', 'hit'],
      description: 'Low impact sound'
    }
  },
  {
    id: 'factory_fx_laser',
    name: 'Laser',
    category: 'fx',
    patch: {
      waveform: 'sawtooth',
      volume: 0.5,
      attack: 0.001,
      release: 0.2,
      cutoff: 8000,
      resonance: 10,
      octave: 7
    },
    metadata: {
      author: 'Factory',
      tags: ['fx', 'laser', 'sci-fi'],
      description: 'Sci-fi laser sound'
    }
  },
  {
    id: 'factory_fx_wind',
    name: 'Wind',
    category: 'fx',
    patch: {
      waveform: 'sine',
      volume: 0.3,
      attack: 2.0,
      release: 3.0,
      cutoff: 300,
      resonance: 0.1,
      octave: 1
    },
    metadata: {
      author: 'Factory',
      tags: ['fx', 'wind', 'ambient', 'atmos'],
      description: 'Wind atmosphere'
    }
  },
  {
    id: 'factory_fx_alarm',
    name: 'Alarm',
    category: 'fx',
    patch: {
      waveform: 'square',
      volume: 0.6,
      attack: 0.001,
      release: 0.1,
      cutoff: 2000,
      resonance: 5,
      octave: 5
    },
    metadata: {
      author: 'Factory',
      tags: ['fx', 'alarm', 'siren'],
      description: 'Alarm/siren sound'
    }
  }
];

// Initialize preset manager globally
let presetManager = null;

// Initialize on page load
if (typeof document !== 'undefined') {
  document.addEventListener('DOMContentLoaded', () => {
    presetManager = new PresetManager();
    presetManager.loadFactoryFavorites();
    console.log('Preset system initialized:', {
      factoryPresets: presetManager.factoryPresets.length,
      userPresets: presetManager.userPresets.length
    });
  });
}
