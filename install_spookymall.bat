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

echo [1/4] Git is not installed on this PC. Installing Git automatically...
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
echo [1/4] Git ready: "%GIT_EXE%"

:: 2. Clone or Update SpookyMall into this folder (100% Anonymous / No Login)
set "GIT_TERMINAL_PROMPT=0"
set "GCM_INTERACTIVE=never"
set "REPO_URL=https://github.com/mitosiskain/SpookyMall.git"

if exist "%~dp0project.godot" goto :already_in_repo
if exist "%~dp0SpookyMall\project.godot" goto :update_subfolder

echo [2/4] Downloading SpookyMall into "%~dp0SpookyMall"...
echo       (This is ~750 MB and may take 1-3 minutes)
"%GIT_EXE%" -c credential.helper= clone "%REPO_URL%" "%~dp0SpookyMall"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to download SpookyMall. Please check your internet connection.
    pause
    exit /b 1
)
set "GAME_DIR=%~dp0SpookyMall"
goto :repo_ready

:update_subfolder
echo [2/4] SpookyMall folder already exists. Updating it...
"%GIT_EXE%" -c credential.helper= -C "%~dp0SpookyMall" pull --ff-only
set "GAME_DIR=%~dp0SpookyMall"
goto :repo_ready

:already_in_repo
echo [2/4] Already inside SpookyMall folder. Updating it...
"%GIT_EXE%" -c credential.helper= -C "%~dp0." pull --ff-only
set "GAME_DIR=%~dp0."

:repo_ready
:: 3. Download portable Godot 4.7.2 into the game folder if not already present
if exist "%GAME_DIR%\Godot_v4.7.2-stable_win64.exe" goto :godot_ready
echo [3/4] Downloading official Godot 4.7.2 for Windows (~65 MB)...
curl.exe -fL --progress-bar -o "%GAME_DIR%\Godot_v4.7.2-stable_win64.exe.zip" "https://github.com/godotengine/godot/releases/download/4.7.2-stable/Godot_v4.7.2-stable_win64.exe.zip"
if not errorlevel 1 (
    echo [*] Extracting Godot 4.7.2...
    tar.exe -xf "%GAME_DIR%\Godot_v4.7.2-stable_win64.exe.zip" -C "%GAME_DIR%"
    del /q "%GAME_DIR%\Godot_v4.7.2-stable_win64.exe.zip" >nul 2>nul
)

:godot_ready
:: 4. Create "Run SpookyMall" Shortcut with Discord Pic Icon (icon.ico)
echo [4/4] Creating "Run SpookyMall" icon with your custom avatar...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$gameDir = (Resolve-Path '%GAME_DIR%').Path; " ^
  "$runBat = Join-Path $gameDir 'run.bat'; " ^
  "$iconFile = Join-Path $gameDir 'icon.ico'; " ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "foreach ($dest in @((Join-Path $desktop 'Run SpookyMall.lnk'), (Join-Path '%~dp0' 'Run SpookyMall.lnk'), (Join-Path $gameDir 'Run SpookyMall.lnk'))) { " ^
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
