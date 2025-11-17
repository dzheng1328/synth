// Project UI Manager
class ProjectUI {
  constructor(projectManager) {
    this.projectManager = projectManager;
    this.init();
  }
  
  init() {
    this.setupEventListeners();
    this.checkForAutosave();
  }
  
  setupEventListeners() {
    // File menu buttons
    document.getElementById('btn-new-project')?.addEventListener('click', () => this.showNewProjectDialog());
    document.getElementById('btn-open-project')?.addEventListener('click', () => this.showOpenDialog());
    document.getElementById('btn-save-project')?.addEventListener('click', () => this.saveProject());
    document.getElementById('btn-save-as')?.addEventListener('click', () => this.saveProjectAs());
    document.getElementById('btn-recent-projects')?.addEventListener('click', () => this.showRecentDialog());
    document.getElementById('btn-export-bundle')?.addEventListener('click', () => this.exportBundle());
    
    // New project dialog
    document.getElementById('new-project-create')?.addEventListener('click', () => this.createNewProject());
    document.getElementById('new-project-cancel')?.addEventListener('click', () => this.hideDialog('new-project-dialog'));
    
    // Template selection
    document.querySelectorAll('.template-card').forEach(card => {
      card.addEventListener('click', () => {
        document.querySelectorAll('.template-card').forEach(c => c.classList.remove('selected'));
        card.classList.add('selected');
      });
    });
    
    // Open file dialog
    document.getElementById('project-file-input')?.addEventListener('change', (e) => this.handleFileSelect(e));
    
    // Recent projects
    document.getElementById('recent-close')?.addEventListener('click', () => this.hideDialog('recent-projects-dialog'));
    
    // Track changes
    document.addEventListener('knob-change', () => {
      if (this.projectManager.currentProject) {
        this.projectManager.hasUnsavedChanges = true;
        this.updateProjectName();
      }
    });
  }
  
  // New Project Dialog
  showNewProjectDialog() {
    this.showDialog('new-project-dialog');
    document.getElementById('project-name-input').value = 'Untitled Project';
    document.getElementById('project-name-input').select();
    
    // Generate template cards
    this.renderTemplateCards();
  }
  
  renderTemplateCards() {
    const container = document.getElementById('template-cards');
    if (!container) return;
    
    container.innerHTML = '';
    
    Object.entries(PROJECT_TEMPLATES).forEach(([key, template]) => {
      const card = document.createElement('div');
      card.className = 'template-card';
      if (key === 'empty') card.classList.add('selected');
      card.dataset.template = key;
      
      card.innerHTML = `
        <div class="template-icon">${template.icon}</div>
        <div class="template-name">${template.name}</div>
        <div class="template-desc">${template.description}</div>
      `;
      
      card.addEventListener('click', () => {
        document.querySelectorAll('.template-card').forEach(c => c.classList.remove('selected'));
        card.classList.add('selected');
      });
      
      container.appendChild(card);
    });
  }
  
  createNewProject() {
    const name = document.getElementById('project-name-input').value || 'Untitled Project';
    const selectedCard = document.querySelector('.template-card.selected');
    const template = selectedCard?.dataset.template || 'empty';
    
    // Check for unsaved changes
    if (this.projectManager.hasUnsavedChanges) {
      if (!confirm('You have unsaved changes. Create new project anyway?')) {
        return;
      }
    }
    
    const project = this.projectManager.newProject(name, template);
    this.projectManager.applyProjectState(project);
    
    this.hideDialog('new-project-dialog');
    this.updateProjectName();
    
    this.showNotification(`Created project: ${name}`);
  }
  
  // Open Project
  showOpenDialog() {
    document.getElementById('project-file-input')?.click();
  }
  
  async handleFileSelect(e) {
    const file = e.target.files[0];
    if (!file) return;
    
    try {
      const project = await this.projectManager.loadProject(file);
      this.updateProjectName();
      this.showNotification(`Opened: ${project.name}`);
    } catch (error) {
      alert('Failed to open project: ' + error.message);
    }
    
    // Reset input
    e.target.value = '';
  }
  
  // Save Project
  saveProject() {
    if (!this.projectManager.currentProject) {
      this.showNotification('No project to save', 'error');
      return;
    }
    
    const filename = this.projectManager.saveProject();
    this.updateProjectName();
    this.showNotification(`Saved: ${filename}`);
  }
  
  saveProjectAs() {
    if (!this.projectManager.currentProject) {
      this.showNotification('No project to save', 'error');
      return;
    }
    
    const newName = prompt('Project name:', this.projectManager.currentProject.name);
    if (!newName) return;
    
    this.projectManager.currentProject.name = newName;
    this.projectManager.currentProject.id = this.projectManager.currentProject.generateId();
    
    const filename = this.projectManager.saveProject(true);
    this.updateProjectName();
    this.showNotification(`Saved as: ${filename}`);
  }
  
  // Recent Projects
  showRecentDialog() {
    this.showDialog('recent-projects-dialog');
    this.renderRecentProjects();
  }
  
  renderRecentProjects() {
    const container = document.getElementById('recent-projects-list');
    if (!container) return;
    
    if (this.projectManager.recentProjects.length === 0) {
      container.innerHTML = '<div class="no-recent">No recent projects</div>';
      return;
    }
    
    container.innerHTML = '';
    
    this.projectManager.recentProjects.forEach(project => {
      const item = document.createElement('div');
      item.className = 'recent-project-item';
      
      const date = new Date(project.modified);
      const timeAgo = this.getTimeAgo(date);
      
      item.innerHTML = `
        <div class="recent-thumbnail">📁</div>
        <div class="recent-info">
          <div class="recent-name">${project.name}</div>
          <div class="recent-meta">
            <span>${timeAgo}</span>
            <span>•</span>
            <span>${project.author}</span>
          </div>
        </div>
      `;
      
      item.addEventListener('click', () => {
        // For now, just show info. Would need to reconstruct full project
        this.showNotification(`Recent projects: Open from file for now`, 'info');
      });
      
      container.appendChild(item);
    });
  }
  
  // Export Bundle
  exportBundle() {
    if (!this.projectManager.currentProject) {
      this.showNotification('No project to export', 'error');
      return;
    }
    
    const filename = this.projectManager.exportProjectBundle();
    this.showNotification(`Exported: ${filename}`);
  }
  
  // Autosave check
  checkForAutosave() {
    const autosaved = this.projectManager.loadAutosave();
    if (!autosaved) return;
    
    const autosaveDialog = document.getElementById('autosave-dialog');
    if (autosaveDialog) {
      const projectName = document.getElementById('autosave-project-name');
      if (projectName) projectName.textContent = autosaved.name;
      
      this.showDialog('autosave-dialog');
      
      document.getElementById('autosave-restore')?.addEventListener('click', () => {
        this.projectManager.applyProjectState(autosaved);
        this.updateProjectName();
        this.hideDialog('autosave-dialog');
        this.showNotification('Autosave restored');
      });
      
      document.getElementById('autosave-discard')?.addEventListener('click', () => {
        this.projectManager.clearAutosave();
        this.hideDialog('autosave-dialog');
      });
    }
  }
  
  // UI Helpers
  showDialog(id) {
    const dialog = document.getElementById(id);
    if (dialog) dialog.style.display = 'flex';
  }
  
  hideDialog(id) {
    const dialog = document.getElementById(id);
    if (dialog) dialog.style.display = 'none';
  }
  
  updateProjectName() {
    const display = document.getElementById('current-project-name');
    if (!display) return;
    
    if (this.projectManager.currentProject) {
      const unsaved = this.projectManager.hasUnsavedChanges ? ' *' : '';
      display.textContent = this.projectManager.currentProject.name + unsaved;
    } else {
      display.textContent = 'No Project';
    }
  }
  
  showNotification(message, type = 'success') {
    const notification = document.getElementById('notification');
    if (!notification) return;
    
    notification.textContent = message;
    notification.className = `notification notification-${type}`;
    notification.style.display = 'block';
    
    setTimeout(() => {
      notification.style.display = 'none';
    }, 3000);
  }
  
  getTimeAgo(date) {
    const seconds = Math.floor((new Date() - date) / 1000);
    
    if (seconds < 60) return 'Just now';
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    if (seconds < 604800) return `${Math.floor(seconds / 86400)}d ago`;
    
    return date.toLocaleDateString();
  }
}

// Initialize when project manager is ready
window.ProjectUI = ProjectUI;
