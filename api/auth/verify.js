import { getUserFromToken } from '../../lib/auth.js';

export default async function handler(req, res) {
  // Enable CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
  
  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }
  
  if (req.method !== 'GET') {
    return res.status(405).json({ error: 'Method not allowed' });
  }
  
  try {
    // Get token from Authorization header
    const authHeader = req.headers.authorization;
    
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      return res.status(401).json({ error: 'No token provided' });
    }
    
    const token = authHeader.substring(7);
    const user = await getUserFromToken(token);
    
    return res.status(200).json({
      success: true,
      user: {
        id: user.id,
        email: user.email,
        created_at: user.created_at,
        last_login: user.last_login
      }
    });
    
  } catch (error) {
    console.error('Verify error:', error);
    
    if (error.message === 'Invalid or expired token' || error.message === 'User not found') {
      return res.status(401).json({ error: 'Invalid or expired token' });
    }
    
    return res.status(500).json({ error: 'Server error' });
  }
}
