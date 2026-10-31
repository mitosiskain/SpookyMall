@echo off
setlocal EnableDelayedExpansion
title SpookyMall - One-Click Installer & Launcher

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : ONE-CLICK INSTALLER, UPDATER ^& LAUNCHER
echo ============================================================
echo.

:: ----------------------------------------------------------------------------
:: STEP 1: Unpack embedded Discord Avatar Icon (icon.ico) if not present
:: ----------------------------------------------------------------------------
if not exist "%~dp0icon.ico" (
    echo [*] Extracting custom SpookyMall icon...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$lines = Get-Content -LiteralPath '%~f0'; " ^
      "$idx = [Array]::IndexOf($lines, ':: === BEGIN_ICON_BASE64 ==='); " ^
      "if ($idx -ge 0) { " ^
      "  $b64 = ($lines[($idx+1)..($lines.Length-1)] -join ''); " ^
      "  [IO.File]::WriteAllBytes((Join-Path '%~dp0' 'icon.ico'), [Convert]::FromBase64String($b64)); " ^
      "}"
)

:: ----------------------------------------------------------------------------
:: STEP 2: Check for Git (and auto-install via winget if missing)
:: ----------------------------------------------------------------------------
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

:: ----------------------------------------------------------------------------
:: STEP 3: Clone or Update SpookyMall Repository (100% Anonymous / No Login)
:: ----------------------------------------------------------------------------
set "GIT_TERMINAL_PROMPT=0"
set "GCM_INTERACTIVE=never"
set "REPO_URL=https://github.com/mitosiskain/SpookyMall.git"

set "GAME_DIR="
if exist "%~dp0project.godot" (
    set "GAME_DIR=%~dp0."
) else if exist "%~dp0SpookyMall\project.godot" (
    set "GAME_DIR=%~dp0SpookyMall"
)

if defined GAME_DIR (
    echo [2/4] Updating existing SpookyMall installation...
    "!GIT_EXE!" -C "!GAME_DIR!" remote set-url origin "!REPO_URL!" >nul 2>nul
    "!GIT_EXE!" -c credential.helper= -C "!GAME_DIR!" pull --ff-only
) else (
    echo [2/4] Downloading SpookyMall into "%~dp0SpookyMall"...
    echo       ^(This is ~750 MB and may take 1-3 minutes on first run^)
    "!GIT_EXE!" -c credential.helper= clone "!REPO_URL!" "%~dp0SpookyMall"
    if !ERRORLEVEL! NEQ 0 (
        echo.
        echo [ERROR] Failed to download SpookyMall from GitHub. Please check your internet connection.
        pause
        exit /b 1
    )
    set "GAME_DIR=%~dp0SpookyMall"
)

:: Copy icon.ico into game folder if needed
if exist "%~dp0icon.ico" (
    if not exist "!GAME_DIR!\icon.ico" copy /y "%~dp0icon.ico" "!GAME_DIR!\icon.ico" >nul 2>nul
)

:: ----------------------------------------------------------------------------
:: STEP 4: Create Shortcut Icons (Desktop + Current Folder) with Discord Pic
:: ----------------------------------------------------------------------------
echo [3/4] Creating "Play SpookyMall" icon with custom avatar...
set "ICON_FILE=!GAME_DIR!\icon.ico"
if not exist "!ICON_FILE!" set "ICON_FILE=%~dp0icon.ico"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "foreach ($dest in @((Join-Path $desktop 'Play SpookyMall.lnk'), (Join-Path '%~dp0' 'Play SpookyMall.lnk'))) { " ^
  "  $sc = $ws.CreateShortcut($dest); " ^
  "  $sc.TargetPath = '%~f0'; " ^
  "  $sc.WorkingDirectory = '%~dp0'; " ^
  "  $sc.Description = 'Update and Play SpookyMall (Megamall Anomaly)'; " ^
  "  if (Test-Path '!ICON_FILE!') { $sc.IconLocation = '!ICON_FILE!' }; " ^
  "  $sc.Save(); " ^
  "}"

:: ----------------------------------------------------------------------------
:: STEP 5: Locate or Auto-Install Godot 4 and Launch SpookyMall!
:: ----------------------------------------------------------------------------
set "GODOT_EXE="

for %%G in (godot.exe godot4.exe) do (
    where %%G >nul 2>nul
    if !ERRORLEVEL! EQU 0 if not defined GODOT_EXE (
        for /f "delims=" %%P in ('where %%G') do if not defined GODOT_EXE set "GODOT_EXE=%%P"
    )
)

if not defined GODOT_EXE (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe" (
        set "GODOT_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe"
    )
)

if not defined GODOT_EXE (
    for /f "delims=" %%F in ('dir /b /s "%LOCALAPPDATA%\Microsoft\WinGet\Packages\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
        if not defined GODOT_EXE set "GODOT_EXE=%%F"
    )
)

if not defined GODOT_EXE (
    for %%D in ("%~dp0" "!GAME_DIR!" "%USERPROFILE%\Desktop" "%USERPROFILE%\Downloads" "%ProgramFiles%\Godot" "C:\Godot") do (
        if exist "%%~D" (
            for /f "delims=" %%F in ('dir /b /s "%%~D\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
                if not defined GODOT_EXE set "GODOT_EXE=%%F"
            )
        )
    )
)

if not defined GODOT_EXE (
    echo [4/4] Godot 4 not found. Installing Godot 4 automatically via winget...
    winget install --id GodotEngine.GodotEngine -e --accept-source-agreements --accept-package-agreements
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe" (
        set "GODOT_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe"
    ) else (
        for /f "delims=" %%F in ('dir /b /s "%LOCALAPPDATA%\Microsoft\WinGet\Packages\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
            if not defined GODOT_EXE set "GODOT_EXE=%%F"
        )
    )
)

if not defined GODOT_EXE (
    echo [ERROR] Could not locate Godot 4 after installation.
    echo Please download Godot 4 from https://godotengine.org/download/windows/
    pause
    exit /b 1
)

echo [4/4] Launching SpookyMall...
start "" "!GODOT_EXE!" --path "!GAME_DIR!" %*
exit /b 0

:: === BEGIN_ICON_BASE64 ===
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
Oyc5t6b0AAAAAElFTkSuQmCC
