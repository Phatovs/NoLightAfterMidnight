@echo off
cd /d "%~dp0"
echo.
echo === NoLightAfterMidnight: C++ derleme ===
echo Unreal Editor KAPALI olmali!
echo.
set "UE="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| findstr InstalledDirectory') do set "UE=%%B"
if not defined UE if exist "C:\Program Files\Epic Games\UE_5.8\Engine" set "UE=C:\Program Files\Epic Games\UE_5.8"
if not defined UE (
  echo Unreal 5.8 klasoru bulunamadi.
  set /p UE=Unreal 5.8 klasorunu yaz ^(ornek: D:\Epic Games\UE_5.8^): 
)
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" (
  echo HATA: "%UE%" icinde Unreal bulunamadi.
  goto hata
)
echo Unreal: %UE%
echo.
echo Derleniyor, birkac dakika surebilir...
call "%UE%\Engine\Build\BatchFiles\Build.bat" NoLightAfterMidnightEditor Win64 Development -Project="%CD%\NoLightAfterMidnight.uproject" -WaitMutex -NoHotReload
if errorlevel 1 goto hata
echo.
echo ===== DERLEME BASARILI =====
echo Simdi NoLightAfterMidnight.uproject'e cift tiklayip Unreal'i acabilirsin.
pause
exit /b 0
:hata
echo.
echo ===== DERLEME HATA VERDI =====
echo Yukaridaki kirmizi/"error" yazan satirlarin ekran goruntusunu Claude'a at.
pause
exit /b 1
