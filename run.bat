@echo off
setlocal EnableDelayedExpansion
title Run SpookyMall (Megamall Anomaly)

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : LAUNCHING GAME
echo ============================================================
echo.

:: Locate Godot 4 executable on this Windows PC
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
    for %%D in ("%~dp0" "%USERPROFILE%\Desktop" "%USERPROFILE%\Downloads" "%ProgramFiles%\Godot" "C:\Godot") do (
        if exist "%%~D" (
            for /f "delims=" %%F in ('dir /b /s "%%~D\Godot_v4*.exe" 2^>nul ^| findstr /v /i "_console.exe"') do (
                if not defined GODOT_EXE set "GODOT_EXE=%%F"
            )
        )
    )
)

if not defined GODOT_EXE (
    echo [*] Godot 4 not found. Installing Godot 4 automatically via winget...
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
    echo [ERROR] Could not locate Godot 4.
    echo Please download Godot 4 from https://godotengine.org/download/windows/
    pause
    exit /b 1
)

echo Launching SpookyMall using: "!GODOT_EXE!"
start "" "!GODOT_EXE!" --path "%~dp0" %*
exit /b 0
