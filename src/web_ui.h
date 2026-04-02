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

  /* Teeth input */
  #teeth-input:read-only { opacity: 0.6; }

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
  <div style="text-align:right; margin-top:10px;">
    <button id="offset-apply"
            style="padding:6px 20px; background:var(--accent); color:#fff;
                   border:none; border-radius:6px; cursor:pointer; font-size:0.9em;">
      Apply
    </button>
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
  <div class="row" style="flex-direction: column; align-items: flex-start; gap: 10px;">
    <div class="card-title" style="margin-bottom: 0;">Trigger Wheel Teeth</div>
    <div style="display:flex; align-items:center; gap:12px; width:100%;">
      <input type="number" id="teeth-input" min="8" max="60" value="?" readonly
             style="width:70px; font-size:1.8em; font-weight:bold; color:var(--accent);
                    background:var(--bg); border:1px solid var(--border); border-radius:6px;
                    text-align:center; padding:4px;">
      <span style="color:var(--muted); font-size:0.8em;">Total teeth<br>(incl. missing)</span>
      <button id="teeth-apply" onclick="applyTeeth()"
              style="display:none; margin-left:auto; padding:6px 16px;
                     background:var(--accent); color:#fff; border:none;
                     border-radius:6px; cursor:pointer; font-size:0.9em;">Apply</button>
    </div>
    <label style="display:flex; align-items:center; gap:8px; cursor:pointer;
                  color:var(--muted); font-size:0.85em; user-select:none;">
      <input type="checkbox" id="teeth-manual-cb" onchange="teethManualChanged(this)">
      Manual override
    </label>
    <div id="teeth-msg" style="color:var(--red); font-size:0.8em; display:none;">
      Stop engine to edit
    </div>
  </div>
</div>

<script>
let lastOffset = 0;
let offsetPending = false;

function fmt(tenths) {
  const v = tenths / 10;
  return (v >= 0 ? '+' : '') + v.toFixed(1) + '\u00b0';
}

// Offset slider
const slider = document.getElementById('offset-slider');
const display = document.getElementById('offset-display');

slider.addEventListener('input', () => {
  display.textContent = fmt(parseInt(slider.value));
  display.style.color = 'var(--accent)';
  offsetPending = true;
});

document.getElementById('offset-apply').addEventListener('click', () => {
  sendOffset(parseInt(slider.value));
  lastOffset = parseInt(slider.value);
  display.style.color = 'var(--text)';
  offsetPending = false;
});

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
  fetch('/api/config?switch_mode=' + sm, { method: 'POST' })
    .catch(() => {});
}

let teethManualLocal = false;

function updateTeethUI(d) {
  const manualCb   = document.getElementById('teeth-manual-cb');
  const teethInput = document.getElementById('teeth-input');
  const applyBtn   = document.getElementById('teeth-apply');
  const teethMsg   = document.getElementById('teeth-msg');

  if (!manualCb._pending) {
    manualCb.checked = d.teeth_manual;
    teethManualLocal = d.teeth_manual;
  }

  if (!teethManualLocal) {
    teethInput.value    = d.teeth_auto > 0 ? d.teeth_auto : '?';
    teethInput.readOnly = true;
    applyBtn.style.display = 'none';
    teethMsg.style.display  = 'none';
  } else if (d.rpm > 0) {
    teethInput.value    = d.teeth;
    teethInput.readOnly = true;
    applyBtn.style.display = 'none';
    teethMsg.style.display  = 'block';
  } else {
    if (teethInput.readOnly) teethInput.value = d.teeth;  // seed on first edit
    teethInput.readOnly = false;
    applyBtn.style.display = 'inline';
    teethMsg.style.display  = 'none';
  }
}

function teethManualChanged(el) {
  el._pending = true;
  teethManualLocal = el.checked;
  fetch('/api/config?teeth_manual=' + (el.checked ? 1 : 0), { method: 'POST' })
    .then(() => { el._pending = false; })
    .catch(() => { el._pending = false; });
}

function applyTeeth() {
  const val = parseInt(document.getElementById('teeth-input').value);
  if (val >= 8 && val <= 60) {
    fetch('/api/config?teeth=' + val + '&teeth_manual=1', { method: 'POST' })
      .catch(() => {});
  }
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

      if (!offsetPending && d.offset_tenths !== lastOffset) {
        lastOffset = d.offset_tenths;
        slider.value = d.offset_tenths;
        display.textContent = fmt(d.offset_tenths);
        display.style.color = 'var(--text)';
      }

      const sw = document.getElementById('switch-mode');
      sw.checked = d.switch_mode;
      sw.dataset.wasChecked = d.switch_mode ? 'true' : 'false';
      setSwitchColour(d.switch_mode);

      updateTeethUI(d);

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
