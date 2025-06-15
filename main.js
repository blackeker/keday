const { app, BrowserWindow, screen, Tray, Menu, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const fetch = require('node-fetch');
const Store = require('electron-store');
const { autoUpdater } = require('electron-updater');
const extract = require('extract-zip');
const fse = require('fs-extra');

const store = new Store();

// Önbellek temizleme
app.commandLine.appendSwitch('disable-gpu-cache');
app.commandLine.appendSwitch('disable-software-rasterizer');
app.commandLine.appendSwitch('disable-gpu-compositing');

// Uygulama başlamadan önce önbelleği temizle
app.on('ready', () => {
    const userDataPath = app.getPath('userData');
    console.log(userDataPath);
    console.log(app.getPath('userData'));
    console.log(app.getPath('exe'));
    console.log(app.getPath('appData'));
    console.log(app.getPath('home'));
    console.log(app.getPath('temp'));
    console.log(app.getPath('userData'));
    console.log(app.getPath('desktop'));
    console.log(app.getPath('documents'));
    const cachePath = path.join(userDataPath, 'Cache');
    if (fs.existsSync(cachePath)) {
        try {
            fs.rmSync(cachePath, { recursive: true, force: true });
        } catch (error) {
            console.error('Önbellek temizlenirken hata oluştu:', error);
        }
    }
});
    
    // Varsayılan ayarlar
    const defaultSettings = {
        speed: 10,
        size: 100,
        alwaysOnTop: true,
        minimizeToTray: true,
        autoLaunch: true,
    };

// Ayarları yükle fonksiyonu
    function loadSettings() {
        const settingsPath = path.join(app.getPath('userData'), 'settings.json');
        try {
            if (fs.existsSync(settingsPath)) {
                const settings = JSON.parse(fs.readFileSync(settingsPath, 'utf8'));
                return { ...defaultSettings, ...settings };
            }
        } catch (error) {
            console.error('Ayarlar yüklenirken hata:', error);
        }
        return defaultSettings;
    }

// Ayarları kaydet fonksiyonu
    function saveSettings(settings) {
        const settingsPath = path.join(app.getPath('userData'), 'settings.json');
        try {
            fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
        } catch (error) {
            console.error('Ayarlar kaydedilirken hata:', error);
        }
    }

let mainWindow;
let tray;
let isVisible = true;
    let settings = loadSettings();
let settingsWindow = null;

    const icoPath = path.join(process.resourcesPath, 'assets', 'icon.ico');

    function toggleCat() {
        if (mainWindow) {
            isVisible = !isVisible;
            if (isVisible) {
                mainWindow.show();
            } else {
                mainWindow.hide();
            }
        }
    }

    function createTray() {
    tray = new Tray(path.join(process.resourcesPath, 'assets', 'icon.png'));
        const contextMenu = Menu.buildFromTemplate([
            { label: 'Ayarlar', click: () => openSettingsWindow() },
            { label: 'Geliştirici Araçlarını Aç (Ayarlar)', click: () => {
                console.log('Developer Tools menu item clicked. Current settingsWindow state:', settingsWindow);
                    settingsWindow.webContents.openDevTools();
                    console.log('Attempting to open DevTools via menu item.');
            } },
            { type: 'separator' },
            { label: 'Çıkış', click: () => app.quit() }
        ]);
        tray.setToolTip('Oneko Kedi');
        tray.setContextMenu(contextMenu);
        tray.on('click', (event, bounds) => {
            toggleCat();
        });
    }

    function openSettingsWindow() {
        console.log('Attempting to open settings window. Current settingsWindow state:', settingsWindow);
        if (settingsWindow) {
            settingsWindow.focus();
            console.log('Settings window already open, focusing.');
            return;
        }
        settingsWindow = new BrowserWindow({
            width: 450,
            height: 500,
            resizable: false,
            minimizable: false,
            maximizable: false,
            alwaysOnTop: true,
            frame: true,
            autoHideMenuBar: true,
            webPreferences: {
                nodeIntegration: true,
                contextIsolation: false,
                devTools: true
            }
        });
        settingsWindow.setMenu(null);
        settingsWindow.loadFile('settings.html');
        settingsWindow.webContents.on('did-finish-load', () => {
            // Ayarlar penceresi açıldığında mevcut değerleri gönder
            settingsWindow.webContents.send('current-speed', settings.speed);
            settingsWindow.webContents.send('current-size', settings.size);
        });
        settingsWindow.on('closed', () => {
            console.log('Settings window closed. Setting settingsWindow to null.');
            settingsWindow = null;
        });
    }

// Uygulamanın tek bir örneğinin çalışmasını sağla
const gotTheLock = app.requestSingleInstanceLock();

if (!gotTheLock) {
    app.quit(); // Zaten bir örnek çalışıyorsa, bu ikinci örneği kapat
        } else {
    // İkinci bir örnek başlatılmaya çalışıldığında bu olay tetiklenir
    app.on('second-instance', (event, commandLine, workingDirectory) => {
        // Mevcut pencereyi ön plana getir
        if (mainWindow) {
            if (mainWindow.isMinimized()) mainWindow.restore();
            mainWindow.focus();
        }
    });
    }

    app.whenReady().then(async () => {
        // Gerekli modülleri indir
        await downloadRequiredModules();
        
        // Pencereyi oluştur
        createWindow();

        // Ayarları uygula
        if (mainWindow) {
            mainWindow.setAlwaysOnTop(settings.alwaysOnTop, 'floating');
            mainWindow.setHasShadow(settings.showShadow);
            
            // Ayarları renderer sürecine gönder
            mainWindow.webContents.on('did-finish-load', () => {
                mainWindow.webContents.send('set-speed', settings.speed);
                mainWindow.webContents.send('set-size', settings.size);
                mainWindow.webContents.send('set-always-on-top', settings.alwaysOnTop);
                mainWindow.webContents.send('set-shadow', settings.showShadow);
                mainWindow.webContents.send('set-shadow-opacity', settings.shadowOpacity);
                mainWindow.webContents.send('set-shadow-size', settings.shadowSize);
            });
        }

        app.on('activate', () => {
            if (BrowserWindow.getAllWindows().length === 0) {
                createWindow();
            }
        });

        // Otomatik güncelleme kontrolü
        autoUpdater.checkForUpdatesAndNotify();
    });

    app.on('window-all-closed', () => {
        if (process.platform !== 'darwin') {
            app.quit();
        }
    });

    // IPC Handlers
    ipcMain.on('set-speed', (event, speed) => {
        console.log('Received set-speed IPC:', speed);
        settings.speed = speed;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-speed', speed);
        });
    });

    ipcMain.on('set-size', (event, size) => {
        console.log('Received set-size IPC:', size);
        settings.size = size;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-size', size);
        });
    });

    ipcMain.on('set-alwaysOnTop', (event, enabled) => {
        console.log('Received set-alwaysOnTop IPC:', enabled);
        settings.alwaysOnTop = enabled;
        saveSettings(settings);
        if (mainWindow) {
            mainWindow.setAlwaysOnTop(enabled, 'floating');
        }
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('alwaysOnTop-status', enabled);
        });
    });

    ipcMain.on('set-shadow', (event, enabled) => {
        console.log('Received set-shadow IPC:', enabled);
        settings.showShadow = enabled;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-shadow', enabled);
        });
    });

    ipcMain.on('set-shadow-opacity', (event, opacity) => {
        console.log('Received set-shadow-opacity IPC:', opacity);
        settings.shadowOpacity = opacity;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-shadow-opacity', opacity);
        });
    });

    ipcMain.on('set-shadow-size', (event, size) => {
        console.log('Received set-shadow-size IPC:', size);
        settings.shadowSize = size;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-shadow-size', size);
        });
    });

    ipcMain.on('get-current-speed', (event) => {
        console.log('Sending current speed:', settings.speed);
        event.reply('current-speed', settings.speed);
    });

    ipcMain.on('get-current-size', (event) => {
        console.log('Sending current size:', settings.size);
        event.reply('current-size', settings.size);
    });

    ipcMain.on('get-alwaysOnTop', (event) => {
        console.log('Received get-alwaysOnTop IPC');
        event.reply('alwaysOnTop-status', settings.alwaysOnTop);
    });

    ipcMain.on('get-shadow-status', (event) => {
        console.log('Received get-shadow-status IPC');
        event.reply('shadow-status', settings.showShadow);
    });

    ipcMain.on('set-autoLaunch', (event, enabled) => {
        console.log('Received set-autoLaunch IPC:', enabled);
        settings.autoLaunch = enabled;
        saveSettings(settings);
        app.setLoginItemSettings({
            openAtLogin: enabled,
            path: app.getPath('exe'),
            args: [] // Boş argümanlar dizisi ekle
        });
    });

    ipcMain.on('get-autoLaunch', async (event) => {
        console.log('Received get-autoLaunch IPC');
        try {
            // Electron'un kendi setLoginItemSettings'i ile durumu kontrol et
            const loginItemSettings = app.getLoginItemSettings();
            event.sender.send('autoLaunch-status', loginItemSettings.openAtLogin);
        } catch (error) {
            console.error('Otomatik başlatma durumu alınırken hata:', error);
            event.sender.send('autoLaunch-status', false);
        }
    });

    ipcMain.on('set-character', (event, character) => {
        console.log('Received set-character IPC:', character);
        settings.character = character;
        saveSettings(settings);
        if (mainWindow) {
            mainWindow.loadFile(character === 'neko' ? 'index.html' : 'anime-girl.html');
        }
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('character-changed', character);
        });
    });

    ipcMain.on('get-current-character', (event) => {
        console.log('Received get-current-character IPC');
        event.reply('current-character', settings.character);
    });

    ipcMain.on('set-idle-time', (event, time) => {
        console.log('Received set-idle-time IPC:', time);
        settings.idleTime = time;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-idle-time', time);
        });
    });

    ipcMain.on('set-opacity', (event, opacity) => {
        console.log('Received set-opacity IPC:', opacity);
        settings.opacity = opacity;
        saveSettings(settings);
        if (mainWindow) {
            mainWindow.setOpacity(opacity / 100);
        }
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-opacity', opacity);
        });
    });

    ipcMain.on('set-follow-mouse', (event, enabled) => {
        console.log('Received set-follow-mouse IPC:', enabled);
        settings.followMouse = enabled;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('follow-mouse-status', enabled);
        });
    });

    ipcMain.on('set-sound-effects', (event, enabled) => {
        console.log('Received set-sound-effects IPC:', enabled);
        settings.soundEffects = enabled;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('sound-effects-status', enabled);
        });
    });

    ipcMain.on('set-emotions', (event, enabled) => {
        console.log('Received set-emotions IPC:', enabled);
        settings.emotions = enabled;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('emotions-status', enabled);
        });
    });

    ipcMain.on('set-trail', (event, enabled) => {
        console.log('Received set-trail IPC:', enabled);
        settings.showTrail = enabled;
        saveSettings(settings);
        mainWindow.webContents.send('set-trail', enabled);
    });

    ipcMain.on('set-minimizeToTray', (event, enabled) => {
        console.log('Received set-minimizeToTray IPC:', enabled);
        settings.minimizeToTray = enabled;
        saveSettings(settings);
    });

    // Get handlers for new settings
    ipcMain.on('get-idle-time', (event) => {
        console.log('Received get-idle-time IPC');
        event.reply('idle-time-status', settings.idleTime);
    });

    ipcMain.on('get-opacity', (event) => {
        console.log('Received get-opacity IPC');
        event.reply('opacity-status', settings.opacity);
    });

    ipcMain.on('get-follow-mouse', (event) => {
        console.log('Received get-follow-mouse IPC');
        event.reply('follow-mouse-status', settings.followMouse);
    });

    ipcMain.on('get-sound-effects', (event) => {
        console.log('Received get-sound-effects IPC');
        event.reply('sound-effects-status', settings.soundEffects);
    });

    ipcMain.on('get-emotions', (event) => {
        console.log('Received get-emotions IPC');
        event.reply('emotions-status', settings.emotions);
    });

    ipcMain.on('get-trail-status', (event) => {
        console.log('Received get-trail-status IPC');
        event.reply('trail-status', settings.showTrail);
    });

    ipcMain.on('get-minimizeToTray', (event) => {
        console.log('Received get-minimizeToTray IPC');
        event.reply('minimizeToTray-status', settings.minimizeToTray);
    });

    ipcMain.on('open-settings', () => {
        console.log('Received open-settings IPC');
        if (settingsWindow) {
            settingsWindow.focus();
            return;
        }
        settingsWindow = new BrowserWindow({
            width: 450,
            height: 500,
            resizable: false,
            minimizable: false,
            maximizable: false,
            alwaysOnTop: true,
            frame: true,
            autoHideMenuBar: true,
            webPreferences: {
                nodeIntegration: true,
                contextIsolation: false,
                devTools: true
            }
        });
        settingsWindow.setMenu(null);
        settingsWindow.loadFile('settings.html');
        settingsWindow.webContents.on('did-finish-load', () => {
            settingsWindow.webContents.openDevTools();
        });
        settingsWindow.on('closed', () => { settingsWindow = null; });
    });

    // Ayarları uygula
    function applySettings(settings) {
        if (settings.speed) {
            mainWindow.webContents.send('set-speed', settings.speed);
        }
        if (settings.size) {
            mainWindow.webContents.send('set-size', settings.size);
        }
        if (settings.alwaysOnTop !== undefined) {
            mainWindow.setAlwaysOnTop(settings.alwaysOnTop);
            mainWindow.webContents.send('set-alwaysOnTop', settings.alwaysOnTop);
        }
        if (settings.minimizeToTray !== undefined) {
        }
        if (settings.autoLaunch !== undefined) {

        }
    }

    // Mini oyun başlatma
    ipcMain.on('start-mini-game', () => {
        if (gameWindow) {
            gameWindow.focus();
            return;
        }

        gameWindow = new BrowserWindow({
            width: 800,
            height: 600,
            webPreferences: {
                nodeIntegration: true,
                contextIsolation: false
            },
            parent: mainWindow,
            modal: true
        });

        gameWindow.loadFile('game.html');
        gameWindow.on('closed', () => {
            gameWindow = null;
        });
    });

    // Başarı rozetlerini göster
    ipcMain.on('show-badges', () => {
        const badges = [
            {
                id: 'first_steps',
                name: 'İlk Adımlar',
                description: 'Kediyi ilk kez hareket ettirdiniz',
                icon: 'assets/badges/first_steps.png',
                unlocked: true
            },
            {
                id: 'playful_cat',
                name: 'Oyunbaz Kedi',
                description: 'Kediyi 10 kez hareket ettirdiniz',
                icon: 'assets/badges/playful_cat.png',
                unlocked: false
            },
            {
                id: 'loyal_companion',
                name: 'Sadık Arkadaş',
                description: 'Uygulamayı 7 gün boyunca kullandınız',
                icon: 'assets/badges/loyal_companion.png',
                unlocked: false
            }
        ];
        settingsWindow.webContents.send('show-badges-modal', badges);
    });

    // Günlük görevleri göster
    ipcMain.on('show-tasks', () => {
        const tasks = [
            {
                id: 'move_cat',
                name: 'Kediyi Hareket Ettir',
                description: 'Kediyi 5 kez hareket ettirin',
                progress: 0,
                completed: false
            },
            {
                id: 'daily_visit',
                name: 'Günlük Ziyaret',
                description: 'Uygulamayı 3 kez açın',
                progress: 0,
                completed: false
            }
        ];
        settingsWindow.webContents.send('show-tasks-modal', tasks);
    });

    // Görev güncelleme
    ipcMain.on('update-task', (event, taskId) => {
        // Görev güncelleme mantığı
        const task = {
            id: taskId,
            progress: 100,
            completed: true
        };
        settingsWindow.webContents.send('task-updated', task);
    });

    // Başarı bildirimi
    ipcMain.on('unlock-achievement', (event, achievementId) => {
        const achievement = {
            id: achievementId,
            name: 'Yeni Başarı Kazanıldı!',
            icon: 'assets/badges/achievement.png'
        };
        settingsWindow.webContents.send('achievement-unlocked', achievement);
    });

    // Başarıları kontrol et
    function checkAchievements() {
        const tasks = settings.tasks || dailyTasks;
        const badges = settings.badges || badges;

        // İlk görev başarısı
        if (tasks.some(task => task.completed) && !badges.find(b => b.id === 'first_steps')?.unlocked) {
            unlockBadge('first_steps');
        }

        // Hız başarısı
        if (settings.speed >= 50 && !badges.find(b => b.id === 'speed_demon')?.unlocked) {
            unlockBadge('speed_demon');
        }

        // Oyun başarısı
        if (settings.gameHighScore >= 1000 && !badges.find(b => b.id === 'game_master')?.unlocked) {
            unlockBadge('game_master');
        }
    }

    // Rozet açma
    function unlockBadge(badgeId) {
        const badges = settings.badges || badges;
        const badge = badges.find(b => b.id === badgeId);
        if (badge && !badge.unlocked) {
            badge.unlocked = true;
            saveSettings(settings);
            
            // Tüm pencerelere başarı bildirimini gönder
            BrowserWindow.getAllWindows().forEach(win => {
                win.webContents.send('achievement-unlocked', badge);
            });
        }
    }

    // Görevleri sıfırla (her gün)
    function resetDailyTasks() {
        const tasks = settings.tasks || dailyTasks;
        tasks.forEach(task => {
            task.progress = 0;
            task.completed = false;
        });
        saveSettings(settings);
    }

    // Her gün gece yarısı görevleri sıfırla
    setInterval(() => {
        const now = new Date();
        if (now.getHours() === 0 && now.getMinutes() === 0) {
            resetDailyTasks();
        }
    }, 60000); // Her dakika kontrol et

    // Oyun skoru güncelleme
    ipcMain.on('update-game-score', (event, score) => {
        if (score > (settings.gameHighScore || 0)) {
            settings.gameHighScore = score;
            saveSettings(settings);
            checkAchievements();
        }
    });

    ipcMain.on('get-speed', (event) => {
        console.log('Received get-speed IPC');
        event.reply('speed-status', settings.speed);
    });

    ipcMain.on('get-size', (event) => {
        console.log('Received get-size IPC');
        event.reply('size-status', settings.size);
    });

function createWindow() {
    const primaryDisplay = screen.getPrimaryDisplay();
    const { width, height } = primaryDisplay.workAreaSize;

    mainWindow = new BrowserWindow({
        width: width,
        height: height,
        x: 0,
        y: 0,
        transparent: true,
        frame: false,
        alwaysOnTop: settings.alwaysOnTop,
        skipTaskbar: true,
        icon: path.join(__dirname, 'assets', 'icon.png'),
        hasShadow: settings.showShadow,
        backgroundColor: '#00000000',
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });

    // Pencere kapatma olayını dinle
    mainWindow.on('close', handleWindowClose);

    mainWindow.setBackgroundColor('#00000000');
    
    // Karakter seçimine göre HTML dosyasını yükle
    mainWindow.loadFile('index.html');
    // İlk hız ve boyut ayarlarını gönder
    mainWindow.webContents.send('current-speed', settings.speed);
    mainWindow.webContents.send('current-size', settings.size);

    mainWindow.setAlwaysOnTop(settings.alwaysOnTop, 'floating');
    mainWindow.setVisibleOnAllWorkspaces(true);
    mainWindow.setIgnoreMouseEvents(true, { forward: true });

    // Automatically toggle window visibility after a short delay to fix initial rendering issues
    setTimeout(() => {
        if (mainWindow) {
            mainWindow.hide();
            mainWindow.show();
        }
    }, 200);

    createTray();
}

function handleWindowClose(event) {
    if (settings.minimizeToTray) {
        event.preventDefault();
        mainWindow.hide();
    } else {
        app.quit();
    }
}

// Gerekli modüllerin indirilmesi
async function downloadRequiredModules() {
  const modules = [
    {
      name: 'assets',
      url: 'https://github.com/blackeker/oneko-assets/archive/refs/heads/main.zip',
      destination: path.join(app.getPath('userData'), 'assets')
    }
  ];

  for (const module of modules) {
    try {
      const response = await fetch(module.url);
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      
      const buffer = await response.buffer();
      const zipPath = path.join(app.getPath('temp'), `${module.name}.zip`);
      
      fs.writeFileSync(zipPath, buffer);
      
      // ZIP dosyasını geçici bir klasöre çıkart
      const tempExtractDir = path.join(app.getPath('temp'), `${module.name}_extract`);
      if (fs.existsSync(tempExtractDir)) fse.removeSync(tempExtractDir);
      await extract(zipPath, { dir: tempExtractDir });
      
      // ZIP içindeki ana klasörü bul (ör: oneko-assets-main)
      const extractedFolders = fs.readdirSync(tempExtractDir);
      const mainFolder = extractedFolders.find(f => f.startsWith('oneko-assets'));
      if (!mainFolder) throw new Error('Ana klasör bulunamadı!');
      const mainFolderPath = path.join(tempExtractDir, mainFolder);
      
      // Hedefe taşı
      if (fs.existsSync(module.destination)) fse.removeSync(module.destination);
      fse.moveSync(mainFolderPath, module.destination, { overwrite: true });
      
      // Geçici klasörleri sil
      fse.removeSync(tempExtractDir);
      fs.unlinkSync(zipPath);
      
      console.log(`${module.name} başarıyla indirildi ve kuruldu.`);
    } catch (error) {
      console.error(`${module.name} indirilirken hata oluştu:`, error);
      dialog.showErrorBox('Hata', `${module.name} indirilirken bir hata oluştu. Lütfen internet bağlantınızı kontrol edin ve tekrar deneyin.`);
    }
  }
} 