@echo off
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
  echo [ERROR] Virtual env not found. Run: python -m venv .venv
  exit /b 1
)

echo Installing/updating Python dependencies...
".venv\Scripts\python.exe" -m pip install -r requirements.txt -q
if errorlevel 1 (
  echo [ERROR] pip install failed
  exit /b 1
)

echo Starting Stemy Mastering Engine on http://127.0.0.1:5050
".venv\Scripts\python.exe" app.py
