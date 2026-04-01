#pragma once

static const char WEB_UI_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XJ40 Trigger</title>
<style>
  :root {
    --bg:      #0f0f1a;
    --card:    #1a1a2e;
    --accent:  #e94560;
    --text:    #e0e0e0;
    --muted:   #7a7a9a;
    --green:   #2ecc71;
    --red:     #e74c3c;
    --border:  #2a2a4a;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text);
         padding: 16px; max-width: 480px; margin: auto; }
  h1 { color: var(--accent); text-align: center; font-size: 1.5em; padding: 16px 0 4px; }
  .ver { text-align: center; color: var(--muted); font-size: 0.75em; margin-bottom: 20px; }
  .card { background: var(--card); border: 1px solid var(--border); border-radius: 10px;
          padding: 18px; margin-bottom: 14px; }
  .card-title { color: var(--muted); font-size: 0.7em; text-transform: uppercase;
                letter-spacing: 1.5px; margin-bottom: 14px; }

  /* Status row */
  .status-row { display: flex; justify-content: space-between; align-items: center; }
  .rpm-val { font-size: 2.2em; font-weight: bold; color: var(--text); }
  .rpm-unit { color: var(--muted); font-size: 0.85em; margin-left: 4px; }
  .sync-badge { padding: 5px 12px; border-radius: 20px; font-size: 0.8em; font-weight: bold; }
  .sync-ok  { background: rgba(46,204,113,0.15); color: var(--green); border: 1px solid var(--green); }
  .sync-no  { background: rgba(231,76,60,0.15);  color: var(--red);   border: 1px solid var(--red); }

  /* Slider */
  .offset-display { text-align: center; font-size: 2em; font-weight: bold;
                    color: var(--accent); margin-bottom: 12px; }
  input[type=range] {
    -webkit-appearance: none; width: 100%; height: 6px;
    background: var(--border); border-radius: 3px; outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%;
    background: var(--accent); cursor: pointer;
  }
  .range-labels { display: flex; justify-content: space-between;
                  color: var(--muted); font-size: 0.75em; margin-top: 6px; }

  /* Toggle */
  .row { display: flex; justify-content: space-between; align-items: center;
         padding: 8px 0; border-bottom: 1px solid var(--border); }
  .row:last-child { border-bottom: none; }
  .row label { color: var(--text); font-size: 0.9em; }
  .row .sub  { color: var(--muted); font-size: 0.75em; }
  .toggle { position: relative; width: 44px; height: 24px; }
  .toggle input { opacity: 0; width: 0; height: 0; }
  .slider-sw {
    position: absolute; inset: 0; background: var(--border);
    border-radius: 24px; cursor: pointer; transition: background 0.2s;
  }
  .slider-sw::before {
    content: ''; position: absolute; width: 18px; height: 18px;
    left: 3px; top: 3px; background: #fff; border-radius: 50%; transition: transform 0.2s;
  }
  .toggle input:checked + .slider-sw { background: var(--accent); }
  .toggle input:checked + .slider-sw::before { transform: translateX(20px); }

  /* Number input */
  input[type=number] {
    background: var(--bg); border: 1px solid var(--border); color: var(--text);
    border-radius: 6px; padding: 4px 8px; width: 70px; text-align: center; font-size: 0.9em;
  }
  input[type=number]:focus { outline: none; border-color: var(--accent); }

  .conn-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted);
              display: inline-block; margin-right: 6px; }
  .conn-dot.live { background: var(--green); }
  .conn-row { display: flex; align-items: center; color: var(--muted); font-size: 0.75em; margin-top: 14px; }
</style>
</head>
<body>

<h1>XJ40 Trigger</h1>
<p class="ver">v)" FIRMWARE_VERSION R"rawhtml(</p>

<!-- Status card -->
<div class="card">
  <div class="card-title">Status</div>
  <div class="status-row">
    <div>
      <span class="rpm-val" id="rpm">--</span>
      <span class="rpm-unit">RPM</span>
    </div>
    <span class="sync-badge sync-no" id="sync-badge">No sync</span>
  </div>
  <div class="conn-row">
    <span class="conn-dot" id="conn-dot"></span>
    <span id="conn-label">Connecting...</span>
  </div>
</div>

<!-- Timing card -->
<div class="card">
  <div class="card-title">Timing Offset</div>
  <div class="offset-display" id="offset-display">0.0°</div>
  <input type="range" id="offset-slider"
         min="-100" max="100" step="1" value="0">
  <div class="range-labels"><span>-10°</span><span>0°</span><span>+10°</span></div>
</div>

<!-- Config card -->
<div class="card">
  <div class="card-title">Configuration</div>
  <div class="row">
    <div>
      <div class="label">Switch Mode</div>
      <div class="sub">Enable pin controls offset bypass</div>
    </div>
    <label class="toggle">
      <input type="checkbox" id="switch-mode" onchange="sendConfig()">
      <span class="slider-sw"></span>
    </label>
  </div>
  <div class="row">
    <div>
      <div class="label">Teeth Total</div>
      <div class="sub">Including missing</div>
    </div>
    <input type="number" id="teeth" min="2" max="60" value="36"
           onchange="sendConfig()">
  </div>
</div>

<script>
let lastOffset = 0;

function fmt(tenths) {
  const v = tenths / 10;
  return (v >= 0 ? '+' : '') + v.toFixed(1) + '\u00b0';
}

// Slider interaction
const slider = document.getElementById('offset-slider');
const display = document.getElementById('offset-display');

slider.addEventListener('input', () => {
  display.textContent = fmt(parseInt(slider.value));
});
slider.addEventListener('change', () => {
  sendOffset(parseInt(slider.value));
});

function sendOffset(tenths) {
  fetch('/api/offset?value=' + tenths, { method: 'POST' })
    .catch(() => {});
}

function sendConfig() {
  const sm = document.getElementById('switch-mode').checked ? 1 : 0;
  const t  = document.getElementById('teeth').value;
  fetch('/api/config?switch_mode=' + sm + '&teeth=' + t, { method: 'POST' })
    .catch(() => {});
}

// Status polling
function poll() {
  fetch('/api/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('rpm').textContent = d.rpm;

      const badge = document.getElementById('sync-badge');
      if (d.synced) {
        badge.textContent = 'Synced';
        badge.className = 'sync-badge sync-ok';
      } else {
        badge.textContent = 'No sync';
        badge.className = 'sync-badge sync-no';
      }

      // Only update slider if user isn't dragging
      if (d.offset_tenths !== lastOffset) {
        lastOffset = d.offset_tenths;
        slider.value = d.offset_tenths;
        display.textContent = fmt(d.offset_tenths);
      }

      document.getElementById('switch-mode').checked = d.switch_mode;
      document.getElementById('teeth').value = d.teeth;

      document.getElementById('conn-dot').className = 'conn-dot live';
      document.getElementById('conn-label').textContent = 'Live';
    })
    .catch(() => {
      document.getElementById('conn-dot').className = 'conn-dot';
      document.getElementById('conn-label').textContent = 'No connection';
    });
}

poll();
setInterval(poll, 1000);
</script>
</body>
</html>
)rawhtml";
