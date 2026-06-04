# Stemy Mastering Engine — VPS Deployment Guide

**Python 3.12 · Flask · Gunicorn · Nginx · DuckDNS**

---

## 1. SSH into VPS

```bash
ssh root@YOUR_VPS_IP
```

Update system:

```bash
apt update && apt upgrade -y
```

---

## 2. Install Dependencies

```bash
apt install -y python3 python3-pip python3-venv nginx git curl ufw
```

Optional security:

```bash
apt install -y fail2ban
```

---

## 3. Firewall

```bash
ufw allow OpenSSH
ufw allow 'Nginx Full'
ufw enable
```

---

## 4. Clone Repository

```bash
mkdir -p /var/www/python-backend
cd /var/www/python-backend
git clone https://github.com/StemyLabs/backend2.git .
```

---

## 5. Python 3.12 Setup

Check Python version:

```bash
python3 --version
```

If Python 3.12 is **not** installed, install it:

```bash
apt install -y software-properties-common
add-apt-repository ppa:deadsnakes/ppa -y
apt update
apt install -y python3.12 python3.12-venv python3.12-dev
```

Install **ffmpeg** (required for ≤90s turbo mastering on long tracks):

```bash
apt install -y ffmpeg
ffmpeg -version
```

---

## 6. Virtual Environment

```bash
cd /var/www/python-backend/mastering_engine
python3.12 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

Test run:

```bash
python app.py
```

Visit `http://YOUR_VPS_IP:5050/health`. Press **Ctrl+C** to stop.

---

## 7. Install Gunicorn

```bash
pip install gunicorn
```

Test Gunicorn:

```bash
gunicorn app:app --bind 127.0.0.1:8000 --timeout 1200
```

Visit `http://YOUR_VPS_IP:8000/health`. Press **Ctrl+C** to stop.

---

## 8. systemd Service

```bash
nano /etc/systemd/system/stemy-backend.service
```

Paste:

```ini
[Unit]
Description=Stemy Mastering Engine
After=network.target

[Service]
User=root
WorkingDirectory=/var/www/python-backend/mastering_engine
Environment="PATH=/var/www/python-backend/mastering_engine/venv/bin"
ExecStart=/var/www/python-backend/mastering_engine/venv/bin/gunicorn app:app --bind 127.0.0.1:8000 --timeout 1200 --workers 2
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
systemctl daemon-reload
systemctl enable stemy-backend
systemctl start stemy-backend
```

Check status:

```bash
systemctl status stemy-backend
```

---

## 9. Nginx Reverse Proxy

```bash
nano /etc/nginx/sites-available/stemy-backend
```

Paste:

```nginx
server {
    listen 80;
    server_name stemy-bg.duckdns.org;

    client_max_body_size 100M;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_read_timeout 1200s;
        proxy_send_timeout 1200s;
    }
}
```

Enable site:

```bash
ln -s /etc/nginx/sites-available/stemy-backend /etc/nginx/sites-enabled
nginx -t
systemctl restart nginx
```

---

## 10. DuckDNS Setup

Your A record should point to your VPS IP:

| Type | Name | Value       |
| ---- | ---- | ----------- |
| A    |      | YOUR_VPS_IP |

Visit: [http://stemy-bg.duckdns.org/health](http://stemy-bg.duckdns.org/health)

---

## 11. SSL (HTTPS) with Let's Encrypt

```bash
apt install -y certbot python3-certbot-nginx
certbot --nginx -d stemy-bg.duckdns.org
```

Test auto-renewal:

```bash
certbot renew --dry-run
```

Your site is now live at: **https://stemy-bg.duckdns.org**

---

## 12. Logs & Debugging

### Backend logs

```bash
journalctl -u stemy-backend -f
```

### Nginx logs

```bash
tail -f /var/log/nginx/error.log
tail -f /var/log/nginx/access.log
```

### Service status

```bash
systemctl status stemy-backend
```

---

## 13. Deployment Updates

```bash
cd /var/www/python-backend
git pull

cd mastering_engine
source venv/bin/activate
pip install -r requirements.txt

systemctl restart stemy-backend
```

---

## 14. Architecture

```
Frontend (Vercel / anywhere)
        ↓  HTTPS
Nginx (VPS — port 443/80)
        ↓  reverse proxy
Gunicorn (127.0.0.1:8000)
        ↓
Flask Mastering Engine (mastering_engine/app.py)
```

---

## 15. Turbo mode (≤90s target, up to 100 MB / ~2 hr)

When Node and Python run on the **same VPS**, use a shared temp folder and the local CLI (no HTTP re-upload of the source file).

```bash
mkdir -p /var/lib/stemy/masters
chmod 755 /var/lib/stemy/masters
```

**Node API** `.env`:

```env
STEMY_TEMP_DIR=/var/lib/stemy/masters
PYTHON_ENGINE_URL=http://127.0.0.1:8000
PYTHON_USE_LOCAL_CLI=true
# Use http (default) — calls warm Gunicorn POST /master/local (~2s). Avoid cli (~10s cold start per job).
PYTHON_LOCAL_MODE=http
```

**Python** (systemd `Environment=` or `/etc/environment`):

```env
STEMY_TEMP_DIR=/var/lib/stemy/masters
STEMY_TURBO=1
STEMY_TURBO_WORKERS=6
STEMY_TARGET_SEC=90
STEMY_OUTPUT_EXT=.flac
STEMY_FFMPEG_TIMEOUT_SEC=85
```

Turbo uses **ffmpeg** (`-threads 0`) for a single-pass genre chain + loudnorm. Output is **FLAC** (smaller/faster than 24-bit WAV). Set `STEMY_TURBO=0` only if you need the legacy two-pass WAV pipeline.

After deploy, logs should show `Turbo ffmpeg done in …s` and total under ~90s for typical 1-hour MP3s on a 6-core VPS.

---

## Notes

- **Python 3.12** is required (`python3.12` binary, virtual env, gunicorn).
- The app is in `/var/www/python-backend/mastering_engine/`.
- Gunicorn binds to `127.0.0.1:8000` (not public).
- Upload limit: **100 MB** (configurable via `STEMY_MAX_UPLOAD_BYTES`).
- Gunicorn timeout: **1200s** (fallback; turbo jobs should finish in &lt;90s).
- **UFW** is enabled — only SSH and Nginx ports are open.
- SSL is handled by **certbot + Let's Encrypt** for DuckDNS.
