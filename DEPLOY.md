# Quick Deployment Guide

## Deploy to Vercel in 5 Minutes

### 1. Install Vercel CLI
```bash
npm install -g vercel
```

### 2. Login to Vercel
```bash
vercel login
```

### 3. Set Up Database

**Option A: Vercel Postgres (Easiest)**
1. Go to https://vercel.com/dashboard
2. Create new project or select existing
3. Go to **Storage** → **Create Database** → **Postgres**
4. Copy the `POSTGRES_URL` (use "Pooled connection")

**Option B: Supabase (Free Forever)**
1. Go to https://supabase.com
2. Create new project
3. Go to **Settings** → **Database**
4. Copy **Connection Pooling** URL

### 4. Deploy
```bash
cd /Users/dzheng/Documents/synth
vercel
```

Follow the prompts:
- Set up and deploy? **Y**
- Which scope? (select your account)
- Link to existing project? **N**
- Project name? (press enter for "synth")
- Directory? (press enter for current)
- Override settings? **N**

### 5. Add Environment Variables
```bash
vercel env add JWT_SECRET
```
Enter a strong random secret (e.g., `openssl rand -base64 32`)

```bash
vercel env add POSTGRES_URL
```
Paste your Postgres connection string

Select: **Production**, **Preview**, and **Development**

### 6. Deploy to Production
```bash
vercel --prod
```

### 7. Initialize Database
```bash
curl -X POST https://your-project.vercel.app/api/init-db
```

Replace `your-project.vercel.app` with your actual Vercel URL (shown after deployment).

### 8. Test It!
Visit your Vercel URL and you should see the synth with login modal!

## Troubleshooting

### "Cannot find module"
```bash
npm install
vercel --prod
```

### Database connection error
- Make sure you're using the **Pooled connection** URL from Vercel/Supabase
- Check the URL doesn't have trailing spaces
- Try re-adding the env var: `vercel env rm POSTGRES_URL` then `vercel env add POSTGRES_URL`

### "Module not found: @vercel/postgres"
The serverless functions will install dependencies automatically. If you see this locally:
```bash
npm install
```

### Local testing
```bash
# Create .env file
cp .env.example .env
# Edit .env with your values
npm run dev
```

Then visit http://localhost:3000

## Next Steps

- Custom domain: In Vercel dashboard, go to Settings → Domains
- Add OAuth: Extend `/api/auth/` with Google/Apple OAuth
- Enable HTTPS-only cookies: Update auth.js to use httpOnly cookies instead of localStorage
- Add rate limiting: Use Vercel's edge middleware

## Commands Cheatsheet

```bash
# Deploy to preview
vercel

# Deploy to production
vercel --prod

# View logs
vercel logs

# View env vars
vercel env ls

# Pull env vars locally
vercel env pull

# Remove deployment
vercel rm project-name
```
