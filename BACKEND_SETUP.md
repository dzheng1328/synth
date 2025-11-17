# Synth - Backend Setup Guide

## Prerequisites

- Node.js 18+ installed
- Vercel account (free tier works)
- Vercel CLI installed: `npm install -g vercel`

## Local Development Setup

### 1. Install Dependencies

```bash
npm install
```

### 2. Set Up Environment Variables

Create a `.env` file in the project root:

```env
JWT_SECRET=your-super-secret-jwt-key-change-this
POSTGRES_URL=your-postgres-connection-string
```

**For local development**, you can use Vercel Postgres or any Postgres database.

### 3. Get a Postgres Database

**Option A: Vercel Postgres (Recommended)**

1. Go to [vercel.com/dashboard](https://vercel.com/dashboard)
2. Create a new project or select existing
3. Go to Storage → Create Database → Postgres
4. Copy the `POSTGRES_URL` connection string

**Option B: Local Postgres**

```bash
# macOS (using Homebrew)
brew install postgresql
brew services start postgresql

# Create database
createdb synth

# Connection string
POSTGRES_URL=postgres://localhost/synth
```

**Option C: Supabase (Free)**

1. Go to [supabase.com](https://supabase.com)
2. Create new project
3. Go to Settings → Database
4. Copy the connection string (use "Connection pooling" URL)

### 4. Run Local Dev Server

```bash
npm run dev
```

This starts Vercel dev server at `http://localhost:3000`

### 5. Initialize Database

Make a POST request to initialize tables:

```bash
curl -X POST http://localhost:3000/api/init-db
```

Or visit: `http://localhost:3000/api/init-db` in your browser (will show method not allowed, but you can use a tool like Postman)

## Deploy to Vercel

### 1. Link Your Project

```bash
vercel link
```

### 2. Set Environment Variables

```bash
vercel env add JWT_SECRET
vercel env add POSTGRES_URL
```

Enter your values when prompted. Make sure to add them for Production, Preview, and Development.

### 3. Deploy

```bash
npm run deploy
```

Or simply:

```bash
vercel --prod
```

### 4. Initialize Production Database

After deployment, initialize the database:

```bash
curl -X POST https://your-project.vercel.app/api/init-db
```

## API Endpoints

### POST /api/auth/signup
Create new user account.

**Request:**
```json
{
  "email": "user@example.com",
  "password": "password123"
}
```

**Response:**
```json
{
  "success": true,
  "user": {
    "id": 1,
    "email": "user@example.com"
  },
  "token": "jwt-token-here"
}
```

### POST /api/auth/login
Login existing user.

**Request:**
```json
{
  "email": "user@example.com",
  "password": "password123"
}
```

**Response:**
```json
{
  "success": true,
  "user": {
    "id": 1,
    "email": "user@example.com"
  },
  "token": "jwt-token-here"
}
```

### GET /api/auth/verify
Verify JWT token and get user info.

**Headers:**
```
Authorization: Bearer jwt-token-here
```

**Response:**
```json
{
  "success": true,
  "user": {
    "id": 1,
    "email": "user@example.com",
    "created_at": "2025-11-16T...",
    "last_login": "2025-11-16T..."
  }
}
```

## Frontend Integration

The auth system is already integrated into the synth UI:

- **Login/Signup Modal**: Shows on first visit
- **Guest Mode**: Users can skip login and use synth without account
- **User Info**: Shows logged-in email in top-right corner
- **Token Storage**: JWT stored in localStorage
- **Auto-Verify**: Checks token on page load

## Security Notes

1. **Change JWT_SECRET**: Use a strong random secret in production
2. **HTTPS Only**: Vercel provides HTTPS automatically
3. **Password Requirements**: Minimum 8 characters enforced
4. **Token Expiry**: Tokens expire after 7 days
5. **CORS**: Configured for all origins (tighten in production)

## Troubleshooting

### "Database connection failed"
- Check your POSTGRES_URL is correct
- Ensure database is accessible from Vercel
- For Vercel Postgres, use the "Connection pooling" URL

### "Module not found"
```bash
npm install
vercel dev
```

### "Token verification failed"
- JWT_SECRET must match between signup and verification
- Check token is being sent in Authorization header

### Local testing without database
The app will work in "guest mode" without backend. Users just click "Continue as Guest".

## Next Steps

- Add password reset functionality
- Implement OAuth (Google/Apple)
- Add email verification
- Store user presets in database
- Add project cloud sync
