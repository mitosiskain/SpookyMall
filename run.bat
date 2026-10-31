@echo off
setlocal
title Run SpookyMall (Megamall Anomaly)

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : LAUNCHING GAME
echo ============================================================
echo.

:: 1. Unpack the bundled Godot_v4.7.2-stable_win64.exe.zip if not unpacked yet
if not exist "Godot_v4.7.2-stable_win64.exe" (
    if exist "Godot_v4.7.2-stable_win64.exe.zip" (
        echo [*] Unpacking bundled Godot 4.7.2 engine from repo...
        tar.exe -xf "Godot_v4.7.2-stable_win64.exe.zip"
    )
)

if not exist "Godot_v4.7.2-stable_win64.exe" (
    echo [ERROR] Godot_v4.7.2-stable_win64.exe was not found in this folder.
    echo Run update.bat first so it pulls Godot_v4.7.2-stable_win64.exe.zip from GitHub!
    pause
    exit /b 1
)

:: 2. If this is the first launch on a fresh clone (.godot\imported missing), import 3D assets once
if not exist ".godot\imported" (
    echo [*] First-time setup: Importing 3D mall assets into Godot cache...
    echo     (This takes ~20-45 seconds on the very first run only)
    "Godot_v4.7.2-stable_win64.exe" --headless --import --path .
)

echo [*] Starting SpookyMall...
"Godot_v4.7.2-stable_win64.exe" --path . %*
if errorlevel 1 (
    echo.
    echo [ERROR] Godot exited with an error code.
    pause
)
exit /b 0
