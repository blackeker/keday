@echo off
echo ====================================
echo   Keday - Native C++ Build Script
echo ====================================
echo.

:: Try to find g++ in PATH
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist C:\msys64\mingw64\bin\g++.exe (
        set PATH=%PATH%;C:\msys64\mingw64\bin
        echo [INFO] C:\msys64\mingw64\bin path'e eklendi.
    ) else if exist C:\msys64\ucrt64\bin\g++.exe (
        set PATH=%PATH%;C:\msys64\ucrt64\bin
        echo [INFO] C:\msys64\ucrt64\bin path'e eklendi.
    ) else (
        echo [HATA] g++ bulunamadi!
        echo.
        echo MSYS2/MinGW kuruluysa, terminali kapatip tekrar acin
        echo veya asagidaki komutu calistirin:
        echo   set PATH=%%PATH%%;C:\msys64\mingw64\bin
        echo.
        pause
        exit /b 1
    )
)

echo [1/4] Build dizini olusturuluyor...
if not exist build mkdir build

echo [2/4] Kaynaklar derleniyor (resources.rc)...
windres src/resources.rc build/resources.o
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] Resource derleme basarisiz!
    pause
    exit /b 1
)

echo [3/4] C++ dosyalari derleniyor...
g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows -c src/main.cpp -o build/main.o
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] main.cpp derleme basarisiz!
    pause
    exit /b 1
)

g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows -c src/settings.cpp -o build/settings.o
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] settings.cpp derleme basarisiz!
    pause
    exit /b 1
)

g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows -c src/settings_window.cpp -o build/settings_window.o
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] settings_window.cpp derleme basarisiz!
    pause
    exit /b 1
)

echo [4/4] Baglaniyor (linking)...
g++ -O2 -std=c++17 -DUNICODE -D_UNICODE -mwindows -o Keday.exe build/main.o build/settings.o build/settings_window.o build/resources.o -lgdiplus -lgdi32 -luser32 -lshell32 -lshlwapi -ladvapi32 -lole32 -lcomctl32
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] Baglama basarisiz!
    pause
    exit /b 1
)

echo.
echo ====================================
echo   Keday.exe basariyla derlendi!
echo ====================================
echo.
echo Calistirmak icin: Keday.exe
echo.
pause
