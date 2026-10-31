@echo off
setlocal
title Create SpookyMall Desktop Icon

cd /d "%~dp0"

echo ============================================================
echo   CREATING SPOOKYMALL DESKTOP ICON FOR WINDOWS
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "$shortcutPath = Join-Path $desktop 'Play SpookyMall.lnk'; " ^
  "$targetPath = Join-Path '%~dp0' 'Play_SpookyMall_Windows.bat'; " ^
  "$iconPath = Join-Path '%~dp0' 'icon.ico'; " ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$sc = $ws.CreateShortcut($shortcutPath); " ^
  "$sc.TargetPath = $targetPath; " ^
  "$sc.WorkingDirectory = '%~dp0'; " ^
  "$sc.Description = 'Launch SpookyMall (Megamall Anomaly) with auto-update'; " ^
  "if (Test-Path $iconPath) { $sc.IconLocation = $iconPath }; " ^
  "$sc.Save(); " ^
  "Write-Host '[SUCCESS] Created desktop icon:' $shortcutPath -ForegroundColor Green"

echo.
echo You can now double-click "Play SpookyMall" on your Desktop anytime to update and play!
pause
