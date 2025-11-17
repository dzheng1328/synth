// Project file format and management
const PROJECT_VERSION = '1.0.0';

// Project templates
const PROJECT_TEMPLATES = {
  empty: {
    name: 'Empty Project',
    description: 'Start from scratch',
    icon: '📄',
    patch: {
      waveform: 'sine',
      volume: 50,
      attack: 5,
      release: 30,
      filter: 50,
      octave: 3
    }
  },
  synth: {
    name: 'Synth Lead',
    description: 'Bright lead synthesizer',
    icon: '🎹',
    patch: {
      waveform: 'sawtooth',
      volume: 60,
      attack: 10,
      release: 40,
      filter: 70,
      octave: 4
    }
  },
  bass: {
    name: 'Bass Patch',
    description: 'Deep bass sound',
    icon: '🔊',
    patch: {
      waveform: 'square',
      volume: 70,
      attack: 5,
      release: 20,
      filter: 30,
      octave: 2
    }
  },
  pad: {
    name: 'Pad Sound',
    description: 'Warm ambient pad',
    icon: '☁️',
    patch: {
      waveform: 'triangle',
      volume: 40,
      attack: 80,
      release: 100,
      filter: 50,
      octave: 3
    }
  },
  ambient: {
    name: 'Ambient Texture',
    description: 'Atmospheric soundscape',
    icon: '🌌',
    patch: {
      waveform: 'sine',
      volume: 35,
      attack: 100,
      release: 150,
      filter: 40,
      octave: 3
    }
  }
};

// Project file structure
class SynthProject {
  constructor(name = 'Untitled Project', template = 'empty') {
    this.version = PROJECT_VERSION;
    this.name = name;
    this.id = this.generateId();
    this.created = new Date().toISOString();
    this.modified = new Date().toISOString();
    this.author = window.authAPI?.currentUser?.email || 'Guest';
    
    // Load template
    const templateData = PROJECT_TEMPLATES[template] || PROJECT_TEMPLATES.empty;
    this.patch = { ...templateData.patch };
    
    // Project settings
    this.settings = {
      tempo: 120,
      masterVolume: 1.0,
      sampleRate: 44100
    };
    
    // Metadata
    this.metadata = {
      thumbnail: null,
      tags: [],
      notes: ''
    };
    
    // File references (for future use)
    this.files = {
      samples: [],
      presets: []
    };
  }
  
  generateId() {
    return `proj_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }
  
  updateModified() {
    this.modified = new Date().toISOString();
  }
  
  toJSON() {
    return {
      version: this.version,
      name: this.name,
      id: this.id,
      created: this.created,
      modified: this.modified,
      author: this.author,
      patch: this.patch,
      settings: this.settings,
      metadata: this.metadata,
      files: this.files
    };
  }
  
  static fromJSON(json) {
    const project = new SynthProject();
    Object.assign(project, json);
    return project;
  }
}

// Project Manager - handles save/load/autosave
class ProjectManager {
  constructor() {
    this.currentProject = null;
    this.recentProjects = this.loadRecentProjects();
    this.autosaveEnabled = true;
    this.autosaveInterval = 30000; // 30 seconds
    this.autosaveTimer = null;
    this.hasUnsavedChanges = false;
    this.lastManualSave = null;
  }
  
  // Create new project
  newProject(name, template = 'empty') {
    this.stopAutosave();
    this.currentProject = new SynthProject(name, template);
    this.hasUnsavedChanges = false;
    this.startAutosave();
    return this.currentProject;
  }
  
  // Get current state from synth
  captureCurrentState() {
    if (!this.currentProject) return null;
    
    // Get current synth settings
    const waveformKnob = document.getElementById('waveform-knob');
    const volumeKnob = document.getElementById('volume-knob');
    const attackKnob = document.getElementById('attack-knob');
    const releaseKnob = document.getElementById('release-knob');
    const filterKnob = document.getElementById('filter-knob');
    const octaveValue = document.getElementById('octave-value');
    
    this.currentProject.patch = {
      waveform: ['sine', 'square', 'sawtooth', 'triangle'][Math.round(parseFloat(waveformKnob?.dataset.value || 2))],
      volume: parseFloat(volumeKnob?.dataset.value || 50),
      attack: parseFloat(attackKnob?.dataset.value || 5),
      release: parseFloat(releaseKnob?.dataset.value || 30),
      filter: parseFloat(filterKnob?.dataset.value || 50),
      octave: parseInt(octaveValue?.textContent || 3)
    };
    
    this.currentProject.updateModified();
    this.hasUnsavedChanges = true;
    
    return this.currentProject;
  }
  
  // Apply project state to synth
  applyProjectState(project) {
    if (!project || !project.patch) return;
    
    // Update synth from project
    const patch = project.patch;
    
    // Update knobs
    const waveforms = ['sine', 'square', 'sawtooth', 'triangle'];
    const waveIndex = waveforms.indexOf(patch.waveform);
    
    if (window.synth) {
      window.synth.updateSettings('waveform', patch.waveform);
      window.synth.updateSettings('volume', patch.volume / 100);
      window.synth.updateSettings('attack', patch.attack / 1000);
      window.synth.updateSettings('release', patch.release / 100);
      window.synth.updateSettings('filterFrequency', 100 + (patch.filter / 100) * 9900);
    }
    
    // Update UI (knobs and octave)
    if (window.knobControllers) {
      window.knobControllers.waveform?.setValue(waveIndex);
      window.knobControllers.volume?.setValue(patch.volume);
      window.knobControllers.attack?.setValue(patch.attack);
      window.knobControllers.release?.setValue(patch.release);
      window.knobControllers.filter?.setValue(patch.filter);
    }
    
    // Update octave
    if (window.currentOctave !== undefined) {
      window.currentOctave = patch.octave;
      const octaveDisplay = document.getElementById('octave-value');
      if (octaveDisplay) octaveDisplay.textContent = patch.octave;
    }
    
    this.currentProject = project;
    this.hasUnsavedChanges = false;
  }
  
  // Save project
  saveProject(saveAs = false) {
    if (!this.currentProject) return null;
    
    this.captureCurrentState();
    
    const projectData = JSON.stringify(this.currentProject.toJSON(), null, 2);
    const blob = new Blob([projectData], { type: 'application/json' });
    const filename = `${this.currentProject.name.replace(/[^a-z0-9]/gi, '_')}.synthproj`;
    
    // Download file
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
    
    // Update recent projects
    this.addToRecent(this.currentProject);
    this.lastManualSave = new Date();
    this.hasUnsavedChanges = false;
    
    return filename;
  }
  
  // Load project from file
  async loadProject(file) {
    try {
      const text = await file.text();
      const json = JSON.parse(text);
      const project = SynthProject.fromJSON(json);
      
      this.stopAutosave();
      this.applyProjectState(project);
      this.addToRecent(project);
      this.startAutosave();
      
      return project;
    } catch (error) {
      console.error('Failed to load project:', error);
      throw new Error('Invalid project file');
    }
  }
  
  // Autosave to localStorage
  autosave() {
    if (!this.currentProject || !this.hasUnsavedChanges) return;
    
    this.captureCurrentState();
    const autosaveData = {
      project: this.currentProject.toJSON(),
      timestamp: new Date().toISOString()
    };
    
    localStorage.setItem('synth_autosave', JSON.stringify(autosaveData));
    console.log('Autosaved project');
  }
  
  // Load autosave
  loadAutosave() {
    const saved = localStorage.getItem('synth_autosave');
    if (!saved) return null;
    
    try {
      const data = JSON.parse(saved);
      return SynthProject.fromJSON(data.project);
    } catch (error) {
      console.error('Failed to load autosave:', error);
      return null;
    }
  }
  
  // Clear autosave
  clearAutosave() {
    localStorage.removeItem('synth_autosave');
  }
  
  // Start autosave timer
  startAutosave() {
    if (!this.autosaveEnabled) return;
    
    this.stopAutosave();
    this.autosaveTimer = setInterval(() => {
      this.autosave();
    }, this.autosaveInterval);
  }
  
  // Stop autosave timer
  stopAutosave() {
    if (this.autosaveTimer) {
      clearInterval(this.autosaveTimer);
      this.autosaveTimer = null;
    }
  }
  
  // Recent projects management
  addToRecent(project) {
    const recent = {
      name: project.name,
      id: project.id,
      modified: project.modified,
      author: project.author,
      patch: project.patch
    };
    
    // Remove duplicates
    this.recentProjects = this.recentProjects.filter(p => p.id !== project.id);
    
    // Add to front
    this.recentProjects.unshift(recent);
    
    // Keep only last 10
    this.recentProjects = this.recentProjects.slice(0, 10);
    
    // Save to localStorage
    localStorage.setItem('synth_recent_projects', JSON.stringify(this.recentProjects));
  }
  
  loadRecentProjects() {
    const saved = localStorage.getItem('synth_recent_projects');
    if (!saved) return [];
    
    try {
      return JSON.parse(saved);
    } catch (error) {
      return [];
    }
  }
  
  // Export project as ZIP (simplified - just JSON for now)
  exportProjectBundle() {
    if (!this.currentProject) return null;
    
    this.captureCurrentState();
    
    // For now, just export JSON. In future, use JSZip to bundle samples/presets
    const projectData = JSON.stringify(this.currentProject.toJSON(), null, 2);
    const blob = new Blob([projectData], { type: 'application/json' });
    const filename = `${this.currentProject.name}_bundle.synthproj`;
    
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
    
    return filename;
  }
}

// Export
window.PROJECT_TEMPLATES = PROJECT_TEMPLATES;
window.SynthProject = SynthProject;
window.ProjectManager = ProjectManager;
