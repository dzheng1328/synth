import { createUser, generateToken } from '../../lib/auth.js';

export default async function handler(req, res) {
  // Enable CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  
  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }
  
  if (req.method !== 'POST') {
    return res.status(405).json({ error: 'Method not allowed' });
  }
  
  try {
    const { email, password } = req.body;
    
    // Validation
    if (!email || !password) {
      return res.status(400).json({ error: 'Email and password required' });
    }
    
    if (password.length < 8) {
      return res.status(400).json({ error: 'Password must be at least 8 characters' });
    }
    
    if (!email.includes('@')) {
      return res.status(400).json({ error: 'Invalid email format' });
    }
    
    // Create user
    const user = await createUser(email, password);
    const token = generateToken(user);
    
    return res.status(201).json({
      success: true,
      user: {
        id: user.id,
        email: user.email
      },
      token
    });
    
  } catch (error) {
    console.error('Signup error:', error);
    
    if (error.message === 'User already exists') {
      return res.status(409).json({ error: 'User already exists' });
    }
    
    return res.status(500).json({ error: 'Server error' });
  }
}
