@echo off
setlocal EnableDelayedExpansion
title Install SpookyMall (One-Click Installer)

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : ONE-CLICK INSTALLER
echo ============================================================
echo.

:: 1. Check for Git (and auto-install via winget if missing)
set "GIT_EXE="
where git >nul 2>nul
if !ERRORLEVEL! EQU 0 (
    set "GIT_EXE=git"
) else (
    if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
    if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"
)

if not defined GIT_EXE (
    echo [1/4] Git is not installed on this PC. Installing Git automatically...
    winget install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements
    set "PATH=%PATH%;%ProgramFiles%\Git\cmd;%LOCALAPPDATA%\Programs\Git\cmd"
    if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
    if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"
    where git >nul 2>nul
    if !ERRORLEVEL! EQU 0 if not defined GIT_EXE set "GIT_EXE=git"
)

if not defined GIT_EXE (
    echo [ERROR] Could not install Git automatically.
    echo Please install Git from https://git-scm.com/download/win and double-click this file again.
    pause
    exit /b 1
)
echo [1/4] Git ready: "!GIT_EXE!"

:: 2. Check for Godot 4 (and auto-install via winget if missing)
set "GODOT_EXE="
for %%G in (godot.exe godot4.exe) do (
    where %%G >nul 2>nul
    if !ERRORLEVEL! EQU 0 if not defined GODOT_EXE (
        for /f "delims=" %%P in ('where %%G') do if not defined GODOT_EXE set "GODOT_EXE=%%P"
    )
)
if not defined GODOT_EXE (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe" set "GODOT_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe"
)
if not defined GODOT_EXE (
    for /f "delims=" %%F in ('dir /b /s "%LOCALAPPDATA%\Microsoft\WinGet\Packages\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
        if not defined GODOT_EXE set "GODOT_EXE=%%F"
    )
)
if not defined GODOT_EXE (
    echo [2/4] Godot 4 not found. Installing Godot 4 automatically via winget...
    winget install --id GodotEngine.GodotEngine -e --accept-source-agreements --accept-package-agreements
) else (
    echo [2/4] Godot 4 ready!
)

:: 3. Clone SpookyMall into this folder (100% Anonymous / No Login)
set "GIT_TERMINAL_PROMPT=0"
set "GCM_INTERACTIVE=never"
set "REPO_URL=https://github.com/mitosiskain/SpookyMall.git"

if exist "%~dp0SpookyMall\project.godot" (
    echo [3/4] SpookyMall folder already exists. Updating it...
    "!GIT_EXE!" -c credential.helper= -C "%~dp0SpookyMall" pull --ff-only
    set "GAME_DIR=%~dp0SpookyMall"
) else if exist "%~dp0project.godot" (
    echo [3/4] Already inside SpookyMall folder. Updating it...
    "!GIT_EXE!" -c credential.helper= -C "%~dp0." pull --ff-only
    set "GAME_DIR=%~dp0."
) else (
    echo [3/4] Downloading SpookyMall into "%~dp0SpookyMall"...
    echo       ^(This is ~750 MB and may take 1-3 minutes^)
    "!GIT_EXE!" -c credential.helper= clone "!REPO_URL!" "%~dp0SpookyMall"
    if !ERRORLEVEL! NEQ 0 (
        echo.
        echo [ERROR] Failed to download SpookyMall. Please check your internet connection.
        pause
        exit /b 1
    )
    set "GAME_DIR=%~dp0SpookyMall"
)

:: 4. Create "Run SpookyMall" Shortcut with Discord Pic Icon (icon.ico from the cloned repo)
echo [4/4] Creating "Run SpookyMall" icon with your custom avatar...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$gameDir = (Resolve-Path '!GAME_DIR!').Path; " ^
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
call "!GAME_DIR!\run.bat"
