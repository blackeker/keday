(function onekoCanvas() {
  const { ipcRenderer } = require('electron');
  console.log('Oneko başlatılıyor...');
  
  // Ana süreçten mevcut hızı al
  ipcRenderer.send('get-current-speed');

  // Mevcut hız güncellemelerini dinle
  ipcRenderer.on('current-speed', (event, speed) => {
    nekoSpeed = speed;
    console.log('Başlangıç/Güncel hız alındı:', speed);
  });
  
  const canvas = document.getElementById('oneko-canvas');
  if (!canvas) {
    console.error('Canvas elementi bulunamadı!');
    return;
  }
  console.log('Canvas bulundu:', canvas.width, 'x', canvas.height);
  
  const ctx = canvas.getContext('2d', { alpha: true, desynchronized: true });
  if (!ctx) {
    console.error('Canvas context oluşturulamadı!');
    return;
  }
  console.log('Canvas context oluşturuldu');
  
  // Double buffering için offscreen canvas
  const offscreenCanvas = document.createElement('canvas');
  const offscreenCtx = offscreenCanvas.getContext('2d', { alpha: true, desynchronized: true });
  
  // Canvas boyutunu ayarla
  function resizeCanvas() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
    offscreenCanvas.width = canvas.width;
    offscreenCanvas.height = canvas.height;
    console.log('Canvas boyutu güncellendi:', canvas.width, 'x', canvas.height);
  }
  
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();
  
  // Sprite önbellekleme
  const spriteCache = new Map();

  function loadSprite(src) {
    if (spriteCache.has(src)) {
        return spriteCache.get(src);
    }

    const img = new Image();
    img.src = src;
    spriteCache.set(src, img);
    return img;
  }

  // Kaynakları temizle
  function cleanup() {
    spriteCache.clear();
    offscreenCanvas.width = 0;
    offscreenCanvas.height = 0;
    canvas.width = 0;
    canvas.height = 0;
  }

  // Pencere kapatıldığında kaynakları temizle
  window.addEventListener('beforeunload', cleanup);
  
  let currentCharacter = 'oneko'; // Varsayılan karakter
  let sprite = loadSprite(`assets/${currentCharacter}.gif`);
  console.log('Sprite yükleniyor...');
  
  // Sprite yükleme işlemi
  sprite.onload = function() {
    console.log('Sprite başarıyla yüklendi');
    console.log('Sprite boyutları:', sprite.width, 'x', sprite.height);
    console.log('Sprite kaynağı:', sprite.src);
    
    // Sprite'ı test et
    try {
      ctx.drawImage(sprite, 0, 0, 32, 32, 0, 0, 32, 32);
      console.log('Test çizimi başarılı');
    } catch (error) {
      console.error('Test çizimi hatası:', error);
    }
  };
  
  sprite.onerror = function(error) {
    console.error('Sprite yükleme hatası:', error);
    console.log('Yüklenmeye çalışılan dosya:', sprite.src);
    console.log('Alternatif sprite deneniyor...');
    sprite.src = 'assets/icon.png';
  };
  
  // Sprite'ı yükle
  sprite.src = `assets/${currentCharacter}.gif`;
  console.log('Sprite yükleme isteği gönderildi');

  // Karakter değişimi için IPC dinleyicisi
  ipcRenderer.on('set-character', (event, character) => {
    console.log('Karakter değiştiriliyor:', character);
    currentCharacter = character;
    
    // Önceki sprite'ı temizle
    spriteCache.clear();
    
    // Yeni sprite'ı yükle
    sprite = loadSprite(`assets/${character}.gif`);
    sprite.onload = function() {
      console.log('Yeni karakter yüklendi:', character);
      console.log('Sprite boyutları:', sprite.width, 'x', sprite.height);
    };
    sprite.onerror = function(error) {
      console.error('Yeni karakter yükleme hatası:', error);
      console.log('Alternatif sprite deneniyor...');
      sprite.src = 'assets/icon.png';
    };
    sprite.src = `assets/${character}.gif`;
  });

  // Mevcut karakteri gönder
  ipcRenderer.on('get-current-character', () => {
    console.log('Mevcut karakter gönderiliyor:', currentCharacter);
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
  let sleepStartTime = Date.now(); // Uyku başlangıç zamanı
  const MIN_SLEEP_DURATION = 5000; // Minimum uyku süresi (5 saniye)
  let nekoPosX = 32;
  let nekoPosY = 32;
  let mousePosX = 0;
  let mousePosY = 0;
  let nekoSpeed = 10;
  let nekoSize = 100;
  let currentSprite = 'idle';
  let currentFrame = 0;
  let lastFrameTime = 0;
  const frameDelay = 100;
  let followMouse = true;
  let lastMousePosX = 0;
  let lastMousePosY = 0;
  let suddenMovement = false;
  const EDGE_THRESHOLD = 20; // Duvar tırmalama için kenar mesafesi

  // Animasyon kontrolü için değişkenler
  let frameCount = 0;
  let lastFPSUpdate = 0;

  // FPS kontrolü
  function updateFPS(now) {
    frameCount++;
    if (now - lastFPSUpdate >= 1000) {
      const fps = Math.round((frameCount * 1000) / (now - lastFPSUpdate));
      console.log(`FPS: ${fps}`);
      frameCount = 0;
      lastFPSUpdate = now;
    }
  }

  // Fare hareketini takip et
  document.addEventListener('mousemove', function(event) {
    const oldMousePosX = mousePosX;
    const oldMousePosY = mousePosY;
    mousePosX = event.clientX;
    mousePosY = event.clientY;
    
    // Ani hareket kontrolü
    const dx = mousePosX - oldMousePosX;
    const dy = mousePosY - oldMousePosY;
    const movement = Math.sqrt(dx * dx + dy * dy);
    suddenMovement = movement > 50; // 50 piksel üzeri ani hareket

    // Fare hareketi varsa ve kedi uyuyorsa uyandır
    if (movement > 0 && isSleeping) {
      isSleeping = false;
      isTired = false;
      currentSprite = 'idle';
      console.log('Neko uyandı!');
    }
  });

  function updateNekoPosition() {
    if (!followMouse) return;

    const dx = mousePosX - nekoPosX;
    const dy = mousePosY - nekoPosY;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Ani fare hareketi kontrolü
    if (suddenMovement && !isSleeping && !isScratching) {
      currentSprite = 'alert';
      suddenMovement = false;
      return;
    }

    // Uyku durumunda fare hareketi kontrolü
    if (isSleeping) {
      return; // Fare hareketi mousemove event'inde kontrol ediliyor
    }

    // Fareye yakın olduğunda hareket etmeyi durdur
    if (distance < 5) {
      currentSprite = 'idle';
      return;
    }

    // Duvar tırmalama kontrolü
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

    // Hareket hızını ayarla
    const normalizedDx = dx / distance;
    const normalizedDy = dy / distance;

    // Hızı mesafeye göre ayarla
    const speed = Math.min(nekoSpeed, distance * 0.5);

    nekoPosX += normalizedDx * speed;
    nekoPosY += normalizedDy * speed;
      
    // Diagonal yön belirleme
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
        console.log('Neko uyandı!');
      }
      if (isScratching) {
        isScratching = false;
      }
    }
  }

  // Animasyon fonksiyonu
  function animate(timestamp) {
    // FPS kontrolü
    updateFPS(timestamp);

    // Frame rate kontrolü
    if (timestamp - lastFrameTime < frameDelay) {
      requestAnimationFrame(animate);
      return;
    }
    lastFrameTime = timestamp;

    // Offscreen canvas'ı temizle
    offscreenCtx.clearRect(0, 0, offscreenCanvas.width, offscreenCanvas.height);

    // Kedi pozisyonunu güncelle
    updateNekoPosition();

    const timeSinceLastMove = Date.now() - lastMoveTime;
    const timeSinceLastStateChange = Date.now() - lastStateChange;
    const timeSinceLastScratch = Date.now() - lastScratchTime;

    // Duvar tırmalama süresi kontrolü
    if (isScratching && timeSinceLastScratch > 2000) {
      isScratching = false;
      currentSprite = 'idle';
    }

    // Durum geçişleri
    if (timeSinceLastMove > 3000 && !isSleeping && !isTired && !isScratching) {
      // Rastgele kendini kaşıma davranışı
      if (Math.random() < 0.3) { // %30 ihtimalle
        currentSprite = 'scratchSelf';
        isScratching = true;
        lastScratchTime = Date.now();
      } else {
        currentSprite = 'tired';
        isTired = true;
        lastStateChange = Date.now();
      }
    } else if (isTired && timeSinceLastStateChange > 1000) {
      currentSprite = 'sleeping';
      isSleeping = true;
      isTired = false;
      sleepStartTime = Date.now();
    }

    // Kendini kaşıma süresi kontrolü
    if (isScratching && timeSinceLastScratch > 2000) {
      isScratching = false;
      currentSprite = 'idle';
    }

    // Karakteri offscreen canvas'a çiz
    if (sprite.complete) {
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

    // Offscreen canvas'ı ana canvas'a kopyala
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(offscreenCanvas, 0, 0);

    // Animasyon karelerini güncelle
    currentFrame = (currentFrame + 1) % spriteSets[currentSprite].length;

    requestAnimationFrame(animate);
  }

  // Animasyonu başlat
  requestAnimationFrame(animate);

  // IPC olaylarını dinle
  ipcRenderer.on('set-speed', (event, speed) => {
    nekoSpeed = speed;
    console.log('Hız ayarlandı:', speed);
  });

  ipcRenderer.on('set-size', (event, size) => {
    nekoSize = size;
    console.log('Boyut ayarlandı:', size);
  });

  ipcRenderer.on('set-follow-mouse', (event, enabled) => {
    followMouse = enabled;
    console.log('Fare takibi ayarlandı:', enabled);
  });
})();