// Auth API client
class AuthAPI {
  constructor(baseURL = '') {
    this.baseURL = baseURL;
    this.token = localStorage.getItem('synth_token');
  }

  async signup(email, password) {
    const response = await fetch(`${this.baseURL}/api/auth/signup`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password })
    });

    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.error || 'Signup failed');
    }

    this.token = data.token;
    localStorage.setItem('synth_token', data.token);
    return data;
  }

  async login(email, password) {
    const response = await fetch(`${this.baseURL}/api/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password })
    });

    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.error || 'Login failed');
    }

    this.token = data.token;
    localStorage.setItem('synth_token', data.token);
    return data;
  }

  async verify() {
    if (!this.token) {
      throw new Error('No token');
    }

    const response = await fetch(`${this.baseURL}/api/auth/verify`, {
      method: 'GET',
      headers: {
        'Authorization': `Bearer ${this.token}`
      }
    });

    const data = await response.json();

    if (!response.ok) {
      this.logout();
      throw new Error(data.error || 'Verification failed');
    }

    return data;
  }

  logout() {
    this.token = null;
    localStorage.removeItem('synth_token');
  }

  isAuthenticated() {
    return !!this.token;
  }
}

// Auth UI Manager
class AuthUI {
  constructor(api) {
    this.api = api;
    this.currentUser = null;
  }

  init() {
    // Check if user is already logged in
    if (this.api.isAuthenticated()) {
      this.verifySession();
    } else {
      this.showAuthModal();
    }

    // Set up event listeners
    this.setupEventListeners();
  }

  async verifySession() {
    try {
      const result = await this.api.verify();
      this.currentUser = result.user;
      this.showAuthenticatedUI();
    } catch (error) {
      console.error('Session verification failed:', error);
      this.showAuthModal();
    }
  }

  setupEventListeners() {
    // Login form
    const loginForm = document.getElementById('login-form');
    if (loginForm) {
      loginForm.addEventListener('submit', (e) => this.handleLogin(e));
    }

    // Signup form
    const signupForm = document.getElementById('signup-form');
    if (signupForm) {
      signupForm.addEventListener('submit', (e) => this.handleSignup(e));
    }

    // Toggle forms
    const showSignup = document.getElementById('show-signup');
    const showLogin = document.getElementById('show-login');
    
    if (showSignup) {
      showSignup.addEventListener('click', (e) => {
        e.preventDefault();
        this.toggleAuthForm('signup');
      });
    }

    if (showLogin) {
      showLogin.addEventListener('click', (e) => {
        e.preventDefault();
        this.toggleAuthForm('login');
      });
    }

    // Logout
    const logoutBtn = document.getElementById('logout-btn');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', () => this.handleLogout());
    }

    // Skip login (guest mode)
    const skipBtn = document.getElementById('skip-login');
    if (skipBtn) {
      skipBtn.addEventListener('click', () => this.skipLogin());
    }
  }

  async handleLogin(e) {
    e.preventDefault();
    const form = e.target;
    const email = form.email.value;
    const password = form.password.value;
    const errorDiv = document.getElementById('login-error');

    try {
      errorDiv.textContent = '';
      const result = await this.api.login(email, password);
      this.currentUser = result.user;
      this.hideAuthModal();
      this.showAuthenticatedUI();
    } catch (error) {
      errorDiv.textContent = error.message;
    }
  }

  async handleSignup(e) {
    e.preventDefault();
    const form = e.target;
    const email = form.email.value;
    const password = form.password.value;
    const confirmPassword = form.confirmPassword.value;
    const errorDiv = document.getElementById('signup-error');

    if (password !== confirmPassword) {
      errorDiv.textContent = 'Passwords do not match';
      return;
    }

    try {
      errorDiv.textContent = '';
      const result = await this.api.signup(email, password);
      this.currentUser = result.user;
      this.hideAuthModal();
      this.showAuthenticatedUI();
    } catch (error) {
      errorDiv.textContent = error.message;
    }
  }

  handleLogout() {
    this.api.logout();
    this.currentUser = null;
    this.showAuthModal();
    this.hideAuthenticatedUI();
  }

  skipLogin() {
    this.hideAuthModal();
    // Continue in guest mode (no auth required)
  }

  toggleAuthForm(type) {
    const loginForm = document.getElementById('login-container');
    const signupForm = document.getElementById('signup-container');

    if (type === 'signup') {
      loginForm.style.display = 'none';
      signupForm.style.display = 'block';
    } else {
      loginForm.style.display = 'block';
      signupForm.style.display = 'none';
    }
  }

  showAuthModal() {
    const modal = document.getElementById('auth-modal');
    if (modal) {
      modal.style.display = 'flex';
    }
  }

  hideAuthModal() {
    const modal = document.getElementById('auth-modal');
    if (modal) {
      modal.style.display = 'none';
    }
  }

  showAuthenticatedUI() {
    const userInfo = document.getElementById('user-info');
    const userEmail = document.getElementById('user-email');
    
    if (userInfo && this.currentUser) {
      userInfo.style.display = 'block';
      if (userEmail) {
        userEmail.textContent = this.currentUser.email;
      }
    }
  }

  hideAuthenticatedUI() {
    const userInfo = document.getElementById('user-info');
    if (userInfo) {
      userInfo.style.display = 'none';
    }
  }
}

// Initialize auth when DOM is ready
const authAPI = new AuthAPI();
const authUI = new AuthUI(authAPI);

// Export for use in other modules
window.authAPI = authAPI;
window.authUI = authUI;
