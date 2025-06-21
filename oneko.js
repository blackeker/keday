(function onekoCanvas() {
  const { ipcRenderer } = require('electron');
  ipcRenderer.send('get-current-speed');
  ipcRenderer.on('current-speed', (event, speed) => {
    nekoSpeed = speed;
  });
  const canvas = document.getElementById('oneko-canvas');
  if (!canvas) {
    console.error('Canvas elementi bulunamadı!');
    return;
  }
  canvas.style.position = 'fixed';
  canvas.style.top = '0';
  canvas.style.left = '0';
  canvas.style.width = '100%';
  canvas.style.height = '100%';
  canvas.style.pointerEvents = 'none';
  canvas.style.zIndex = '9999';
  canvas.style.backgroundColor = 'transparent';
  const ctx = canvas.getContext('2d', { alpha: true, desynchronized: true });
  if (!ctx) {
    console.error('Canvas context oluşturulamadı!');
    return;
  }
  const offscreenCanvas = document.createElement('canvas');
  const offscreenCtx = offscreenCanvas.getContext('2d', { alpha: true, desynchronized: true });
  function resizeCanvas() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
    offscreenCanvas.width = canvas.width;
    offscreenCanvas.height = canvas.height;
  }
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();
  const spriteCache = new Map();
  function loadSprite(src) {
    if (spriteCache.has(src)) {
      return spriteCache.get(src);
    }
    const img = new Image();
    img.onload = function() {
      if (!animationStarted) {
        requestAnimationFrame(animate);
        animationStarted = true;
      }
    };
    img.onerror = function(error) {
      console.error('Sprite yükleme hatası:', error);
      const paths = [
        src,
        `./assets/${currentCharacter}.gif`,
        `../assets/${currentCharacter}.gif`,
        `../../assets/${currentCharacter}.gif`,
        path.join(process.resourcesPath, 'assets', `${currentCharacter}.gif`),
        path.join(__dirname, 'assets', `${currentCharacter}.gif`),
        path.join(process.resourcesPath, 'app.asar.unpacked', 'assets', `${currentCharacter}.gif`)
      ];
      let currentPathIndex = 0;
      const tryNextPath = () => {
        if (currentPathIndex < paths.length) {
          const currentPath = paths[currentPathIndex];
          const fs = require('fs');
          if (fs.existsSync(currentPath)) {
            img.src = currentPath;
          } else {
            currentPathIndex++;
            tryNextPath();
          }
        } else {
          console.error('Hiçbir sprite yolu çalışmadı!');
          img.src = 'assets/icon.png';
        }
      };
      tryNextPath();
    };
    img.src = src;
    spriteCache.set(src, img);
    return img;
  }
  function cleanup() {
    spriteCache.clear();
    offscreenCanvas.width = 0;
    offscreenCanvas.height = 0;
    canvas.width = 0;
    canvas.height = 0;
  }
  window.addEventListener('beforeunload', cleanup);
  let currentCharacter = 'oneko';
  let animationStarted = false;
  let lastFrameTime = 0;
  const frameDelay = 150;
  function animate(timestamp) {
    updateFPS(timestamp);
    if (timestamp - lastFrameTime < frameDelay) {
      requestAnimationFrame(animate);
      return;
    }
    lastFrameTime = timestamp;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    offscreenCtx.clearRect(0, 0, offscreenCanvas.width, offscreenCanvas.height);
    updateNekoPosition();
    if (sprite && sprite.complete) {
      const spriteFrame = spriteSets[currentSprite][currentFrame];
      if (spriteFrame) {
        const size = 32 * (nekoSize / 100);
        offscreenCtx.drawImage(
          sprite,
          spriteFrame[0] * 32,
          spriteFrame[1] * 32,
          32,
          32,
          nekoPosX,
          nekoPosY,
          size,
          size
        );
      }
    }
    ctx.drawImage(offscreenCanvas, 0, 0);
    currentFrame = (currentFrame + 1) % spriteSets[currentSprite].length;
    requestAnimationFrame(animate);
  }
  let sprite = loadSprite(`assets/${currentCharacter}.gif`);
  ipcRenderer.on('set-character', (event, character) => {
    currentCharacter = character;
    spriteCache.clear();
    sprite = loadSprite(`assets/${character}.gif`);
    sprite.onload = function() {};
    sprite.onerror = function(error) {
      console.error('Yeni karakter yükleme hatası:', error);
      sprite.src = 'assets/icon.png';
    };
    sprite.src = `assets/${character}.gif`;
  });
  ipcRenderer.on('get-current-character', () => {
    ipcRenderer.send('current-character', currentCharacter);
  });
  const spriteSets = {
    idle: [[3, 3]],
    alert: [[7, 3]],
    scratchSelf: [[5, 0], [6, 0], [7, 0]],
    scratchWallN: [[0, 0], [0, 1]],
    scratchWallS: [[7, 1], [6, 2]],
    scratchWallE: [[2, 2], [2, 3]],
    scratchWallW: [[4, 0], [4, 1]],
    tired: [[3, 2]],
    sleeping: [[2, 0], [2, 1]],
    N: [[1, 2], [1, 3]],
    NE: [[0, 2], [0, 3]],
    E: [[3, 0], [3, 1]],
    SE: [[5, 1], [5, 2]],
    S: [[6, 3], [7, 2]],
    SW: [[5, 3], [6, 1]],
    W: [[4, 2], [4, 3]],
    NW: [[1, 0], [1, 1]]
  };
  let lastMoveTime = Date.now();
  let isSleeping = false;
  let isTired = false;
  let lastStateChange = Date.now();
  let isScratching = false;
  let lastScratchTime = Date.now();
  let sleepStartTime = Date.now();
  const MIN_SLEEP_DURATION = 5000;
  let nekoPosX = 32;
  let nekoPosY = 32;
  let mousePosX = 0;
  let mousePosY = 0;
  let nekoSpeed = 10;
  let nekoSize = 100;
  let currentSprite = 'idle';
  let currentFrame = 0;
  let followMouse = true;
  let lastMousePosX = 0;
  let lastMousePosY = 0;
  let suddenMovement = false;
  const EDGE_THRESHOLD = 20;
  let frameCount = 0;
  let lastFPSUpdate = 0;
  function updateFPS(now) {
    frameCount++;
    if (now - lastFPSUpdate >= 1000) {
      const fps = Math.round((frameCount * 1000) / (now - lastFPSUpdate));
      frameCount = 0;
      lastFPSUpdate = now;
    }
  }
  document.addEventListener('mousemove', function(event) {
    const oldMousePosX = mousePosX;
    const oldMousePosY = mousePosY;
    mousePosX = event.clientX;
    mousePosY = event.clientY;
    const dx = mousePosX - oldMousePosX;
    const dy = mousePosY - oldMousePosY;
    const movement = Math.sqrt(dx * dx + dy * dy);
    suddenMovement = movement > 50;
    if (movement > 0 && isSleeping) {
      isSleeping = false;
      isTired = false;
      currentSprite = 'idle';
    }
  });
  function updateNekoPosition() {
    if (!followMouse) return;
    const dx = mousePosX - nekoPosX;
    const dy = mousePosY - nekoPosY;
    const distance = Math.sqrt(dx * dx + dy * dy);
    if (suddenMovement && !isSleeping && !isScratching) {
      currentSprite = 'alert';
      suddenMovement = false;
      return;
    }
    if (isSleeping) {
      return;
    }
    if (distance < 5) {
      currentSprite = 'idle';
      return;
    }
    if (!isSleeping && !isTired && !isScratching) {
      if (nekoPosY < EDGE_THRESHOLD) {
        currentSprite = 'scratchWallN';
        isScratching = true;
        lastScratchTime = Date.now();
        return;
      } else if (nekoPosY > canvas.height - EDGE_THRESHOLD) {
        currentSprite = 'scratchWallS';
        isScratching = true;
        lastScratchTime = Date.now();
        return;
      } else if (nekoPosX < EDGE_THRESHOLD) {
        currentSprite = 'scratchWallW';
        isScratching = true;
        lastScratchTime = Date.now();
        return;
      } else if (nekoPosX > canvas.width - EDGE_THRESHOLD) {
        currentSprite = 'scratchWallE';
        isScratching = true;
        lastScratchTime = Date.now();
        return;
      }
    }
    const normalizedDx = dx / distance;
    const normalizedDy = dy / distance;
    const speed = Math.min(nekoSpeed, distance * 0.5);
    nekoPosX += normalizedDx * speed;
    nekoPosY += normalizedDy * speed;
    const angle = Math.atan2(dy, dx) * 180 / Math.PI;
    if (angle >= -22.5 && angle < 22.5) {
      currentSprite = 'E';
    } else if (angle >= 22.5 && angle < 67.5) {
      currentSprite = 'SE';
    } else if (angle >= 67.5 && angle < 112.5) {
      currentSprite = 'S';
    } else if (angle >= 112.5 && angle < 157.5) {
      currentSprite = 'SW';
    } else if (angle >= 157.5 || angle < -157.5) {
      currentSprite = 'W';
    } else if (angle >= -157.5 && angle < -112.5) {
      currentSprite = 'NW';
    } else if (angle >= -112.5 && angle < -67.5) {
      currentSprite = 'N';
    } else if (angle >= -67.5 && angle < -22.5) {
      currentSprite = 'NE';
    }
    if (distance >= 8) {
      lastMoveTime = Date.now();
      if (isSleeping) {
        isSleeping = false;
        isTired = false;
      }
      if (isScratching) {
        isScratching = false;
      }
    }
  }
  requestAnimationFrame(animate);
  ipcRenderer.on('set-speed', (event, speed) => {
    nekoSpeed = speed;
  });
  ipcRenderer.on('set-size', (event, size) => {
    nekoSize = size;
  });
  ipcRenderer.on('set-follow-mouse', (event, enabled) => {
    followMouse = enabled;
  });
  ipcRenderer.on('global-mouse-move', (event, pos) => {
    mousePosX = pos.x;
    mousePosY = pos.y;
  });
})();