@echo off
setlocal
title Update SpookyMall

cd /d "%~dp0"

echo ============================================================
echo   SPOOKYMALL : UPDATING FROM GITHUB
echo ============================================================
echo.

set "GIT_TERMINAL_PROMPT=0"
set "GCM_INTERACTIVE=never"

set "GIT_EXE="
where git >nul 2>nul
if not errorlevel 1 set "GIT_EXE=git"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "GIT_EXE=%LOCALAPPDATA%\Programs\Git\cmd\git.exe"

if not defined GIT_EXE (
    echo [ERROR] Git was not found on this PC.
    pause
    exit /b 1
)

"%GIT_EXE%" remote set-url origin "https://github.com/mitosiskain/SpookyMall.git" >nul 2>nul
"%GIT_EXE%" -c credential.helper= pull --ff-only

echo.
if not errorlevel 1 (
    echo [SUCCESS] SpookyMall is up to date!
) else (
    echo [WARNING] Update encountered an issue.
)
pause
