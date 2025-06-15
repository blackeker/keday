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
  
  const sprite = loadSprite('assets/oneko.gif');
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
  sprite.src = 'assets/oneko.gif';
  console.log('Sprite yükleme isteği gönderildi');

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
    mousePosX = event.clientX;
    mousePosY = event.clientY;
  });

  function updateNekoPosition() {
    if (!followMouse) return;

    const dx = mousePosX - nekoPosX;
    const dy = mousePosY - nekoPosY;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Fareye yakın olduğunda hareket etmeyi durdur
    if (distance < 8) {
      currentSprite = 'idle';
      return;
    }

    // Hareket hızını ayarla
    const normalizedDx = dx / distance;
    const normalizedDy = dy / distance;

    // Hızı mesafeye göre ayarla
    const speed = Math.min(nekoSpeed, distance * 0.5);

    nekoPosX += normalizedDx * speed;
    nekoPosY += normalizedDy * speed;
      
    // Yön belirleme
    if (Math.abs(dx) > Math.abs(dy)) {
      currentSprite = dx > 0 ? 'E' : 'W';
    } else {
      currentSprite = dy > 0 ? 'S' : 'N';
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
