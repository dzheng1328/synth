import { sql } from '@vercel/postgres';
import bcrypt from 'bcryptjs';
import jwt from 'jsonwebtoken';

const JWT_SECRET = process.env.JWT_SECRET || 'dev-secret-change-in-production';
const JWT_EXPIRES_IN = '7d';

// Initialize database tables
export async function initDB() {
  await sql`
    CREATE TABLE IF NOT EXISTS users (
      id SERIAL PRIMARY KEY,
      email VARCHAR(255) UNIQUE NOT NULL,
      password_hash VARCHAR(255) NOT NULL,
      created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
      last_login TIMESTAMP
    )
  `;
}

// User signup
export async function createUser(email, password) {
  const existingUser = await sql`
    SELECT id FROM users WHERE email = ${email}
  `;
  
  if (existingUser.rows.length > 0) {
    throw new Error('User already exists');
  }
  
  const passwordHash = await bcrypt.hash(password, 10);
  
  const result = await sql`
    INSERT INTO users (email, password_hash)
    VALUES (${email}, ${passwordHash})
    RETURNING id, email, created_at
  `;
  
  return result.rows[0];
}

// User login
export async function loginUser(email, password) {
  const result = await sql`
    SELECT id, email, password_hash FROM users WHERE email = ${email}
  `;
  
  if (result.rows.length === 0) {
    throw new Error('Invalid credentials');
  }
  
  const user = result.rows[0];
  const isValid = await bcrypt.compare(password, user.password_hash);
  
  if (!isValid) {
    throw new Error('Invalid credentials');
  }
  
  // Update last login
  await sql`
    UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = ${user.id}
  `;
  
  return {
    id: user.id,
    email: user.email
  };
}

// Generate JWT token
export function generateToken(user) {
  return jwt.sign(
    { id: user.id, email: user.email },
    JWT_SECRET,
    { expiresIn: JWT_EXPIRES_IN }
  );
}

// Verify JWT token
export function verifyToken(token) {
  try {
    return jwt.verify(token, JWT_SECRET);
  } catch (error) {
    throw new Error('Invalid or expired token');
  }
}

// Get user from token
export async function getUserFromToken(token) {
  const decoded = verifyToken(token);
  
  const result = await sql`
    SELECT id, email, created_at, last_login FROM users WHERE id = ${decoded.id}
  `;
  
  if (result.rows.length === 0) {
    throw new Error('User not found');
  }
  
  return result.rows[0];
}
