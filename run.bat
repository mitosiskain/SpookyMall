@echo off
setlocal
title Run SpookyMall (Megamall Anomaly)

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : LAUNCHING GAME
echo ============================================================
echo.

:: 1. Check if portable Godot 4.7.2 is already in this folder
set "GODOT_EXE="
if exist "%~dp0Godot_v4.7.2-stable_win64.exe" (
    set "GODOT_EXE=%~dp0Godot_v4.7.2-stable_win64.exe"
    goto :have_godot
)

:: 2. Check if any Godot_v4*.exe is in this folder
for %%F in ("%~dp0Godot_v4*.exe") do (
    echo "%%~nxF" | findstr /i "_console.exe" >nul
    if errorlevel 1 if not defined GODOT_EXE set "GODOT_EXE=%%~fF"
)
if defined GODOT_EXE goto :have_godot

:: 3. Check WinGet Links folder
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe" (
    set "GODOT_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe"
    goto :have_godot
)

:: 4. Download official portable Godot 4.7.2 directly into this folder (no winget needed!)
echo [1/2] Godot 4.7.2 not found yet. Downloading official Godot 4.7.2 for Windows (~65 MB)...
curl.exe -fL --progress-bar -o "%~dp0Godot_v4.7.2-stable_win64.exe.zip" "https://github.com/godotengine/godot/releases/download/4.7.2-stable/Godot_v4.7.2-stable_win64.exe.zip"
if errorlevel 1 goto :godot_error

echo [*] Extracting Godot 4.7.2...
tar.exe -xf "%~dp0Godot_v4.7.2-stable_win64.exe.zip" -C "%~dp0"
del /q "%~dp0Godot_v4.7.2-stable_win64.exe.zip" >nul 2>nul

if exist "%~dp0Godot_v4.7.2-stable_win64.exe" (
    set "GODOT_EXE=%~dp0Godot_v4.7.2-stable_win64.exe"
    goto :have_godot
)

:godot_error
echo.
echo [ERROR] Could not download or locate Godot 4.7.2.
echo Please check your internet connection or place Godot_v4.7.2-stable_win64.exe in this folder.
pause
exit /b 1

:have_godot
echo Using Godot: "%GODOT_EXE%"

:: 5. If this is the first launch on a fresh clone (.godot\imported missing), import 3D assets once
if not exist "%~dp0.godot\imported" (
    echo.
    echo [*] First-time setup: Importing 3D mall assets into Godot cache...
    echo     (This takes ~20-45 seconds on the very first run only)
    "%GODOT_EXE%" --headless --import --path .
)

echo.
echo [*] Starting SpookyMall...
"%GODOT_EXE%" --path . %*
if errorlevel 1 (
    echo.
    echo [ERROR] Godot exited with an error code.
    pause
)
exit /b 0
