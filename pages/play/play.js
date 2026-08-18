// play.js — the shell around the wasm engine.
//
// Owns everything outside the panel: booting the module, and turning taps and keys
// into the button EDGES the engine expects. The wasm draws itself onto #screen and
// asks nothing of this file (src/platform/web/main_web.cpp).
//
// The one contract that matters: every button sends a press AND a release, because
// each button's hold is the bigger version of its tap and a tap/hold settles on the
// release edge. A control that only sent presses would make every hold impossible.
(function () {
  'use strict';

  // Button ordinals, matching mal::Button (platform/platform.h): A=NEXT, B=ACCEPT,
  // C=CANCEL. A+C held together is the Exploit chord, which the engine resolves
  // itself from the two edges — the page never names it.
  var KEYS = {
    KeyZ: 0, ArrowLeft: 0,
    KeyX: 1, ArrowDown: 1, Space: 1,
    KeyC: 2, ArrowRight: 2
  };

  var mod = null;
  var down = [false, false, false];   // per-button, so a repeat cannot double-fire

  function send(btn, pressed) {
    if (!mod || btn < 0 || btn > 2) return;
    if (down[btn] === pressed) return;      // ignore key auto-repeat and re-entry
    down[btn] = pressed;
    mod._mal_button(btn, pressed ? 1 : 0);
    var el = document.querySelector('.key[data-btn="' + btn + '"]');
    if (el) el.classList.toggle('down', pressed);
  }

  // --- on-screen keys ---
  document.querySelectorAll('.key').forEach(function (el) {
    var btn = parseInt(el.dataset.btn, 10);
    // Pointer events cover mouse, touch and pen in one path. Capture keeps the
    // release coming to this element even if the finger slides off it mid-hold —
    // otherwise the button sticks down forever.
    el.addEventListener('pointerdown', function (e) {
      e.preventDefault();
      el.setPointerCapture(e.pointerId);
      send(btn, true);
    });
    var release = function () { send(btn, false); };
    el.addEventListener('pointerup', release);
    el.addEventListener('pointercancel', release);
    // A context menu on long-press would swallow the release edge.
    el.addEventListener('contextmenu', function (e) { e.preventDefault(); });
  });

  // --- keyboard ---
  window.addEventListener('keydown', function (e) {
    var btn = KEYS[e.code];
    if (btn === undefined) return;
    e.preventDefault();      // stop arrows and space from scrolling the page
    send(btn, true);
  });
  window.addEventListener('keyup', function (e) {
    var btn = KEYS[e.code];
    if (btn === undefined) return;
    e.preventDefault();
    send(btn, false);
  });
  // A tab switched away mid-hold never delivers the keyup, so the button would be
  // stuck down on return. Release everything on blur.
  window.addEventListener('blur', function () {
    for (var i = 0; i < 3; i++) send(i, false);
  });

  // --- page controls ---
  document.getElementById('ui-mode').addEventListener('click', function () {
    if (mod) mod._mal_cycle_ui_mode();
  });

  document.getElementById('restart').addEventListener('click', function () {
    if (!mod) return;
    if (!window.confirm('Start over? This wipes the pet you have been playing with.')) return;
    mod._mal_reset_demo();
    window.location.reload();
  });

  // The autosave cadence leaves a few seconds unsaved; flushing on hide is what
  // makes closing the tab cost nothing. `pagehide` fires where `beforeunload` is
  // unreliable on mobile.
  ['pagehide', 'visibilitychange'].forEach(function (evt) {
    window.addEventListener(evt, function () {
      if (mod && document.visibilityState !== 'visible') mod._mal_flush_save();
    });
  });

  // --- boot ---
  var stamp = document.getElementById('stamp');
  Malwarium()
    .then(function (m) {
      mod = m;
      document.getElementById('boot').classList.add('gone');
      stamp.textContent = 'running in this browser';
    })
    .catch(function (err) {
      stamp.textContent = 'failed to load';
      document.getElementById('boot').textContent = 'ENGINE FAILED TO LOAD';
      console.error('malwarium:', err);
    });
})();
