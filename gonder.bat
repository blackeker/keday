@echo off
chcp 65001 >nul

set PATH=%PATH%;C:\Program Files\Git\cmd
cd /d "%~dp0"

echo ======================================================
echo   GitHub Otomatik Senkronizasyon Çalışıyor...
echo   Kapatmak için bu pencereyi kapatabilirsiniz.
echo ======================================================

:loop
:: Değişiklik olup olmadığını kontrol et
git status --porcelain | findstr /r "." >nul
if %errorlevel% neq 0 goto wait_block

echo [%time%] Değişiklik algılandı, işleniyor...

git add .
git commit -m "Otomatik güncelleme yapıldı"

echo GitHub'dan güncellemeler alınıyor (pull)...
git pull origin main --no-edit

echo GitHub'a gönderiliyor (push)...
git push origin main

echo [%time%] İşlem tamamlandı. Beklemeye geçiliyor.
echo --------------------------------------------------

:wait_block
:: 10 saniye bekle ve tekrar kontrol et
timeout /t 10 >nul
goto loop