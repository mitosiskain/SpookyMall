@echo off
setlocal EnableDelayedExpansion
title SpookyMall (Megamall Anomaly) Launcher

:: Move into the repository root folder where this .bat file resides
cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : MEGAMALL ANOMALY - WINDOWS TESTER LAUNCHER
echo ============================================================
echo.

:: 1. Auto-update from GitHub if git is installed and this is a git clone
where git >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    if exist ".git" (
        echo [1/2] Checking GitHub for latest SpookyMall updates...
        git pull --ff-only
        echo.
    )
)

:: 2. Locate Godot 4 executable on this Windows PC
set "GODOT_EXE="

:: Check PATH first
for %%G in (godot.exe godot4.exe) do (
    where %%G >nul 2>nul
    if !ERRORLEVEL! EQU 0 if not defined GODOT_EXE (
        for /f "delims=" %%P in ('where %%G') do if not defined GODOT_EXE set "GODOT_EXE=%%P"
    )
)

:: Check WinGet Links folder
if not defined GODOT_EXE (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe" (
        set "GODOT_EXE=%LOCALAPPDATA%\Microsoft\WinGet\Links\godot.exe"
    )
)

:: Search WinGet Packages folder for GodotEngine.GodotEngine
if not defined GODOT_EXE (
    for /f "delims=" %%F in ('dir /b /s "%LOCALAPPDATA%\Microsoft\WinGet\Packages\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
        if not defined GODOT_EXE set "GODOT_EXE=%%F"
    )
)

:: Search common user folders (Repo Root, Desktop, Downloads, Program Files)
if not defined GODOT_EXE (
    for %%D in ("%~dp0" "%USERPROFILE%\Desktop" "%USERPROFILE%\Downloads" "%ProgramFiles%\Godot" "C:\Godot") do (
        if exist "%%~D" (
            for /f "delims=" %%F in ('dir /b /s "%%~D\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
                if not defined GODOT_EXE set "GODOT_EXE=%%F"
            )
        )
    )
)

:: 3. If Godot 4 is still not found, offer automatic install via WinGet
if not defined GODOT_EXE (
    echo [!] Godot 4 was not found on your PC.
    echo [*] Installing Godot 4 automatically via Windows Package Manager ^(winget^)...
    winget install --id GodotEngine.GodotEngine -e --accept-source-agreements --accept-package-agreements
    echo.
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
    echo and place the Godot .exe inside this SpookyMall folder, then run this icon again.
    pause
    exit /b 1
)

echo [2/2] Launching SpookyMall using:
echo       "!GODOT_EXE!"
echo.
start "" "!GODOT_EXE!" --path "%~dp0" %*
exit /b 0
