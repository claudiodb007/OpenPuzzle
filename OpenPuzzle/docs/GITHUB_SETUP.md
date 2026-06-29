# GitHub Setup

## Create the repository

```bash
cd OpenPuzzle
git init
git add .
git commit -m "Initial OpenPuzzle foundation"
```

Create a GitHub repository named `OpenPuzzle`, then:

```bash
git branch -M main
git remote add origin git@github.com:YOUR_USERNAME/OpenPuzzle.git
git push -u origin main
```

## Recommended repository settings

- License: MIT
- Default branch: `main`
- Enable Issues
- Enable Discussions later
- Protect `main` after the first stable milestone
- Require GitHub Actions build before merging pull requests

## Suggested milestones

1. Foundation/Core
2. Execution Engine
3. Hardware Manager
4. Dashboard/Heatmap
5. Community Sync
6. Qt Desktop GUI
