const { app, BrowserWindow, screen, Tray, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');

let mainWindow;
let tray;
let isVisible = true;

const iconPath = path.join(__dirname, 'assets', 'icon.png');

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
    tray = new Tray(iconPath);
    const contextMenu = Menu.buildFromTemplate([
        { type: 'separator' },
        { label: 'Çıkış', click: () => app.quit() }
    ]);
    tray.setToolTip('Oneko Kedi');
    tray.setContextMenu(contextMenu);
    
    tray.on('click', (event, bounds) => {
        toggleCat();
    });
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
        alwaysOnTop: true,
        type: 'toolbar',
        icon: iconPath,
        hasShadow: false,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });

    mainWindow.setBackgroundColor('#00000000');
    
    mainWindow.loadFile('index.html');
    mainWindow.setAlwaysOnTop(true, 'screen-saver');
    mainWindow.setVisibleOnAllWorkspaces(true);
    mainWindow.setIgnoreMouseEvents(true, { forward: true });

    setInterval(() => {
        if (!mainWindow.isAlwaysOnTop()) {
            mainWindow.setAlwaysOnTop(true, 'screen-saver');
        }
    }, 1000);

    createTray();
}

app.whenReady().then(() => {
    createWindow();

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