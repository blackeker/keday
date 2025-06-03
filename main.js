const { app, BrowserWindow, screen, Tray, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');

// Disable hardware acceleration
app.disableHardwareAcceleration();

let mainWindow;
let tray;
let isVisible = true;

const iconPath = path.join(__dirname, 'assets', 'icon.png');
const icoPath = path.join(__dirname, 'assets', 'icon.ico');

// Disable GPU acceleration
app.commandLine.appendSwitch('disable-gpu');
// Disable GPU compositing
app.commandLine.appendSwitch('disable-gpu-compositing');

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
        skipTaskbar: true,
        icon: iconPath,
        hasShadow: false,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });

    mainWindow.setBackgroundColor('#00000000');
    
    mainWindow.loadFile('index.html');
    mainWindow.setAlwaysOnTop(true, 'floating');
    mainWindow.setVisibleOnAllWorkspaces(true);
    mainWindow.setIgnoreMouseEvents(true, { forward: true });

    // Automatically toggle window visibility after a short delay to fix initial rendering issues
    setTimeout(() => {
        if (mainWindow) {
            mainWindow.hide();
            mainWindow.show();
        }
    }, 200); // 200ms delay

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