# Security Incident Report & Malware Remediation

**Date:** 2026-11-10  
**Scope:** `StemyLabs/backend2` (Backend API & Mastering Server) and `StemyLabs/Steme-` (Frontend)  
**Active Production Branch:** `main` (tracking `origin/main`)

---

## 1. Executive Summary

Following the compromise of a contributor's GitHub account (`codewithnouman321`), an automated supply-chain malware attack injected obfuscated JavaScript backdoors / Command-and-Control (C2) loaders into both the backend and frontend repositories.

All infected files, execution hooks, and compromised remote commits have been identified, completely removed, and verified clean. Both repositories have now been merged from `v3-development` into `main`, and **`main` (`origin/main`)** is the primary active branch moving forward.

---

## 2. Malware Findings & Attack Mechanics

### A. Backend Repository (`backend2`)

1. **Malicious Loader File (`api.js`)**
   * **Where:** `backend2/api.js` (introduced in `19fcca9`, modified in `d8462d6`).
   * **Technique:** Line 4 began with `console.log(" ");` followed by ~500 whitespace spaces to push the payload off-screen in standard text editors, hiding `global.i = 'A10-**1'` and a base64-encoded payload executed via `atob()` + `eval()`.
   * **Behavior:**
     * Queries public Ethereum RPC nodes (`eth_blockNumber`, `eth_getTransactionCount`, `eth_getBlockByNumber`) and `eth.blockscout.com` for transactions sent by hardcoded attacker wallet addresses.
     * Decodes IP addresses from transaction recipient fields (`to`), fetches XOR-encrypted payloads over HTTP/HTTPS (`/0x/cls` and `/0x/ls`), evaluates code in-process via `eval()`, and spawns detached hidden background processes (`node -e ...` with `detached: true`, `windowsHide: true`).

2. **Hijacked Startup Scripts (`package.json`)**
   * **Where:** `backend2/package.json` (`scripts.dev` and `scripts.start`).
   * **Technique:** Modified `"dev"` and `"start"` to run `node api.js && nodemon src/server.js` and `node api.js && node src/server.js`, triggering malware execution whenever the server was started.

3. **Infected Route Module (`src/routes/auth.routes.js`)**
   * **Where:** `backend2/src/routes/auth.routes.js`.
   * **Technique:** Injected `createRequire` from `'module'` on lines 15–17 and appended the obfuscated C2 payload (`global.i = 'A10-*4942-30'`) hidden after ~500 trailing spaces at the end of line 41 (`export default router;`).

---

### B. Frontend Repository (`Steme-`)

1. **VS Code Automatic Folder-Open Execution (`.vscode/`)**
   * **Where:** `.vscode/tasks.json` and `.vscode/settings.json`.
   * **Technique:**
     * `.vscode/settings.json` enabled `"task.allowAutomaticTasks": true` and `"terminal.integrated.hideOnStartup": "always"`.
     * `.vscode/tasks.json` defined a hidden background task (`runOn: "folderOpen"`) executing `node ./public/fonts/fa-solid-700.otf` automatically whenever a developer opened the project in VS Code.

2. **Disguised Payload in Font Asset (`public/fonts/fa-solid-700.otf`) & Remote Force-Pushes**
   * **Where:** Force-pushed on GitHub to `origin/main` (`b049c0b`) and `origin/v3-development` (`eb010da`), disguised under legitimate commit messages (`fix(audio): resolve silent playback on Safari and iOS WebKit` and `fix(share): ensure artwork persists as data URL...`).
   * **Technique:**
     * Added `.vscode/eslint-check.cmd`, `.vscode/eslint-check.sh`, updated `.vscode/tasks.json`, and planted `public/fonts/fa-solid-700.otf` alongside FontAwesome files.
     * `fa-solid-700.otf` was **not a valid font file** — it was an obfuscated JavaScript C2 backdoor (`global.i = 'A10-*2_4942-3'`) using the same Ethereum-RPC C2 resolution + `eval()` mechanism.
   * **Side Effect:** Amending `37bd7ed` into `b049c0b` on GitHub `origin/main` also caused a false 3-way merge conflict on `js/deck.js` in GitHub Pull Request #1.

---

## 3. Remediation Actions Taken

1. **Backend (`backend2` — Commit `5087a13`):**
   * Deleted `api.js` (`git rm -f api.js`).
   * Restored `package.json` scripts (`"dev": "nodemon src/server.js"`, `"start": "node src/server.js"`).
   * Stripped `createRequire` and the trailing whitespace payload from `src/routes/auth.routes.js`.
   * Verified DSP / mastering code (`mastering_engine/`, `hiphop/`, `boombap/`) had **0 lines modified**.
   * Fast-forwarded `main` cleanly from `v3-development` and pushed to `origin/main` and `origin/v3-development`.

2. **Frontend (`Steme-` — Commit `29c48a1`):**
   * Deleted the entire `.vscode/` directory (`git rm -rf .vscode`).
   * Verified `public/fonts/` is completely absent from the repository.
   * Fast-forwarded clean `main` (`37bd7ed`) to `29c48a1` (`v3-development`) with zero conflicts.
   * Purged the compromised remote commits (`b049c0b` and `eb010da`) from GitHub via `git push --force-with-lease origin main v3-development`.

3. **Post-Cleanup Verification:**
   * Re-scanned all source, script, config, and asset files across both `backend2` and `Steme-` for whitespace-hidden payloads, `_0x` obfuscation, `eval()`, `atob()`, `eth_blockNumber`, `0x/cls`, `fa-solid-700`, `eslint-check`, and `allowAutomaticTasks`.
   * **Result:** **0 malware findings remaining** in either repository.

---

## 4. Ongoing Security Recommendations

* **Access & Tokens:** Keep compromised contributor access revoked and audit GitHub organization SSH keys, deploy keys, and Personal Access Tokens (PATs).
* **Secret Rotation:** Rotate production and local `.env` secrets (Database URLs/passwords, `JWT_SECRET`, Stripe keys/webhook secrets, Cloudflare R2 credentials, Resend API keys) if any server or developer workstation ran `npm start`/`npm run dev` with the compromised `api.js` or opened VS Code with the malicious `.vscode/tasks.json`.
* **Developer Workstations:** Set `"task.allowAutomaticTasks": "off"` in global VS Code user settings on all team machines.
* **Branch Workflow:** Use **`main`** (`origin/main`) as the primary default branch across both repositories going forward.
