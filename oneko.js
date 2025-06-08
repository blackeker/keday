// oneko.js: Canvas tabanlı Oneko
(function onekoCanvas() {
  const { ipcRenderer } = require('electron');
  console.log('Oneko başlatılıyor...');
  
  const canvas = document.getElementById('oneko-canvas');
  if (!canvas) {
    console.error('Canvas elementi bulunamadı!');
    return;
  }
  console.log('Canvas bulundu:', canvas.width, 'x', canvas.height);
  
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    console.error('Canvas context oluşturulamadı!');
    return;
  }
  console.log('Canvas context oluşturuldu');
  
  // Canvas boyutunu ayarla
  function resizeCanvas() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
    console.log('Canvas boyutu güncellendi:', canvas.width, 'x', canvas.height);
  }
  
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();
  
  const sprite = new Image();
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
    sprite.src = './assets/icon.png';
  };
  
  // Sprite'ı yükle
  sprite.src = './assets/oneko.gif';
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
  let frameCount = 0;
  let idleTime = 0;
  let idleAnimation = null;
  let idleAnimationFrame = 0;
  let nekoSpeed = 10;
  let nekoSize = 100;
  let currentSprite = 'idle';
  let currentFrame = 0;
  let lastFrameTime = 0;
  const frameDelay = 100;
  let followMouse = true;

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
    if (distance < 10) {
      currentSprite = 'idle';
      return;
    }

    // Hareket hızını mesafeye göre ayarla
    const speed = Math.min(nekoSpeed, distance / 2);
    nekoPosX += (dx / distance) * speed;
    nekoPosY += (dy / distance) * speed;
      
      // Yön belirleme
      if (Math.abs(dx) > Math.abs(dy)) {
        currentSprite = dx > 0 ? 'E' : 'W';
      } else {
        currentSprite = dy > 0 ? 'S' : 'N';
    }
  }

  function animate() {
    const now = Date.now();
    if (now - lastFrameTime < frameDelay) {
      requestAnimationFrame(animate);
      return;
    }
    lastFrameTime = now;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Kedi pozisyonunu güncelle
    updateNekoPosition();

    // Karakteri çiz
    if (sprite.complete) {
      const spriteFrame = spriteSets[currentSprite][currentFrame];
      if (spriteFrame) {
        const size = 32 * (nekoSize / 100);
        ctx.drawImage(
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

    // Animasyon karelerini güncelle
    currentFrame = (currentFrame + 1) % spriteSets[currentSprite].length;

    requestAnimationFrame(animate);
  }

  // Animasyonu başlat
  animate();

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
