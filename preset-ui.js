/**
 * Preset Browser UI
 * 
 * Handles all UI interactions for the preset system:
 * - Preset browser modal with category tabs
 * - Search and filtering
 * - Favorite/star management
 * - Preview playback
 * - Save/delete operations
 * - Preset morphing UI
 */

class PresetUI {
  constructor(presetManager) {
    this.presetManager = presetManager;
    this.currentCategory = 'all';
    this.searchQuery = '';
    this.morphEnabled = false;
    
    this.initUI();
    this.attachEventListeners();
  }
  
  initUI() {
    // Add preset browser button to file menu
    this.addPresetButton();
    
    // Create preset browser modal
    this.createPresetBrowserModal();
    
    // Create save preset dialog
    this.createSavePresetDialog();
    
    // Create morph UI
    this.createMorphUI();
  }
  
  addPresetButton() {
    const fileMenu = document.querySelector('.file-menu-bar');
    if (!fileMenu) return;
    
    const presetBtn = document.createElement('button');
    presetBtn.className = 'file-menu-button';
    presetBtn.innerHTML = '🎨 Presets';
    presetBtn.title = 'Browse presets (Ctrl+P)';
    presetBtn.onclick = () => this.showPresetBrowser();
    
    // Insert after "Export" button
    const exportBtn = fileMenu.querySelector('button:last-child');
    if (exportBtn) {
      exportBtn.insertAdjacentElement('afterend', presetBtn);
    } else {
      fileMenu.appendChild(presetBtn);
    }
    
    // Add save preset button
    const savePresetBtn = document.createElement('button');
    savePresetBtn.className = 'file-menu-button';
    savePresetBtn.innerHTML = '💾 Save Preset';
    savePresetBtn.title = 'Save current sound as preset';
    savePresetBtn.onclick = () => this.showSavePresetDialog();
    presetBtn.insertAdjacentElement('afterend', savePresetBtn);
  }
  
  createPresetBrowserModal() {
    const modal = document.createElement('div');
    modal.id = 'preset-browser-modal';
    modal.className = 'modal-overlay';
    modal.style.display = 'none';
    
    modal.innerHTML = `
      <div class="modal-content preset-browser-content">
        <div class="modal-header">
          <h2>🎨 Preset Browser</h2>
          <button class="modal-close" onclick="presetUI.hidePresetBrowser()">✕</button>
        </div>
        
        <div class="preset-browser-controls">
          <div class="preset-search-bar">
            <input type="text" id="preset-search" placeholder="Search presets..." />
            <button class="preset-favorites-toggle" id="show-favorites-btn" title="Show favorites">
              ⭐ Favorites
            </button>
          </div>
          
          <div class="preset-category-tabs">
            <button class="category-tab active" data-category="all">All</button>
            <button class="category-tab" data-category="bass">Bass</button>
            <button class="category-tab" data-category="lead">Lead</button>
            <button class="category-tab" data-category="pad">Pad</button>
            <button class="category-tab" data-category="keys">Keys</button>
            <button class="category-tab" data-category="fx">FX</button>
          </div>
        </div>
        
        <div class="preset-grid" id="preset-grid">
          <!-- Preset cards will be rendered here -->
        </div>
        
        <div class="modal-footer">
          <button class="btn-secondary" onclick="presetUI.hidePresetBrowser()">Close</button>
        </div>
      </div>
    `;
    
    document.body.appendChild(modal);
  }
  
  createSavePresetDialog() {
    const modal = document.createElement('div');
    modal.id = 'save-preset-modal';
    modal.className = 'modal-overlay';
    modal.style.display = 'none';
    
    modal.innerHTML = `
      <div class="modal-content save-preset-content">
        <div class="modal-header">
          <h2>💾 Save Preset</h2>
          <button class="modal-close" onclick="presetUI.hideSavePresetDialog()">✕</button>
        </div>
        
        <div class="modal-body">
          <div class="form-group">
            <label for="preset-name-input">Preset Name:</label>
            <input type="text" id="preset-name-input" placeholder="My Awesome Preset" required />
          </div>
          
          <div class="form-group">
            <label for="preset-category-select">Category:</label>
            <select id="preset-category-select">
              <option value="lead">Lead</option>
              <option value="bass">Bass</option>
              <option value="pad">Pad</option>
              <option value="keys">Keys</option>
              <option value="fx">FX</option>
            </select>
          </div>
          
          <div class="form-group">
            <label for="preset-tags-input">Tags (comma-separated):</label>
            <input type="text" id="preset-tags-input" placeholder="bright, aggressive, edm" />
          </div>
          
          <div class="form-group">
            <label for="preset-description-input">Description:</label>
            <textarea id="preset-description-input" rows="3" placeholder="Describe this preset..."></textarea>
          </div>
          
          <div class="form-group">
            <label for="preset-bpm-input">BPM (optional):</label>
            <input type="number" id="preset-bpm-input" min="1" max="999" placeholder="120" />
          </div>
          
          <div class="form-group">
            <label for="preset-key-input">Key (optional):</label>
            <input type="text" id="preset-key-input" placeholder="C minor" />
          </div>
        </div>
        
        <div class="modal-footer">
          <button class="btn-secondary" onclick="presetUI.hideSavePresetDialog()">Cancel</button>
          <button class="btn-primary" onclick="presetUI.saveNewPreset()">Save Preset</button>
        </div>
      </div>
    `;
    
    document.body.appendChild(modal);
  }
  
  createMorphUI() {
    const synthPanel = document.querySelector('.synth-panel');
    if (!synthPanel) return;
    
    const morphContainer = document.createElement('div');
    morphContainer.id = 'preset-morph-container';
    morphContainer.className = 'preset-morph-container';
    morphContainer.style.display = 'none';
    
    morphContainer.innerHTML = `
      <div class="morph-header">
        <h3>🎭 Preset Morph</h3>
        <button class="morph-close" onclick="presetUI.disableMorph()">✕</button>
      </div>
      <div class="morph-controls">
        <div class="morph-preset-slot">
          <span class="morph-label">A</span>
          <div class="morph-preset-name" id="morph-preset-a">Select A</div>
          <button class="morph-select-btn" onclick="presetUI.selectMorphPreset('A')">Select</button>
        </div>
        <div class="morph-slider-container">
          <input type="range" id="morph-slider" min="0" max="100" value="50" />
          <div class="morph-amount-display" id="morph-amount">50%</div>
        </div>
        <div class="morph-preset-slot">
          <span class="morph-label">B</span>
          <div class="morph-preset-name" id="morph-preset-b">Select B</div>
          <button class="morph-select-btn" onclick="presetUI.selectMorphPreset('B')">Select</button>
        </div>
      </div>
    `;
    
    // Insert before controls
    const controls = synthPanel.querySelector('.controls');
    if (controls) {
      controls.insertAdjacentElement('beforebegin', morphContainer);
    } else {
      synthPanel.appendChild(morphContainer);
    }
  }
  
  attachEventListeners() {
    // Search input
    document.addEventListener('input', (e) => {
      if (e.target.id === 'preset-search') {
        this.searchQuery = e.target.value;
        this.renderPresetGrid();
      }
    });
    
    // Category tabs
    document.addEventListener('click', (e) => {
      if (e.target.classList.contains('category-tab')) {
        document.querySelectorAll('.category-tab').forEach(tab => {
          tab.classList.remove('active');
        });
        e.target.classList.add('active');
        this.currentCategory = e.target.dataset.category;
        this.renderPresetGrid();
      }
    });
    
    // Favorites toggle
    document.addEventListener('click', (e) => {
      if (e.target.id === 'show-favorites-btn') {
        e.target.classList.toggle('active');
        this.renderPresetGrid();
      }
    });
    
    // Morph slider
    document.addEventListener('input', (e) => {
      if (e.target.id === 'morph-slider') {
        const amount = parseInt(e.target.value) / 100;
        this.presetManager.setMorphAmount(amount);
        document.getElementById('morph-amount').textContent = e.target.value + '%';
      }
    });
    
    // Keyboard shortcut: Ctrl+P for preset browser
    document.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'p') {
        e.preventDefault();
        this.showPresetBrowser();
      }
    });
  }
  
  showPresetBrowser() {
    const modal = document.getElementById('preset-browser-modal');
    if (modal) {
      modal.style.display = 'flex';
      this.renderPresetGrid();
      
      // Focus search input
      const searchInput = document.getElementById('preset-search');
      if (searchInput) {
        setTimeout(() => searchInput.focus(), 100);
      }
    }
  }
  
  hidePresetBrowser() {
    const modal = document.getElementById('preset-browser-modal');
    if (modal) {
      modal.style.display = 'none';
    }
  }
  
  showSavePresetDialog() {
    const modal = document.getElementById('save-preset-modal');
    if (modal) {
      modal.style.display = 'flex';
      
      // Focus name input
      const nameInput = document.getElementById('preset-name-input');
      if (nameInput) {
        setTimeout(() => nameInput.focus(), 100);
      }
    }
  }
  
  hideSavePresetDialog() {
    const modal = document.getElementById('save-preset-modal');
    if (modal) {
      modal.style.display = 'none';
      
      // Clear form
      document.getElementById('preset-name-input').value = '';
      document.getElementById('preset-tags-input').value = '';
      document.getElementById('preset-description-input').value = '';
      document.getElementById('preset-bpm-input').value = '';
      document.getElementById('preset-key-input').value = '';
    }
  }
  
  renderPresetGrid() {
    const grid = document.getElementById('preset-grid');
    if (!grid) return;
    
    // Get presets based on filters
    let presets;
    const showFavoritesOnly = document.getElementById('show-favorites-btn')?.classList.contains('active');
    
    if (showFavoritesOnly) {
      presets = this.presetManager.getFavoritePresets();
    } else if (this.searchQuery) {
      presets = this.presetManager.searchPresets(this.searchQuery);
    } else {
      presets = this.presetManager.getPresetsByCategory(this.currentCategory);
    }
    
    // Render preset cards
    grid.innerHTML = presets.map(preset => this.renderPresetCard(preset)).join('');
  }
  
  renderPresetCard(preset) {
    const categoryIcon = {
      bass: '🔊',
      lead: '🎺',
      pad: '🌊',
      keys: '🎹',
      fx: '✨'
    }[preset.category] || '🎵';
    
    const factoryBadge = preset.isFactory ? '<span class="factory-badge">Factory</span>' : '';
    const favoriteClass = preset.isFavorite ? 'favorited' : '';
    
    return `
      <div class="preset-card" data-preset-id="${preset.id}">
        <div class="preset-card-header">
          <button class="preset-favorite-btn ${favoriteClass}" 
                  onclick="presetUI.toggleFavorite('${preset.id}')"
                  title="Add to favorites">
            ${preset.isFavorite ? '⭐' : '☆'}
          </button>
          ${factoryBadge}
        </div>
        
        <div class="preset-card-icon">${categoryIcon}</div>
        
        <div class="preset-card-body">
          <h4 class="preset-name">${preset.name}</h4>
          <p class="preset-category">${preset.category}</p>
          
          ${preset.metadata.description ? `
            <p class="preset-description">${preset.metadata.description}</p>
          ` : ''}
          
          ${preset.metadata.tags.length > 0 ? `
            <div class="preset-tags">
              ${preset.metadata.tags.map(tag => `<span class="preset-tag">${tag}</span>`).join('')}
            </div>
          ` : ''}
          
          <div class="preset-metadata">
            ${preset.metadata.bpm ? `<span>♩ ${preset.metadata.bpm}</span>` : ''}
            ${preset.metadata.key ? `<span>🎵 ${preset.metadata.key}</span>` : ''}
          </div>
        </div>
        
        <div class="preset-card-actions">
          <button class="btn-preview" onclick="presetUI.previewPreset('${preset.id}')" title="Preview">
            ▶️
          </button>
          <button class="btn-load" onclick="presetUI.loadPreset('${preset.id}')" title="Load preset">
            Load
          </button>
          ${!preset.isFactory ? `
            <button class="btn-delete" onclick="presetUI.deletePreset('${preset.id}')" title="Delete">
              🗑️
            </button>
          ` : `
            <button class="btn-duplicate" onclick="presetUI.duplicatePreset('${preset.id}')" title="Duplicate">
              📋
            </button>
          `}
          <button class="btn-export" onclick="presetUI.exportPreset('${preset.id}')" title="Export">
            💾
          </button>
        </div>
      </div>
    `;
  }
  
  loadPreset(presetId) {
    try {
      const preset = this.presetManager.applyPreset(presetId);
      this.showNotification(`✅ Loaded: ${preset.name}`, 'success');
      this.hidePresetBrowser();
      
      // Mark project as having unsaved changes
      if (typeof projectManager !== 'undefined' && projectManager) {
        projectManager.hasUnsavedChanges = true;
        projectManager.updateProjectName();
      }
    } catch (error) {
      this.showNotification(`❌ ${error.message}`, 'error');
    }
  }
  
  previewPreset(presetId) {
    try {
      const preset = this.presetManager.getPresetById(presetId);
      if (!preset) return;
      
      // Save current state
      const previousPatch = this.presetManager.captureCurrentPatch();
      
      // Apply preset
      this.presetManager.applyPatch(preset.patch);
      
      // Play a preview note (C4)
      if (typeof synth !== 'undefined' && synth) {
        const frequency = 261.63; // C4
        synth.noteOn(frequency);
        
        // Stop after 1 second and restore previous state
        setTimeout(() => {
          synth.noteOff(frequency);
          setTimeout(() => {
            this.presetManager.applyPatch(previousPatch);
          }, 100);
        }, 1000);
      }
    } catch (error) {
      console.error('Preview failed:', error);
    }
  }
  
  toggleFavorite(presetId) {
    const isFavorite = this.presetManager.toggleFavorite(presetId);
    this.renderPresetGrid();
    
    const preset = this.presetManager.getPresetById(presetId);
    if (preset) {
      const message = isFavorite ? 
        `⭐ Added "${preset.name}" to favorites` : 
        `Removed "${preset.name}" from favorites`;
      this.showNotification(message, 'success');
    }
  }
  
  deletePreset(presetId) {
    const preset = this.presetManager.getPresetById(presetId);
    if (!preset) return;
    
    if (confirm(`Delete preset "${preset.name}"? This cannot be undone.`)) {
      try {
        this.presetManager.deletePreset(presetId);
        this.renderPresetGrid();
        this.showNotification(`🗑️ Deleted: ${preset.name}`, 'success');
      } catch (error) {
        this.showNotification(`❌ ${error.message}`, 'error');
      }
    }
  }
  
  duplicatePreset(presetId) {
    const preset = this.presetManager.getPresetById(presetId);
    if (!preset) return;
    
    const copy = preset.duplicate();
    this.presetManager.userPresets.push(copy);
    this.presetManager.saveUserPresets();
    this.renderPresetGrid();
    this.showNotification(`📋 Duplicated: ${copy.name}`, 'success');
  }
  
  exportPreset(presetId) {
    try {
      this.presetManager.exportPreset(presetId);
      const preset = this.presetManager.getPresetById(presetId);
      this.showNotification(`💾 Exported: ${preset.name}`, 'success');
    } catch (error) {
      this.showNotification(`❌ ${error.message}`, 'error');
    }
  }
  
  saveNewPreset() {
    const name = document.getElementById('preset-name-input').value.trim();
    const category = document.getElementById('preset-category-select').value;
    const tagsInput = document.getElementById('preset-tags-input').value;
    const description = document.getElementById('preset-description-input').value.trim();
    const bpm = document.getElementById('preset-bpm-input').value;
    const key = document.getElementById('preset-key-input').value.trim();
    
    if (!name) {
      this.showNotification('❌ Please enter a preset name', 'error');
      return;
    }
    
    // Parse tags
    const tags = tagsInput
      .split(',')
      .map(tag => tag.trim())
      .filter(tag => tag.length > 0);
    
    const metadata = {
      tags,
      description,
      bpm: bpm ? parseInt(bpm) : null,
      key: key || null
    };
    
    try {
      const preset = this.presetManager.saveCurrentAsPreset(name, category, metadata);
      this.hideSavePresetDialog();
      this.showNotification(`✅ Saved: ${preset.name}`, 'success');
      
      // Clear unsaved changes flag
      if (typeof projectManager !== 'undefined' && projectManager) {
        projectManager.hasUnsavedChanges = false;
        projectManager.updateProjectName();
      }
    } catch (error) {
      this.showNotification(`❌ ${error.message}`, 'error');
    }
  }
  
  // Morph UI methods
  enableMorph() {
    const container = document.getElementById('preset-morph-container');
    if (container) {
      container.style.display = 'block';
      this.morphEnabled = true;
    }
  }
  
  disableMorph() {
    const container = document.getElementById('preset-morph-container');
    if (container) {
      container.style.display = 'none';
      this.morphEnabled = false;
      this.presetManager.disableMorph();
    }
  }
  
  selectMorphPreset(slot) {
    // Store which slot we're selecting for
    this.morphSlotSelecting = slot;
    
    // Show preset browser in "morph select" mode
    this.showPresetBrowser();
    
    // Add visual indicator
    const modal = document.getElementById('preset-browser-modal');
    if (modal) {
      modal.setAttribute('data-morph-mode', slot);
    }
  }
  
  setMorphPreset(presetId) {
    if (!this.morphSlotSelecting) return;
    
    const preset = this.presetManager.getPresetById(presetId);
    if (!preset) return;
    
    const slot = this.morphSlotSelecting;
    const displayEl = document.getElementById(`morph-preset-${slot.toLowerCase()}`);
    
    if (displayEl) {
      displayEl.textContent = preset.name;
    }
    
    // Store preset reference
    if (slot === 'A') {
      this.morphPresetA = presetId;
    } else {
      this.morphPresetB = presetId;
    }
    
    // Enable morphing if both presets selected
    if (this.morphPresetA && this.morphPresetB) {
      this.presetManager.enableMorph(this.morphPresetA, this.morphPresetB);
    }
    
    this.morphSlotSelecting = null;
    this.hidePresetBrowser();
  }
  
  showNotification(message, type = 'info') {
    // Reuse existing notification system
    if (typeof projectUI !== 'undefined' && projectUI) {
      projectUI.showNotification(message, type);
    } else {
      // Fallback
      console.log(`[${type.toUpperCase()}] ${message}`);
    }
  }
}

// Initialize preset UI globally
let presetUI = null;

if (typeof document !== 'undefined') {
  document.addEventListener('DOMContentLoaded', () => {
    // Wait for preset manager to initialize
    setTimeout(() => {
      if (presetManager) {
        presetUI = new PresetUI(presetManager);
        console.log('Preset UI initialized');
      }
    }, 100);
  });
}
