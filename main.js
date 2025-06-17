const { app, BrowserWindow, screen, Tray, Menu, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const Store = require('electron-store');
const { autoUpdater } = require('electron-updater');
const { exec } = require('child_process');

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
        hiddenWindows: [], // Gizlenecek pencerelerin listesi
        hideOnFullscreen: true, // Tam ekran modunda gizleme
        character: 'oneko' // Varsayılan karakter
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
let tray = null;
let isVisible = true;
    let settings = loadSettings();
let settingsWindow = null;
let isQuitting = false;
let catProcess = null; // Keday sürecini tutacak değişken

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
        if (tray) {
            tray.destroy();
        }
        
        const iconPath = path.join(__dirname, 'assets', 'icon.ico');
        tray = new Tray(iconPath);
        tray.setToolTip('Keday');
        
        const contextMenu = Menu.buildFromTemplate([
            { 
                label: 'Ayarlar', 
                click: () => {
                    openSettingsWindow();
                }
            },
            { type: 'separator' },
            { 
                label: 'Çıkış', 
                click: () => {
                    if (catProcess) {
                        catProcess.kill();
                    }
                    if (mainWindow) {
                        mainWindow.destroy();
                    }
                    if (tray) {
                        tray.destroy();
                    }
                    app.quit();
                }
            }
        ]);
        
        tray.setContextMenu(contextMenu);
        tray.on('click', () => {
            openSettingsWindow();
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

    app.whenReady().then(() => {
        createWindow();
        createTray();
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
        console.log('Karakter değiştiriliyor:', character);
        settings.character = character;
        saveSettings(settings);
        BrowserWindow.getAllWindows().forEach(win => {
            win.webContents.send('set-character', character);
        });
    });

    ipcMain.on('get-current-character', (event) => {
        console.log('Mevcut karakter gönderiliyor:', settings.character || 'oneko');
        event.reply('current-character', settings.character || 'oneko');
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

    ipcMain.on('get-speed', (event) => {
        console.log('Received get-speed IPC');
        event.reply('speed-status', settings.speed);
    });

    ipcMain.on('get-size', (event) => {
        console.log('Received get-size IPC');
        event.reply('size-status', settings.size);
    });

    // Aktif pencere kontrolü
    let activeWindow = null;
    let isFullscreen = false;

    // Aktif pencereyi kontrol et
    function checkActiveWindow() {
        const focusedWindow = screen.getDisplayNearestPoint(screen.getCursorScreenPoint());
        const windows = BrowserWindow.getAllWindows();
        
        // Tam ekran kontrolü
        isFullscreen = focusedWindow.bounds.width === screen.getPrimaryDisplay().workAreaSize.width &&
                      focusedWindow.bounds.height === screen.getPrimaryDisplay().workAreaSize.height;

        // Aktif pencereyi bul
        activeWindow = windows.find(win => {
            const bounds = win.getBounds();
            return bounds.x <= focusedWindow.bounds.x && 
                   bounds.x + bounds.width >= focusedWindow.bounds.x &&
                   bounds.y <= focusedWindow.bounds.y && 
                   bounds.y + bounds.height >= focusedWindow.bounds.y;
        });

        // Kedi penceresini kontrol et
        if (mainWindow) {
            const shouldHide = (settings.hideOnFullscreen && isFullscreen) ||
                             (activeWindow && settings.hiddenWindows.includes(activeWindow.getTitle()));
            
            if (shouldHide) {
                mainWindow.hide();
            } else {
                mainWindow.show();
            }
        }
    }

    // Pencere kontrolü için interval
    setInterval(checkActiveWindow, 1000);

    // IPC Handlers
    ipcMain.on('set-hidden-windows', (event, windows) => {
        console.log('Gizlenecek pencereler güncelleniyor:', windows);
        settings.hiddenWindows = windows;
        saveSettings(settings);
    });

    ipcMain.on('set-hide-on-fullscreen', (event, hide) => {
        console.log('Tam ekran gizleme ayarı güncelleniyor:', hide);
        settings.hideOnFullscreen = hide;
        saveSettings(settings);
    });

    // Aktif uygulamaları al
    function getActiveApplications() {
        return new Promise((resolve) => {
            exec('tasklist /v /fo csv', (error, stdout) => {
                if (error) {
                    console.error('Aktif uygulamalar alınırken hata:', error);
                    resolve([]);
                    return;
                }

                const lines = stdout.split('\n');
                const activeApps = new Set();
                const systemProcesses = [
                    'System', 'Registry', 'smss.exe', 'csrss.exe', 'wininit.exe',
                    'services.exe', 'lsass.exe', 'svchost.exe', 'spoolsv.exe',
                    'explorer.exe', 'taskmgr.exe', 'dwm.exe', 'conhost.exe',
                    'RuntimeBroker.exe', 'SearchHost.exe', 'ShellExperienceHost.exe',
                    'StartMenuExperienceHost.exe', 'TextInputHost.exe', 'WmiPrvSE.exe',
                    'ApplicationFrameHost.exe', 'SearchIndexer.exe', 'sihost.exe',
                    'ctfmon.exe', 'fontdrvhost.exe', 'SecurityHealthService.exe',
                    'SecurityHealthSystray.exe', 'smartscreen.exe', 'WmiApSrv.exe',
                    'WUDFHost.exe', 'Idle', 'winlogon.exe', 'winlogon.exe', 'winlogon.exe',
                ];

                lines.forEach(line => {
                    if (line.includes('"Window Title"')) return; // Başlık satırını atla
                    const parts = line.split('","');
                    if (parts.length >= 8) {
                        const processName = parts[0].replace(/"/g, '').trim();
                        const windowTitle = parts[7].replace(/"/g, '').trim();
                        // Microsoft hizmetleri ve sistem uygulamaları filtrele
                        if (
                            !systemProcesses.includes(processName) &&
                            windowTitle &&
                            windowTitle !== 'N/A' &&
                            windowTitle !== 'Default IME' &&
                            !windowTitle.toLowerCase().includes('microsoft') &&
                            !windowTitle.toLowerCase().includes('windows') &&
                            !windowTitle.toLowerCase().includes('host process') &&
                            !windowTitle.toLowerCase().includes('service') &&
                            !windowTitle.toLowerCase().includes('runtime broker') &&
                            !windowTitle.toLowerCase().includes('background task host')
                        ) {
                            activeApps.add(windowTitle);
                        }
                    }
                });

                resolve(Array.from(activeApps));
            });
        });
    }

    ipcMain.on('get-window-list', async (event) => {
        const activeApps = await getActiveApplications();
        console.log('Aktif uygulamalar:', activeApps);
        event.reply('window-list', activeApps);
    });

    ipcMain.on('get-settings', (event) => {
        console.log('Ayarlar gönderiliyor:', settings);
        event.reply('current-settings', settings);
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

// Keday sürecini başlatma fonksiyonu
function startCat() {
    if (catProcess) {
        catProcess.kill();
    }
    catProcess = require('child_process').spawn('Keday', [], {
        detached: true
    });
}

// Keday sürecini durdurma fonksiyonu
function stopCat() {
    if (catProcess) {
        catProcess.kill();
        catProcess = null;
    }
}

app.on('before-quit', () => {
    isQuitting = true;
    // Tüm pencereleri kapat
    if (mainWindow) {
        mainWindow.destroy();
    }
    if (settingsWindow) {
        settingsWindow.destroy();
    }
    // Tray'i kaldır
    if (tray) {
        tray.destroy();
    }
    // Keday sürecini sonlandır
    if (catProcess) {
        catProcess.kill();
    }
    // Kullanıcı verilerini temizle
    const userDataPath = app.getPath('userData');
    try {
        fs.rmSync(userDataPath, { recursive: true, force: true });
    } catch (error) {
        console.error('Kullanıcı verileri temizlenirken hata:', error);
    }
});