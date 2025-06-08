const { app, BrowserWindow, screen, Tray, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const AutoLaunch = require('auto-launch');

// Önbellek temizleme
app.commandLine.appendSwitch('disable-gpu-cache');
app.commandLine.appendSwitch('disable-software-rasterizer');
app.commandLine.appendSwitch('disable-gpu-compositing');

// Uygulama başlamadan önce önbelleği temizle
app.on('ready', () => {
    const userDataPath = app.getPath('userData');
    const cachePath = path.join(userDataPath, 'Cache');
    if (fs.existsSync(cachePath)) {
        fs.rmSync(cachePath, { recursive: true, force: true });
    }
});

let mainWindow;
let tray;
let isVisible = true;

// Varsayılan ayarlar
const defaultSettings = {
    speed: 10,
    size: 100,
    alwaysOnTop: true,
    minimizeToTray: true,
    autoLaunch: false,
    badges: [],
    tasks: [],
    gameHighScore: 0
};

// Başarı rozetleri
const badges = [
    {
        id: 'first_steps',
        name: 'İlk Adımlar',
        description: 'İlk görevi tamamla',
        icon: './assets/badges/first_steps.png',
        unlocked: false
    },
    {
        id: 'speed_demon',
        name: 'Hız Ustası',
        description: 'Kediyi maksimum hıza çıkar',
        icon: './assets/badges/speed_demon.png',
        unlocked: false
    },
    {
        id: 'game_master',
        name: 'Oyun Ustası',
        description: 'Mini oyunda 1000 puan topla',
        icon: './assets/badges/game_master.png',
        unlocked: false
    }
];

// Günlük görevler
const dailyTasks = [
    {
        id: 'play_game',
        name: 'Mini oyun oyna',
        description: 'Mini oyunu en az 1 kez oyna',
        progress: 0,
        completed: false
    },
    {
        id: 'high_score',
        name: 'Yüksek skor yap',
        description: 'Mini oyunda 500 puan topla',
        progress: 0,
        completed: false
    },
    {
        id: 'pet_cat',
        name: 'Kediyi sev',
        description: 'Kediyi 10 kez tıkla',
        progress: 0,
        completed: false
    }
];

// Mini oyun penceresi
let gameWindow = null;

// Ayarları yükle
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

// Ayarları kaydet
function saveSettings(settings) {
    const settingsPath = path.join(app.getPath('userData'), 'settings.json');
    try {
        fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
    } catch (error) {
        console.error('Ayarlar kaydedilirken hata:', error);
    }
}

// Mevcut ayarları yükle
let settings = loadSettings();

const iconPath = path.join(__dirname, 'assets', 'icon.png');
const icoPath = path.join(__dirname, 'assets', 'icon.ico');

// Geliştirme modunda başlık çubuğu sorununu gidermek için .png ikonunu zorla
// Bu, ikon.ico dosyasının başlık çubuğunda düzgün görünmemesi sorununu teşhis etmeye yardımcı olacaktır.
let appIcon = iconPath; // Varsayılan olarak icon.png kullan

const nekoAutoLauncher = new AutoLaunch({ name: 'Oneko' });

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
    tray = new Tray(appIcon);
    const contextMenu = Menu.buildFromTemplate([
        { label: 'Ayarlar', click: () => openSettingsWindow() },
        { type: 'separator' },
        { label: 'Çıkış', click: () => app.quit() }
    ]);
    tray.setToolTip('Oneko Kedi');
    tray.setContextMenu(contextMenu);
    
    tray.on('click', (event, bounds) => {
        toggleCat();
    });
}

let settingsWindow = null;
function openSettingsWindow() {
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
            contextIsolation: false
        }
    });
    settingsWindow.setMenu(null);
    settingsWindow.loadFile('settings.html');
    settingsWindow.on('closed', () => { settingsWindow = null; });
}

// Pencere kapatma olayını yönet
function handleWindowClose(event) {
    if (settings.minimizeToTray) {
        event.preventDefault();
        mainWindow.hide();
    } else {
        app.quit();
    }
}

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
        icon: appIcon,
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
    mainWindow.loadFile(settings.character === 'neko' ? 'index.html' : 'anime-girl.html');
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

// Uygulama başladığında ayarları uygula
app.whenReady().then(async () => {
    // Otomatik başlatma ayarını kontrol et
    try {
        const isAutoLaunchEnabled = await nekoAutoLauncher.isEnabled();
        settings.autoLaunch = isAutoLaunchEnabled;
    } catch (error) {
        console.error('Otomatik başlatma durumu kontrol edilirken hata:', error);
    }

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
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

// IPC Handlers
ipcMain.on('set-speed', (event, speed) => {
    settings.speed = speed;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-speed', speed);
    });
});

ipcMain.on('set-size', (event, size) => {
    settings.size = size;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-size', size);
    });
});

ipcMain.on('set-always-on-top', (event, enabled) => {
    settings.alwaysOnTop = enabled;
    saveSettings(settings);
    if (mainWindow) {
        mainWindow.setAlwaysOnTop(enabled, 'floating');
    }
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('always-on-top-status', enabled);
    });
});

ipcMain.on('set-shadow', (event, enabled) => {
    settings.showShadow = enabled;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-shadow', enabled);
    });
});

ipcMain.on('set-shadow-opacity', (event, opacity) => {
    settings.shadowOpacity = opacity;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-shadow-opacity', opacity);
    });
});

ipcMain.on('set-shadow-size', (event, size) => {
    settings.shadowSize = size;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-shadow-size', size);
    });
});

ipcMain.on('get-current-speed', (event) => {
    event.reply('current-speed', settings.speed);
});

ipcMain.on('get-current-size', (event) => {
    event.reply('current-size', settings.size);
});

ipcMain.on('get-always-on-top', (event) => {
    event.reply('always-on-top-status', settings.alwaysOnTop);
});

ipcMain.on('get-shadow-status', (event) => {
    event.reply('shadow-status', settings.showShadow);
});

ipcMain.on('set-auto-launch', (event, enabled) => {
    settings.autoLaunch = enabled;
    saveSettings();
    app.setLoginItemSettings({
        openAtLogin: enabled,
        path: app.getPath('exe')
    });
});

ipcMain.on('get-auto-launch', async (event) => {
    try {
        const enabled = await nekoAutoLauncher.isEnabled();
        event.sender.send('auto-launch-status', enabled);
    } catch (error) {
        console.error('Otomatik başlatma durumu alınırken hata:', error);
        event.sender.send('auto-launch-status', false);
    }
});

ipcMain.on('set-character', (event, character) => {
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
    event.reply('current-character', settings.character);
});

ipcMain.on('set-idle-time', (event, time) => {
    settings.idleTime = time;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('set-idle-time', time);
    });
});

ipcMain.on('set-opacity', (event, opacity) => {
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
    settings.followMouse = enabled;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('follow-mouse-status', enabled);
    });
});

ipcMain.on('set-sound-effects', (event, enabled) => {
    settings.soundEffects = enabled;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('sound-effects-status', enabled);
    });
});

ipcMain.on('set-emotions', (event, enabled) => {
    settings.emotions = enabled;
    saveSettings(settings);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send('emotions-status', enabled);
    });
});

ipcMain.on('set-trail', (event, enabled) => {
    settings.showTrail = enabled;
    saveSettings(settings);
    mainWindow.webContents.send('set-trail', enabled);
});

ipcMain.on('set-minimize-to-tray', (event, enabled) => {
    settings.minimizeToTray = enabled;
    saveSettings();
});

// Get handlers for new settings
ipcMain.on('get-idle-time', (event) => {
    event.reply('idle-time-status', settings.idleTime);
});

ipcMain.on('get-opacity', (event) => {
    event.reply('opacity-status', settings.opacity);
});

ipcMain.on('get-follow-mouse', (event) => {
    event.reply('follow-mouse-status', settings.followMouse);
});

ipcMain.on('get-sound-effects', (event) => {
    event.reply('sound-effects-status', settings.soundEffects);
});

ipcMain.on('get-emotions', (event) => {
    event.reply('emotions-status', settings.emotions);
});

ipcMain.on('get-trail-status', (event) => {
    event.reply('trail-status', settings.showTrail);
});

ipcMain.on('get-minimize-to-tray', (event) => {
    event.reply('minimize-to-tray-status', settings.minimizeToTray);
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
        mainWindow.webContents.send('set-always-on-top', settings.alwaysOnTop);
    }
    if (settings.minimizeToTray !== undefined) {
        minimizeToTray = settings.minimizeToTray;
    }
    if (settings.autoLaunch !== undefined) {
        setAutoLaunch(settings.autoLaunch);
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
        saveSettings();
        
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
    saveSettings();
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
        saveSettings();
        checkAchievements();
    }
}); 