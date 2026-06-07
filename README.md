# Keday

Masaüstünüzde fare imlecini takip eden sevimli, native C++ desktop pet uygulaması.

---

## 🐱 Açıklama

Keday, klasik Unix uygulaması "oneko"nun modern ve yüksek performanslı bir Windows sürümüdür. Tamamen **C++ ve Win32 API / GDI+** kullanılarak geliştirilmiştir. 

Eski Electron sürümüne kıyasla sıfır CPU gecikmesi ve son derece düşük RAM kullanımı (yaklaşık 2-3 MB) sunarak arka planda sisteminizi hiç yormadan çalışır. Kediniz masaüstünde koşar, fareyi takip eder, kenarlarda tırnaklarını biler ve sıkıldığında kıvrılıp uykuya dalar.

---

## ✨ Özellikler

- **Mükemmel Performans:** Native C++ yapısı sayesinde sıfır yük, yüksek FPS.
- **Gelişmiş Ayarlar Paneli:**
  - 🐱 **Karakter Seçimi:** Keday (klasik), Kırmızı, Yeşil ve Siyah-Beyaz kedi temaları.
  - ⚡ **Yürüme Hızı & Boyut:** Kedinizin hızını ve boyutunu (%50 - %300) istediğiniz gibi ayarlayın.
  - 🎚️ **Şeffaflık:** %10 ile %100 arasında şeffaflık ayarı.
  - 🪟 **Gölge Efekti:** Kedinin altında gerçek zamanlı yarı şeffaf gölge.
  - ⚙️ **Çalışma Ayarları:** Her zaman üstte kalma, fareyi takip etme, tam ekranda gizlenme, sistem tepsisine küçülme ve başlangıçta otomatik açılma (Autostart).
- **Modern Kurulum Sihirbazı:** Eğlenceli ve samimi Türkçe metinlerle süslenmiş kolay kurulum ekranı.

---

## 🛠️ Derleme ve Kurulum

### Geliştiriciler İçin Derleme

Projeyi yerel makinenizde derlemek için bir C++ derleyicisine (örneğin MinGW-w64 g++) ihtiyacınız vardır.

1. **Projeyi indirin:**
   ```bash
   git clone https://github.com/blackeker/Keday.git
   cd Keday
   ```

2. **Uygulamayı derleyin:**
   `build.bat` dosyasını çalıştırarak kaynak kodları g++ ile otomatik olarak derleyebilirsiniz:
   ```cmd
   build.bat
   ```
   Derleme tamamlandığında dizinde `Keday.exe` dosyası oluşturulacaktır.

### Kurulum Paketini Hazırlama (NSIS)

Eğlenceli Türkçe arayüze sahip kurulum sihirbazını (`KedaySetup.exe`) paketlemek için:
1. Bilgisayarınızda **NSIS (Nullsoft Scriptable Install System)** kurulu olmalıdır.
2. Root dizindeki `setup.nsi` dosyasına sağ tıklayıp **Compile NSIS Script** seçeneğini seçin veya terminalden şu komutu verin:
   ```cmd
   makensis setup.nsi
   ```
3. Oluşan `KedaySetup.exe` dosyasını çalıştırarak kolayca kurulum yapabilirsiniz.

---

## 🚀 Kullanım

- Uygulama başladığında kedi fare imlecinizi kovalamaya başlar.
- Sistem tepsisindeki (Tray) kedi simgesine **sağ tıklayarak** Ayarlar panelini açabilir, kediyi gizleyebilir veya uygulamadan çıkış yapabilirsiniz.
- Sistem tepsisindeki kedi simgesine **çift tıklayarak** doğrudan Ayarlar ekranına ulaşabilirsiniz.

---

## 🔗 Destek ve İletişim

- **Discord Sunucusu:** [Discord](https://discord.gg/hentaitr)
- **Instagram:** [@blackekerr](https://instagram.com/blackekerr)
- **GitHub:** [blackeker/Keday](https://github.com/blackeker/Keday)
