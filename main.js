const { app, BrowserWindow, screen, Tray, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const Store = require('electron-store');
const { autoUpdater } = require('electron-updater');
const isDev = process.env.NODE_ENV === 'development';

app.commandLine.appendSwitch('disable-gpu-cache');
app.commandLine.appendSwitch('disable-software-rasterizer');
app.commandLine.appendSwitch('disable-gpu-compositing');

// Uygulama başlamadan önce önbelleği temizle
app.on('ready', () => {
    const userDataPath = app.getPath('userData');
    const cachePath = path.join(userDataPath, 'Cache');
    if (fs.existsSync(cachePath)) {
        try {
            fs.rmSync(cachePath, { recursive: true, force: true });
        } catch (error) {
            console.error('Önbellek temizlenirken hata oluştu:', error);
        }
    }
});
    
const store = new Store({
    defaults: {
        speed: 10,
        size: 100,
        alwaysOnTop: true,
        minimizeToTray: true,
        autoLaunch: true,
        hiddenWindows: [],
        hideOnFullscreen: true,
        character: 'oneko',
        showShadow: false,
        shadowOpacity: 1,
        shadowSize: 10,
        idleTime: 0,
        opacity: 100,
        followMouse: false,
        soundEffects: false,
        emotions: false,
        showTrail: false
    }
});

let mainWindow;
let tray = null;
let settingsWindow = null;

function getAssetPath(file) {
    if (isDev) {
        return path.join(__dirname, 'assets', file);
    }
    return path.join(process.resourcesPath, 'assets', file);
}

function createTray() {
    if (tray) tray.destroy();
    const trayIcon = getAssetPath('icon.png');
    tray = new Tray(trayIcon);
    tray.setToolTip('Keday');
    const contextMenu = Menu.buildFromTemplate([
        {
            label: 'Ayarlar',
            click: openSettingsWindow
        },
        {
            label: 'Geliştirici Konsolu',
            click: () => {
                if (mainWindow && mainWindow.webContents) {
                    mainWindow.show();
                    mainWindow.webContents.openDevTools({ mode: 'detach' });
                }
            }
        },
        { type: 'separator' },
        {
            label: 'Çıkış',
            click: () => {
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
    tray.on('click', openSettingsWindow);
}

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
            contextIsolation: false,
            devTools: true
        }
    });
    settingsWindow.setMenu(null);
    settingsWindow.loadFile('settings.html');
    settingsWindow.webContents.on('did-finish-load', () => {
        settingsWindow.webContents.send('current-settings', store.store);
    });
    settingsWindow.on('closed', () => { settingsWindow = null; });
}

// Uygulamanın tek bir örneğinin çalışmasını sağla
const gotTheLock = app.requestSingleInstanceLock();

if (!gotTheLock) {
    app.quit();
} else {
    app.on('second-instance', (event, commandLine, workingDirectory) => {
        if (mainWindow) {
            if (mainWindow.isMinimized()) mainWindow.restore();
            mainWindow.focus();
        }
    });
}

app.whenReady().then(() => {
    copyAssets();
    createWindow();
    autoUpdater.checkForUpdatesAndNotify();
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

function updateSetting(key, value) {
    store.set(key, value);
    BrowserWindow.getAllWindows().forEach(win => {
        win.webContents.send(`set-${key}`, value);
    });
}

ipcMain.on('update-setting', (event, key, value) => {
    updateSetting(key, value);
    if (key === 'alwaysOnTop' && mainWindow) {
        mainWindow.setAlwaysOnTop(value, 'floating');
    }
    if (key === 'opacity' && mainWindow) {
        mainWindow.setOpacity(value / 100);
    }
});

ipcMain.on('get-setting', (event, key) => {
    event.reply(`${key}-status`, store.get(key));
});

ipcMain.on('get-settings', (event) => {
    event.reply('current-settings', store.store);
});

ipcMain.on('open-settings', openSettingsWindow);

let mouseInterval = null;

function createWindow() {
    try {
        const primaryDisplay = screen.getPrimaryDisplay();
        const { width, height } = primaryDisplay.workAreaSize;

        mainWindow = new BrowserWindow({
            width: width,
            height: height,
            x: 0,
            y: 0,
            transparent: true,
            frame: false,
            alwaysOnTop: store.get('alwaysOnTop'),
            skipTaskbar: true,
            icon: getAssetPath('icon.png'),
            hasShadow: store.get('showShadow'),
            backgroundColor: '#00000000',
            webPreferences: {
                nodeIntegration: true,
                contextIsolation: false,
                backgroundThrottling: true
            }
        });

        mainWindow.on('close', handleWindowClose);
        mainWindow.loadFile('index.html');
        mainWindow.webContents.send('current-speed', store.get('speed'));
        mainWindow.webContents.send('current-size', store.get('size'));
        mainWindow.setAlwaysOnTop(store.get('alwaysOnTop'), 'floating');
        mainWindow.setVisibleOnAllWorkspaces(true);
        mainWindow.setIgnoreMouseEvents(true, { forward: true });
        setTimeout(() => {
            if (mainWindow) {
                mainWindow.hide();
                mainWindow.show();
            }
        }, 200);
        createTray();
        mouseInterval = setInterval(() => {
            if (mainWindow && !mainWindow.isDestroyed()) {
                const { x, y } = screen.getCursorScreenPoint();
                mainWindow.webContents.send('global-mouse-move', { x, y });
            }
        }, 100);
    } catch (e) {
        console.error('Ana pencere oluşturulurken hata:', e);
    }
}

function handleWindowClose(event) {
    if (mouseInterval) {
        clearInterval(mouseInterval);
        mouseInterval = null;
    }
    if (store.get('minimizeToTray')) {
        event.preventDefault();
        mainWindow.hide();
    } else {
        app.quit();
    }
}

app.on('before-quit', () => {
    if (mainWindow) {
        mainWindow.destroy();
    }
    if (settingsWindow) {
        settingsWindow.destroy();
    }
    if (tray) {
        tray.destroy();
    }
    const userDataPath = app.getPath('userData');
    try {
        fs.rmSync(userDataPath, { recursive: true, force: true });
    } catch (error) {
        console.error('Kullanıcı verileri temizlenirken hata:', error);
    }
});

const getAssetsPath = () => {
  if (process.env.NODE_ENV === 'development') {
    return path.join(__dirname, 'assets');
  }
  return path.join(process.resourcesPath, 'assets');
};

const copyAssets = () => {
  const sourcePath = path.join(__dirname, 'assets');
  const targetPath = getAssetsPath();
  if (!fs.existsSync(targetPath)) {
    fs.mkdirSync(targetPath, { recursive: true });
  }
  if (fs.existsSync(sourcePath)) {
    const files = fs.readdirSync(sourcePath);
    files.forEach(file => {
      const sourceFile = path.join(sourcePath, file);
      const targetFile = path.join(targetPath, file);
      if (!fs.existsSync(targetFile)) {
        fs.copyFileSync(sourceFile, targetFile);
      }
    });
  } else {
    console.error('Kaynak assets klasörü bulunamadı:', sourcePath);
  }
};