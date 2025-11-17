import { initDB } from '../../lib/auth.js';

export default async function handler(req, res) {
  if (req.method !== 'POST') {
    return res.status(405).json({ error: 'Method not allowed' });
  }
  
  try {
    await initDB();
    return res.status(200).json({ success: true, message: 'Database initialized' });
  } catch (error) {
    console.error('DB init error:', error);
    return res.status(500).json({ error: 'Failed to initialize database' });
  }
}
