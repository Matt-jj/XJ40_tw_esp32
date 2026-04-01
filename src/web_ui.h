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
    --accent:  #e87722;
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

  /* Offset slider */
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
                  color: var(--text); font-size: 0.9em; margin-top: 4px; }
  .range-dir { display: flex; justify-content: space-between;
               color: var(--text); font-size: 0.9em; margin-top: 4px; font-style: italic; }

  /* Config rows */
  .row { display: flex; justify-content: space-between; align-items: center;
         padding: 8px 0; border-bottom: 1px solid var(--border); }
  .row:last-child { border-bottom: none; }
  .row label { color: var(--text); font-size: 0.9em; }
  .row .sub  { color: var(--muted); font-size: 0.75em; }

  /* Radio button */
  .radio-wrap { display: flex; align-items: center; gap: 8px; cursor: pointer; }
  .radio-wrap input[type=radio] {
    -webkit-appearance: none; appearance: none;
    width: 20px; height: 20px; border-radius: 50%;
    border: 2px solid var(--border); background: var(--bg);
    cursor: pointer; transition: border-color 0.2s, background 0.2s;
    flex-shrink: 0;
  }
  .radio-wrap input[type=radio]:checked {
    border-color: var(--accent); background: var(--accent);
    box-shadow: inset 0 0 0 4px var(--bg);
  }
  .radio-wrap span { color: var(--text); font-size: 0.9em; }

  /* Teeth slider */
  .teeth-display { text-align: center; font-size: 2em; font-weight: bold;
                   color: var(--accent); margin-bottom: 12px; }

  .conn-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted);
              display: inline-block; margin-right: 6px; }
  .conn-dot.live { background: var(--green); }
  .conn-row { display: flex; align-items: center; color: var(--muted); font-size: 0.75em; margin-top: 14px; }
</style>
</head>
<body>

<h1>XJ40 Trigger</h1>
<p class="ver">v)rawhtml" FIRMWARE_VERSION R"rawhtml(</p>

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
         min="-100" max="100" step="5" value="0">
  <div class="range-labels">
    <span>-10°</span><span>0°</span><span>+10°</span>
  </div>
  <div class="range-dir">
    <span>◄ Retard</span><span>Advance ►</span>
  </div>
</div>

<!-- Config card -->
<div class="card">
  <div class="card-title">Configuration</div>
  <div class="row">
    <div class="label" id="switch-label">Remote Switch</div>
    <label class="radio-wrap">
      <input type="radio" id="switch-mode" onclick="toggleSwitch(this)">
      <span>Enabled</span>
    </label>
  </div>
  <div class="row" style="flex-direction: column; align-items: stretch; gap: 4px;">
    <div class="card-title" style="margin-bottom: 4px;">Trigger Wheel Teeth</div>
    <div class="teeth-display" id="teeth-display">36</div>
    <input type="range" id="teeth" min="8" max="60" step="1" value="36"
           oninput="updateTeeth(this)" onchange="sendConfig()">
    <div class="range-labels"><span>8</span><span>Total including missing tooth</span><span>60</span></div>
  </div>
</div>

<script>
let lastOffset = 0;

function fmt(tenths) {
  const v = tenths / 10;
  return (v >= 0 ? '+' : '') + v.toFixed(1) + '\u00b0';
}

// Offset slider
const slider = document.getElementById('offset-slider');
const display = document.getElementById('offset-display');

slider.addEventListener('input', () => {
  display.textContent = fmt(parseInt(slider.value));
});
slider.addEventListener('change', () => {
  sendOffset(parseInt(slider.value));
});

function updateTeeth(el) {
  document.getElementById('teeth-display').textContent = parseInt(el.value);
}

function sendOffset(tenths) {
  fetch('/api/offset?value=' + tenths, { method: 'POST' })
    .catch(() => {});
}

// Remote switch radio — clicking a checked radio re-checks it (no deselect),
// so we handle toggle manually.
function setSwitchColour(checked) {
  document.getElementById('switch-label').style.color = checked ? 'var(--green)' : 'var(--text)';
}

function toggleSwitch(el) {
  if (el.dataset.wasChecked === 'true') {
    el.checked = false;
    el.dataset.wasChecked = 'false';
  } else {
    el.dataset.wasChecked = 'true';
  }
  setSwitchColour(el.checked);
  sendConfig();
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

      if (d.offset_tenths !== lastOffset) {
        lastOffset = d.offset_tenths;
        slider.value = d.offset_tenths;
        display.textContent = fmt(d.offset_tenths);
      }

      const sw = document.getElementById('switch-mode');
      sw.checked = d.switch_mode;
      sw.dataset.wasChecked = d.switch_mode ? 'true' : 'false';
      setSwitchColour(d.switch_mode);

      const teethSlider = document.getElementById('teeth');
      teethSlider.value = d.teeth;
      updateTeeth(teethSlider);

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
