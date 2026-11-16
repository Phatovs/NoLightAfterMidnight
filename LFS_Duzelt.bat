@echo off
cd /d "%~dp0"
echo.
echo === NoLightAfterMidnight: bozuk ses dosyalarini duzeltme ===
echo.
git fetch origin
if errorlevel 1 goto hata
git checkout origin/Mekanikler -- "Content/Audio/NL/chair_sit.uasset" "Content/Audio/NL/chair_stand.uasset" "Content/Audio/NL/charger_undock.uasset" "Content/Audio/NL/drowned_leave.uasset" "Content/Audio/NL/drowned_return.uasset" "Content/Audio/NL/drowned_warning.uasset" "Content/Audio/NL/flashlight_empty.uasset" "Content/Audio/NL/flashlight_off.uasset" "Content/Audio/NL/flashlight_on.uasset" "Content/Audio/NL/fuse_trip.uasset" "Content/Audio/NL/player_death.uasset" "Content/Audio/NL/runner_retreat.uasset"
if errorlevel 1 goto hata
git lfs pull --include="Content/Audio/NL/*"
git lfs checkout
echo.
echo Kontrol (her dosya birkac KB olmali, 300 byte altinda olan hala bozuk):
dir "Content\Audio\NL\chair_sit.uasset" "Content\Audio\NL\player_death.uasset" "Content\Audio\NL\runner_retreat.uasset" | findstr uasset
echo.
findstr /M /C:"<<<<<<<" Content\Audio\NL\*.uasset && echo HALA CAKISMALI DOSYA VAR || echo Cakisma kalmadi, tamam.
echo.
echo Simdi GitHub Desktop'tan commit + push yap.
pause
exit /b 0
:hata
echo.
echo Bir hata oldu. Bu pencerenin ekran goruntusunu Claude'a at.
pause
exit /b 1
