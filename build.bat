@echo off
echo ====================================
echo   Keday - Native C++ Build Script
echo ====================================
echo.

:: Find g++
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 goto gxx_found
if exist C:\msys64\mingw64\bin\g++.exe (
    set "PATH=%PATH%;C:\msys64\mingw64\bin"
    goto gxx_found
)
if exist C:\msys64\ucrt64\bin\g++.exe (
    set "PATH=%PATH%;C:\msys64\ucrt64\bin"
    goto gxx_found
)
echo [HATA] g++ bulunamadi!
pause & exit /b 1

:gxx_found
set GXX=g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows

echo [1/5] Build dizini olusturuluyor...
if not exist build mkdir build

echo [2/5] Kaynaklar derleniyor (resources.rc)...
windres src/resources.rc build/resources.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] resources.rc & pause & exit /b 1 )

echo [3/5] C++ dosyalari derleniyor...

%GXX% -c src/app.cpp              -o build/app.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] app.cpp & pause & exit /b 1 )

%GXX% -c src/audio.cpp            -o build/audio.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] audio.cpp & pause & exit /b 1 )

%GXX% -c src/settings.cpp         -o build/settings.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] settings.cpp & pause & exit /b 1 )

%GXX% -c src/features.cpp         -o build/features.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] features.cpp & pause & exit /b 1 )

%GXX% -c src/render.cpp           -o build/render.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] render.cpp & pause & exit /b 1 )

%GXX% -c src/neko.cpp             -o build/neko.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] neko.cpp & pause & exit /b 1 )

%GXX% -c src/tray.cpp             -o build/tray.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] tray.cpp & pause & exit /b 1 )

%GXX% -c src/settings_window.cpp  -o build/settings_window.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] settings_window.cpp & pause & exit /b 1 )

%GXX% -c src/main.cpp             -o build/main.o
if %ERRORLEVEL% NEQ 0 ( echo [HATA] main.cpp & pause & exit /b 1 )

echo [4/5] Baglaniyor (linking)...
%GXX% -static -o Keday.exe ^
    build/main.o ^
    build/app.o ^
    build/audio.o ^
    build/settings.o ^
    build/features.o ^
    build/render.o ^
    build/neko.o ^
    build/tray.o ^
    build/settings_window.o ^
    build/resources.o ^
    -lgdiplus -lgdi32 -luser32 -lshell32 -lshlwapi -ladvapi32 -lole32 -lcomctl32 -lwinmm
if %ERRORLEVEL% NEQ 0 ( echo [HATA] Baglama basarisiz! & pause & exit /b 1 )

echo.
echo [5/5] Installer paketi olusturuluyor (KedaySetup.exe)...

set "MAKENSIS_CMD="
where makensis >nul 2>&1
if %ERRORLEVEL% EQU 0 set "MAKENSIS_CMD=makensis"
if exist "C:\Program Files (x86)\NSIS\makensis.exe" set "MAKENSIS_CMD=C:\Program Files (x86)\NSIS\makensis.exe"
if "%MAKENSIS_CMD%"=="" if exist "C:\Program Files\NSIS\makensis.exe" set "MAKENSIS_CMD=C:\Program Files\NSIS\makensis.exe"

if "%MAKENSIS_CMD%"=="" (
    echo [UYARI] makensis bulunamadi - sadece Keday.exe olusturuldu.
    goto build_end
)
"%MAKENSIS_CMD%" setup.nsi
if %ERRORLEVEL% NEQ 0 ( echo [HATA] Installer basarisiz! & pause & exit /b 1 )
echo [INFO] KedaySetup.exe basariyla olusturuldu!

:build_end
echo.
echo ===============================================
echo   Keday basariyla derlendi!
echo   Dosyalar: Keday.exe  ve  KedaySetup.exe
echo ===============================================
echo.
