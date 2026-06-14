Unicode true
!include "MUI2.nsh"

Name "Keday"
OutFile "KedaySetup.exe"
InstallDir "$LOCALAPPDATA\Keday"
InstallDirRegKey HKCU "Software\Keday" "Install_Dir"

RequestExecutionLevel user

# Icon settings
!define MUI_ICON "assets\icon.ico"
!define MUI_UNICON "assets\icon.ico"

# Welcome Page
!define MUI_WELCOMEPAGE_TITLE "Topla Kendini, Kurulum Başlıyor"
!define MUI_WELCOMEPAGE_TEXT "  |\\__/,|   ($\r$\n  |o o  |__ )    Eyvallah, hoş geldin koçum.$\r$\n _.( T   )  \`   Kuruluma bastın mı? Artık geri dönüş yok.$\r$\n_ \`^--' /_.'   Keday sabırsız, hadi gaza bas!$\r$\n$\r$\nBu sihirbaz denen zımbırtı seni elinden tutacak, yoksa ortada kalırsın sonra."
!insertmacro MUI_PAGE_WELCOME

# License Page
!define MUI_LICENSEPAGE_TITLE "Yasal Masal - Okumayan Sürünsün"
!define MUI_LICENSEPAGE_TEXT_TOP "Bak knk, bu lisans işte. Kimse okumuyo biliyorum ama bi göz at yine de.$\r$\nYarın öbür gün 'Ben nerden bileydim?' deme."
!define MUI_LICENSEPAGE_TEXT_BOTTOM "Devam etmek istiyosan, bu zıkkımı kabul edeceksin. Kural böyle. İtiraz yok."
!insertmacro MUI_PAGE_LICENSE "LICENSE"

# Directory Page
!define MUI_DIRECTORYPAGE_TITLE "Keday Nereye Çöksün?"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Bize bi adres ver de bu kediyi nereye salacağımızı bilelim."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Klasör Seç (Nereye kurayım bacanak?)"
!insertmacro MUI_PAGE_DIRECTORY

# InstFiles Page
!insertmacro MUI_PAGE_INSTFILES

# Finish Page
!define MUI_FINISHPAGE_TITLE "Oldu Bu İş, Kurulum Tamam"
!define MUI_FINISHPAGE_TEXT "Helal be! Keday yüklendi gardaş.$\r$\nArtık masaüstünde sağa sola koşturacak bu minnoş.$\r$\n$\r$\nŞimdi çayı kap, arkana yaslan, kediye bak."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Keday.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Yolla gitsin Keday'ı, çalıştır!"
!insertmacro MUI_PAGE_FINISH

# Uninstaller Pages
!define MUI_UNCONFIRMPAGE_TEXT_TOP "Bu kediyi sistemden kazıyacaksın şimdi. Emin misin lan?"
!define MUI_UNCONFIRMPAGE_TEXT_LOCATION "Yok edilecek klasörü seç hele. İyice düşün, sonra ağlama."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!define MUI_UNFINISHPAGE_TITLE "Kedi Gitti, Huzur Kaldı mı?"
!define MUI_UNFINISHPAGE_TEXT "Keday silindi gardaş.$\r$\nKalbimizde bi boşluk oluştu ama ne yapalım, yolun açık olsun küçük dostum...$\r$\n$\r$\nAma bak, özlersen yine gel ha, her zaman bekleriz."
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "Turkish"

# Installer Section
Section "Install"
  SetOutPath "$INSTDIR"
  
  # Copy files
  File "Keday.exe"
  
  # Copy assets folder structure
  CreateDirectory "$INSTDIR\assets"
  SetOutPath "$INSTDIR\assets"
  File "assets\icon.ico"
  File "assets\icon.png"
  
  # Copy frame directories recursively
  File /r "assets\oneko"
  File /r "assets\red"
  File /r "assets\green"
  File /r "assets\rw"
  File /r "assets\dog"
  File /r "assets\tora"
  File /r "assets\sakura"
  File /r "assets\bsd"
  File /r "assets\tomoyo"
  File /r "assets\eevee"
  File /r "assets\bunny"
  File /r "assets\black"

  # Write registry for install dir and uninstall
  WriteRegStr HKCU "Software\Keday" "Install_Dir" "$INSTDIR"
  
  # Write Uninstall keys for Windows Add/Remove Programs
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday" "DisplayName" "Keday"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday" "DisplayIcon" "$INSTDIR\assets\icon.ico"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday" "Publisher" "Keke"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday" "DisplayVersion" "3.2.0"
  
  # Create Uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  # Create Desktop shortcut
  CreateShortcut "$DESKTOP\Keday.lnk" "$INSTDIR\Keday.exe" "" "$INSTDIR\assets\icon.ico"
SectionEnd

# Uninstaller Section
Section "Uninstall"
  # Delete files
  Delete "$INSTDIR\Keday.exe"
  Delete "$INSTDIR\assets\green.gif"
  Delete "$INSTDIR\assets\red.gif"
  Delete "$INSTDIR\assets\rw.gif"
  Delete "$INSTDIR\assets\oneko.gif"
  Delete "$INSTDIR\assets\icon.ico"
  Delete "$INSTDIR\assets\icon.png"
  Delete "$INSTDIR\uninstall.exe"
  
  RMDir /r "$INSTDIR\assets\oneko"
  RMDir /r "$INSTDIR\assets\red"
  RMDir /r "$INSTDIR\assets\green"
  RMDir /r "$INSTDIR\assets\rw"
  RMDir /r "$INSTDIR\assets\dog"
  RMDir /r "$INSTDIR\assets\tora"
  RMDir /r "$INSTDIR\assets\sakura"
  RMDir /r "$INSTDIR\assets\bsd"
  RMDir /r "$INSTDIR\assets\tomoyo"
  RMDir /r "$INSTDIR\assets\eevee"
  RMDir /r "$INSTDIR\assets\bunny"
  RMDir /r "$INSTDIR\assets\black"
  RMDir "$INSTDIR\assets"
  RMDir "$INSTDIR"
  
  # Remove Desktop shortcut
  Delete "$DESKTOP\Keday.lnk"
  
  # Remove registry keys
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Keday"
  DeleteRegKey HKCU "Software\Keday"
  
  # Remove startup registry key if exists
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "Keday"
SectionEnd
