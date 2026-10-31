@echo off
setlocal
title Install SpookyMall (One-Click Installer)

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : ONE-CLICK INSTALLER
echo ============================================================
echo.

:: 1. Check for Git (and auto-install via winget if missing)
set "GIT_EXE="
where git >nul 2>nul
if not errorlevel 1 set "GIT_EXE=git"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"
if defined GIT_EXE goto :git_ready

echo [1/3] Git is not installed on this PC. Installing Git automatically...
winget install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements
if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"
if not defined GIT_EXE (
    where git >nul 2>nul
    if not errorlevel 1 set "GIT_EXE=git"
)

if not defined GIT_EXE (
    echo [ERROR] Could not install Git automatically.
    echo Please install Git from https://git-scm.com/download/win and double-click this file again.
    pause
    exit /b 1
)

:git_ready
echo [1/3] Git ready: "%GIT_EXE%"

:: 2. Clone or Update SpookyMall into this folder (includes bundled Godot 4.7.2 zip!)
set "GIT_TERMINAL_PROMPT=0"
set "GCM_INTERACTIVE=never"
set "REPO_URL=https://github.com/mitosiskain/SpookyMall.git"

if exist "project.godot" goto :already_in_repo
if exist "SpookyMall\project.godot" goto :update_subfolder

echo [2/3] Downloading SpookyMall + bundled Godot 4.7.2 into "%~dp0SpookyMall"...
echo       (This is ~830 MB and may take 1-3 minutes)
"%GIT_EXE%" -c credential.helper= clone "%REPO_URL%" "SpookyMall"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to download SpookyMall. Please check your internet connection.
    pause
    exit /b 1
)
set "GAME_DIR=SpookyMall"
goto :repo_ready

:update_subfolder
echo [2/3] SpookyMall folder already exists. Updating it...
"%GIT_EXE%" -c credential.helper= -C "SpookyMall" pull --ff-only
set "GAME_DIR=SpookyMall"
goto :repo_ready

:already_in_repo
echo [2/3] Already inside SpookyMall folder. Updating it...
"%GIT_EXE%" -c credential.helper= pull --ff-only
set "GAME_DIR=."

:repo_ready
:: 3. Create "Run SpookyMall" Shortcut with Discord Pic Icon (icon.ico)
echo [3/3] Creating "Run SpookyMall" icon with your custom avatar...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$gameDir = (Resolve-Path '%GAME_DIR%').Path; " ^
  "$runBat = Join-Path $gameDir 'run.bat'; " ^
  "$iconFile = Join-Path $gameDir 'icon.ico'; " ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "foreach ($dest in @((Join-Path $desktop 'Run SpookyMall.lnk'), (Join-Path (Get-Location).Path 'Run SpookyMall.lnk'), (Join-Path $gameDir 'Run SpookyMall.lnk'))) { " ^
  "  $sc = $ws.CreateShortcut($dest); " ^
  "  $sc.TargetPath = $runBat; " ^
  "  $sc.WorkingDirectory = $gameDir; " ^
  "  $sc.Description = 'Launch SpookyMall (Megamall Anomaly)'; " ^
  "  if (Test-Path $iconFile) { $sc.IconLocation = $iconFile }; " ^
  "  $sc.Save(); " ^
  "}"

echo.
echo ============================================================
echo   INSTALLATION COMPLETE!
echo   - Use "Run SpookyMall" (on your Desktop or folder) to play
echo   - Use "update.bat" inside SpookyMall to get updates
echo ============================================================
echo.
call "%GAME_DIR%\run.bat"
