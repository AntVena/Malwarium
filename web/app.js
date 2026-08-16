/* ============================================================
   MALWARIUM://PEDIA — app.js
   Vanilla JS, no framework. Hydrates from:
     1. live  GET /pedia_state.json   (served by the ESP32)
     2. else  window.PEDIA_STATE_FIXTURE (fixtures/pedia_state.js)
   Content pack: window.PEDIA_DATA (data/pedia_data.js, generated).
   Hash routing so the ESP32 serves exactly one HTML file.
   ============================================================ */
(function () {
  'use strict';

  var D = window.PEDIA_DATA;
  var S = null; // state, resolved async

  /* ---------- state helpers ---------- */
  function st(map, id) { return (S && S[map] && S[map][id]) || 'locked'; }
  function petSt(id) { return st('pets', id); }

  function revealedCount() {
    var n = 0, total = D.creatures.length + D.malbeasts.length;
    D.creatures.forEach(function (c) { if (petSt(c.id) === 'hatched') n++; });
    D.malbeasts.forEach(function (b) { if (st('malbeasts', b.id) === 'defeated') n++; });
    return { n: n, total: total };
  }

  /* ---------- tiny DOM helpers ---------- */
  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }
  function mask(name) { return '\u2593'.repeat(Math.max(6, Math.min(10, name.length))); }

  /* A sprite's box + background rule, from the geometry gen_pedia_data.py put on
     the entry: a sheet is a GRID of cellW x cellH frames, so the box crops to one
     cell (frame 0 = idle_a) while the background scales the WHOLE sheet. Sizing the
     background by the cell instead squashes every frame into that one box. */
  function sprStyle(s, scale) {
    var cw = (s.cellW || 56) * scale, ch = (s.cellH || 48) * scale;
    return {
      box: 'width:' + cw + 'px;height:' + ch + 'px;',
      bg: 'background-image:url(\'' + s.sprite + '\');background-size:' +
          ((s.sheetW || s.cellW || 56) * scale) + 'px ' +
          ((s.sheetH || s.cellH || 48) * scale) + 'px;background-position:0 0;'
    };
  }

  /* Sprite as a scaled background div, cropped to frame 0. */
  function spr(s, scale, cls) {
    var g = sprStyle(s, scale);
    return '<div class="spr ' + (cls || '') + '" style="' + g.box + g.bg + '"></div>';
  }

  /* Glitch silhouette: two stacked tinted copies, low-fps jitter (frag ramp). */
  function glitchSpr(s, scale) {
    var g = sprStyle(s, scale), bg = g.bg + 'background-repeat:no-repeat;';
    return '<div class="glitchbox" style="' + g.box + 'flex:none;">' +
      '<div class="layer a spr glitch" style="' + bg + '"></div>' +
      '<div class="layer b spr glitch-hi" style="' + bg + '"></div></div>';
  }

  function chip(state) {
    var map = {
      hatched:   ['hatched', '\u25a0 HATCHED'],
      unlocked:  ['owned', '\u25a0 UNLOCKED'],
      owned:     ['owned', '\u25a0 OWNED'],
      known:     ['owned', '\u25a0 KNOWN'],
      unknown:   ['locked', '\u26bf METHOD LOCKED'],
      seen:      ['seen', '\u2593 SEEN'],
      defeated:  ['hatched', '\u25a0 DEFEATED'],
      locked:    ['locked', '\u26bf LOCKED'],
      complete:  ['complete', '\u25a0 COMPLETE'],
      incomplete:['incomplete', '\u2026 INCOMPLETE'],
      crashed:   ['crashed', '\u2715 CRASHED'],
      retired:   ['retired', '\u25a0 RETIRED'],
      enc:       ['enc', '\u25cd ENCOUNTERED']
    };
    var m = map[state] || map.locked;
    return '<span class="chip ' + m[0] + '">' + m[1] + '</span>';
  }
  function rar(r) { return '<span class="rar ' + r + '">' + r + '</span>'; }

  /* ---------- views ---------- */

  function vLanding() {
    var pet = S.active_pet || {};
    var c = D.creatures.find(function (x) { return x.id === pet.species; });
    var rc = revealedCount();
    var h = '';

    h += '<h2 class="sect">// ACTIVE PROCESS</h2>';
    if (c) {
      h += '<a class="card" href="#/entry/' + c.id + '">' +
        spr(c, 2) +
        '<div class="body"><div class="name">' + esc(c.name.toUpperCase()) + '</div>' +
        '<div class="meta"><span class="lit">' + c.stageName.toUpperCase() + '</span>' +
        (c.line ? ' \u00b7 ' + esc(D.meta.lines[c.line].toUpperCase()) + ' LINE' : '') + '</div>' +
        '<div class="ctx">uptime ' + esc(pet.uptime || '\u2014') + ' \u00b7 frag ' + (pet.frag_pct || 0) + '% \u00b7 mistakes ' + (pet.mistakes || 0) + '</div>' +
        '</div></a>';
    } else {
      h += '<div class="card locked"><div class="body"><div class="name masked">NO PROCESS RUNNING</div>' +
        '<div class="ctx">hatch an egg, then refresh</div></div></div>';
    }

    h += '<h2 class="sect">// DECRYPTION PROGRESS</h2>';
    h += '<div class="card"><div class="body"><div class="bar">' +
      '<div class="rail"><div class="fill" style="width:' + Math.round(100 * rc.n / rc.total) + '%"></div></div>' +
      '<span class="num">' + rc.n + '/' + rc.total + '</span><span class="lbl">FILES DECRYPTED</span>' +
      '</div></div></div>';

    h += '<h2 class="sect">// SECTIONS</h2>';
    [['#/index', 'CREATURE INDEX', 'petware + wild malbeasts, three reveal states'],
     ['#/items', 'ITEMS + MODS', 'consumables, keys, caches, hardware mods'],
     ['#/food', 'FOOD + RECIPES', 'the pantry, the dishes, and the methods that cook them'],
     ['#/moves', 'MOVES', 'core kit + line signatures'],
     ['#/ach', 'ACHIEVEMENTS', 'the permanent record'],
     ['#/archive', 'ARCHIVE', 'past pets \u00b7 retired and crashed'],
     ['#/tag', 'DEVICE SETTINGS', 'hackertag \u00b7 home network \u00b7 update source']
    ].forEach(function (r) {
      h += '<a class="card" href="' + r[0] + '"><div class="body">' +
        '<div class="name">' + r[1] + '</div><div class="ctx">' + r[2] + '</div></div>' +
        '<span style="color:var(--ink-dim)">\u25b8</span></a>';
    });
    return h;
  }

  function creatureCell(c) {
    var state = petSt(c.id);
    var inner, nm, stc;
    if (state === 'hatched') {
      inner = spr(c, 1);
      nm = '<span class="nm">' + esc(c.name.toUpperCase()) + '</span>';
      stc = '<span class="st" style="color:var(--calm)">\u25a0 ' + c.stageName.toUpperCase() + '</span>';
    } else if (state === 'seen') {
      inner = glitchSpr(c, 1);
      nm = '<span class="nm masked">' + mask(c.name) + '</span>';
      stc = '<span class="st" style="color:var(--frag-hi)">\u2593 ' + c.stageName.toUpperCase() + '</span>';
    } else {
      inner = '<div style="width:56px;height:48px;display:flex;align-items:center;justify-content:center">' +
        '<img src="' + D.meta.lockIcon + '" alt="" style="width:24px;height:24px;image-rendering:pixelated;opacity:.4"></div>';
      nm = '<span class="nm masked">\u2014\u2014\u2014\u2014</span>';
      stc = '<span class="st" style="color:var(--ink-dim)">\u26bf ENCRYPTED</span>';
    }
    var href = state === 'locked' ? '' : ' href="#/entry/' + c.id + '"';
    var tagEl = state === 'locked' ? 'div' : 'a';
    return '<' + tagEl + ' class="cell ' + state + '"' + href + '>' + inner + nm + stc + '</' + tagEl + '>';
  }

  function beastCell(b) {
    var state = st('malbeasts', b.id);
    var inner, nm, stc;
    if (state === 'defeated') {
      inner = spr(b, 1);
      nm = '<span class="nm">' + esc(b.name.toUpperCase()) + '</span>';
      stc = '<span class="st" style="color:var(--calm)">\u25a0 DEFEATED</span>';
    } else if (state === 'seen') {
      inner = spr(b, 1, 'dimmed');
      nm = '<span class="nm">' + esc(b.name.toUpperCase()) + '</span>';
      stc = '<span class="st" style="color:var(--warn)">\u25cd ENCOUNTERED</span>';
    } else {
      inner = '<div style="width:56px;height:48px;display:flex;align-items:center;justify-content:center">' +
        '<img src="' + D.meta.lockIcon + '" alt="" style="width:24px;height:24px;image-rendering:pixelated;opacity:.4"></div>';
      nm = '<span class="nm masked">\u2014\u2014\u2014\u2014</span>';
      stc = '<span class="st" style="color:var(--ink-dim)">\u26bf UNMET</span>';
    }
    var href = state === 'locked' ? '' : ' href="#/beast/' + b.id + '"';
    var tagEl = state === 'locked' ? 'div' : 'a';
    return '<' + tagEl + ' class="cell ' + state + '"' + href + '>' + inner + nm + stc + '</' + tagEl + '>';
  }

  function vIndex() {
    var h = '';
    Object.keys(D.meta.lines).forEach(function (lineId) {
      var roster = D.creatures.filter(function (c) { return c.line === lineId; })
        .sort(function (a, b) { return a.stage - b.stage; });
      if (!roster.length) return;
      var got = roster.filter(function (c) { return petSt(c.id) === 'hatched'; }).length;
      var icon = D.meta.lineIcons[lineId];
      h += '<h2 class="sect">' + (icon ? '<img src="' + icon + '" alt="">' : '') + '// ' +
        esc(D.meta.lines[lineId].toUpperCase()) + ' LINE<span class="count">' + got + '/' + roster.length + '</span></h2>';
      h += '<div class="grid">' + roster.map(creatureCell).join('') + honeytokenCell(lineId) + '</div>';
    });

    var strays = D.creatures.filter(function (c) { return !c.line; });
    if (strays.length) {
      h += '<h2 class="sect">// UNAFFILIATED PROCESSES</h2>';
      h += '<div class="grid">' + strays.map(creatureCell).join('') + '</div>';
    }

    h += '<h2 class="sect">// WILD MALBEASTS<span class="count">' +
      D.malbeasts.filter(function (b) { return st('malbeasts', b.id) === 'defeated'; }).length +
      '/' + D.malbeasts.length + '</span></h2>';
    h += '<div class="note">seen on encounter \u00b7 full record on first win</div>';
    h += '<div class="grid">' + D.malbeasts.map(beastCell).join('') + '</div>';

    h += '<h2 class="sect">// WORM LINE<span class="count">0/?</span></h2>';
    h += '<div class="note">line not yet detected on this device \u00b7 keep exploring the \u2019net</div>';
    return h;
  }

  /* The DEVTOOLS_INTRUDER honeytoken: a fake locked cell, hidden inline.
     Un-hiding it in DevTools (or deleting the style/class) trips the observer. */
  function honeytokenCell(lineId) {
    if (lineId !== 'ransomware') return '';
    return '<!-- entry 0x7F: nothing to see here. definitely not a hidden file. -->' +
      '<div class="cell locked" id="honeytoken" data-entry="zero_day" style="display:none">' +
      '<div style="width:56px;height:48px;display:flex;align-items:center;justify-content:center">' +
      '<img src="' + D.meta.lockIcon + '" alt="" style="width:24px;height:24px;image-rendering:pixelated;opacity:.4"></div>' +
      '<span class="nm masked">0X7F</span>' +
      '<span class="st" style="color:var(--ink-dim)">\u26bf SEALED</span></div>';
  }

  function armHoneytoken() {
    var el = document.getElementById('honeytoken');
    if (!el) return;
    var tripped = false;
    function trip() {
      if (tripped) return;
      tripped = true;
      el.classList.add('honeypot-reveal');
      el.style.display = '';
      el.innerHTML =
        '<div style="width:56px;height:48px;display:flex;align-items:center;justify-content:center;font-size:24px;color:var(--calm)">\u2691</div>' +
        '<span class="nm" style="font-size:10px">DEVTOOLS_INTRUDER</span>' +
        '<span class="st" style="color:var(--calm)">\u25a0 ACH UNLOCKED</span>';
      var note = document.createElement('div');
      note.className = 'note';
      note.style.color = 'var(--calm)';
      note.textContent = '>> well well well. an inspector. the \u2019Pedia respects curiosity \u2014 achievement logged.';
      el.parentNode.insertAdjacentElement('afterend', note);
      if (S && S.achievements) S.achievements.DEVTOOLS_INTRUDER = 'complete';
      // callback to the device; harmless no-op on fixture/disk
      try { fetch('/api/achievement/DEVTOOLS_INTRUDER', { method: 'POST' }).catch(function () {}); } catch (e) {}
    }
    var mo = new MutationObserver(function () {
      var vis = el.style.display !== 'none' && getComputedStyle(el).display !== 'none';
      if (vis) trip();
    });
    mo.observe(el, { attributes: true, attributeFilter: ['style', 'class'] });
  }

  function evoNode(id, hereId) {
    var c = D.creatures.find(function (x) { return x.id === id; });
    if (!c) return '';
    var state = petSt(c.id);
    var here = id === hereId ? ' here' : '';
    if (state === 'hatched') {
      return '<a class="node' + here + '" href="#/entry/' + c.id + '">' + spr(c, 1) +
        '<span>' + esc(c.name.toUpperCase()) + '</span></a>';
    }
    if (state === 'seen') {
      return '<a class="node unknown' + here + '" href="#/entry/' + c.id + '">' + glitchSpr(c, 1) +
        '<span>' + mask(c.name) + '</span></a>';
    }
    return '<span class="node unknown' + here + '"><span style="font-size:22px;line-height:48px">?</span><span>ENCRYPTED</span></span>';
  }

  function vEntry(id) {
    var c = D.creatures.find(function (x) { return x.id === id; });
    if (!c) return vMissing();
    var state = petSt(c.id);
    if (state === 'locked') return vMissing();
    var hatched = state === 'hatched';
    var h = '<div class="backlink"><a href="#/index">\u25c2 INDEX</a></div>';

    h += '<div class="entry-hero">' +
      (hatched ? spr(c, 3) : glitchSpr(c, 3)) +
      '<div class="entry-title"><div class="nm' + (hatched ? '' : ' masked') + '">' +
      (hatched ? esc(c.name.toUpperCase()) : mask(c.name)) + '</div></div>' +
      chip(state) +
      '<div class="stagepos">STAGE ' + c.stage + ' \u00b7 ' + c.stageName.toUpperCase() +
      (hatched && c.line ? ' \u2014 ' + esc(D.meta.lines[c.line].toUpperCase()) + ' LINE' : ' \u2014 LINE \u2593\u2593\u2593\u2593\u2593') +
      (c.branch ? ' \u00b7 ' + c.branch.toUpperCase() + ' BRANCH' : '') + '</div></div>';

    h += '<div class="card"><div class="body"><div class="hint">' + esc(c.hint) + '</div>' +
      '<div class="ctx">CONTEXT: ' + (hatched ? esc(c.context) : '\u2593\u2593\u2593\u2593\u2593\u2593 \u2593\u2593\u2593\u2593 \u2593\u2593\u2593') + '</div></div></div>';

    if (hatched) {
      var prev = D.creatures.filter(function (x) { return (x.evolvesTo || []).indexOf(c.id) >= 0; });
      var next = c.evolvesTo || [];
      if (prev.length || next.length) {
        h += '<h2 class="sect">// EVOLUTION PATH</h2><div class="evo">';
        prev.forEach(function (p) { h += evoNode(p.id, c.id) + '<span class="arrow">\u25b8</span>'; });
        h += evoNode(c.id, c.id);
        if (next.length === 1) h += '<span class="arrow">\u25b8</span>' + evoNode(next[0], c.id);
        else if (next.length > 1) {
          h += '<span class="arrow">\u25b8</span><div class="branch">' +
            next.map(function (n) { return evoNode(n, c.id); }).join('') + '</div>';
        }
        h += '</div>';
        if (c.branchSplit) h += '<div class="note" style="text-align:center">branch decided by the care-mistake budget \u00b7 0\u20132 good \u00b7 3\u20134 bad</div>';
      }
    }
    return h;
  }

  function vBeast(id) {
    var b = D.malbeasts.find(function (x) { return x.id === id; });
    if (!b) return vMissing();
    var state = st('malbeasts', b.id);
    if (state === 'locked') return vMissing();
    var full = state === 'defeated';
    var h = '<div class="backlink"><a href="#/index">\u25c2 INDEX</a></div>';
    h += '<div class="entry-hero">' +
      spr(b, 3, full ? '' : 'dimmed') +
      '<div class="entry-title"><div class="nm">' + esc(b.name.toUpperCase()) + '</div></div>' +
      chip(full ? 'defeated' : 'enc') +
      '<div class="stagepos">WILD MALBEAST \u00b7 TIER ' + b.tier + ' \u00b7 ' + esc(b.sector.toUpperCase()) + '</div></div>';
    if (full) {
      h += '<div class="card"><div class="body">' +
        '<div class="hint">Threat profile decrypted after first takedown.</div>' +
        '<div class="ctx">HP ' + b.hp + ' \u00b7 POW ' + b.pow + ' \u00b7 KNOWN MOVES: ' +
        b.moves.map(function (m) {
          var mv = D.moves.find(function (x) { return x.id === m; });
          return esc((mv ? mv.name : m).toUpperCase());
        }).join(' \u00b7 ') + '</div></div></div>';
    } else {
      h += '<div class="card"><div class="body">' +
        '<div class="hint">Encountered in the wild. Log a win to decrypt its full threat profile.</div>' +
        '<div class="ctx">HP \u2593\u2593 \u00b7 POW \u2593\u2593 \u00b7 MOVES \u2593\u2593\u2593\u2593</div></div></div>';
    }
    return h;
  }

  // The row's derived stat line (generated from the firmware's own magnitudes — see
  // core/content/effect_text.h). Rendered as its own line under the prose so a
  // magnitude is visible even when the description doesn't mention it. Withheld on a
  // locked row for the same reason the effect text is: it IS the spec.
  function stats(s) {
    return s ? '<div class="fxnum">' + esc(s) + '</div>' : '';
  }

  function itemRow(it) {
    var state = st('items', it.id);
    var un = state === 'unlocked';
    return '<div class="row ' + (un ? '' : 'locked') + '">' +
      '<img class="icon20 ' + (un ? '' : 'dim') + '" src="' + (un ? it.icon : D.meta.lockIcon) + '" alt="">' +
      '<div class="body"><div class="nm">' + (un ? esc(it.name.toUpperCase()) : mask(it.name)) +
      rar(it.rarity) + (it.use ? '<span class="chip enc">' + it.use + '</span>' : '') + '</div>' +
      '<div class="fx">' + (un ? esc(it.effect) : 'effect encrypted \u2014 find or buy one first') + '</div>' +
      (un ? stats(it.stats) : '') + '</div>' +
      (un && it.bits ? '<span class="bits">' + it.bits + ' BITS</span>' : '') +
      '</div>';
  }

  /* FOOD is not here — it has its own tab (vFood). A dish is the one item type with a
     second gate on it (the method that cooks it), and at 192 of 216 rows the pantry
     was also most of this page: leaving it in buried the two dozen items that AREN'T
     food under nine screens of groceries. */
  function vItems() {
    var h = '';
    var groups = [['BUFFS', 'BUFFS'], ['QUEST', 'QUEST']];
    groups.forEach(function (g) {
      var list = D.items.filter(function (i) { return i.type === g[0]; });
      if (!list.length) return;
      var got = list.filter(function (i) { return st('items', i.id) === 'unlocked'; }).length;
      h += '<h2 class="sect">// ' + g[1] + '<span class="count">' + got + '/' + list.length + '</span></h2>';
      h += list.map(itemRow).join('');
    });

    h += '<h2 class="sect">// HARDWARE MODS<span class="count">' +
      D.mods.filter(function (m) { return st('mods', m.id) === 'unlocked'; }).length + '/' + D.mods.length + '</span></h2>';
    /* Tiers walked off the DATA, never a literal list \u2014 a literal one silently drops
       every mod at a tier it forgets to name, and the ladder grows with the area list.
       Sorted numerically because the keys arrive as strings. */
    Object.keys(D.modTiers).map(Number).sort(function (a, b) { return a - b; }).forEach(function (t) {
      var list = D.mods.filter(function (m) { return m.tier === t; });
      if (!list.length) return;
      h += '<div class="note">TIER ' + t + ' \u00b7 ' + esc(D.modTiers[t].toUpperCase()) + '</div>';
      h += list.sort(function (a, b) { return a.equipLevel - b.equipLevel; }).map(function (m) {
        var un = st('mods', m.id) === 'unlocked';
        return '<div class="row ' + (un ? '' : 'locked') + '">' +
          '<img class="icon20 ' + (un ? '' : 'dim') + '" src="' + (un ? m.icon : D.meta.lockIcon) + '" alt="">' +
          '<div class="body"><div class="nm">' + (un ? esc(m.name.toUpperCase()) : mask(m.name)) +
          rar(m.rarity) + '<span class="chip enc">' + esc(m.tag) + '</span>' +
          /* The equip gate is not a spoiler \u2014 it is what tells a player whether a mod
             they just found is usable, so it shows on a locked row too. */
          '<span class="chip">L' + m.equipLevel + '</span>' +
          (m.oneShot ? '<span class="chip crashed">1-USE</span>' : '') + '</div>' +
          '<div class="fx">' + (un ? esc(m.effect) : 'spec encrypted \u2014 drops in ' + esc(D.modTiers[m.tier])) + '</div>' +
          (un ? stats(m.stats) : '') + '</div></div>';
      }).join('');
    });
    return h;
  }

  /* ---------- the kitchen ---------- */

  /* An item row by id, for the ingredient and dish lookups below. Built once, lazily:
     the recipe table names ~400 ingredients across its rows, and a linear scan of 216
     items per name is the kind of thing a phone browser notices. */
  var _itemIx = null;
  function itemById(id) {
    if (!_itemIx) {
      _itemIx = {};
      D.items.forEach(function (i) { _itemIx[i.id] = i; });
    }
    return _itemIx[id];
  }

  function itemName(id) {
    var it = itemById(id);
    return esc((it ? it.name : id).toUpperCase());
  }

  /* One MERGE HUB method. The kitchen gates on two INDEPENDENT axes and this row shows
     both, because which one a player is waiting on is the whole question:

       MET    — the dish has been held at least once (items{}), which decrypts what it
                DOES. A dish can be met without its method: several are sold on shelves.
       KNOWN  — the method itself was won off a Decryptogram's prize ladder (recipes{}),
                which decrypts what it TAKES. Methods are never bought at any price.

     The dish NAME survives both, matching the device's own hub (game_merge.cpp draws
     every recipe's title and tags the unwon ones LOCKED): you are meant to be able to
     see what you're cooking toward. */
  function recipeRow(r) {
    var known = st('recipes', r.output) === 'known';
    var met = st('items', r.output) === 'unlocked';
    var dish = itemById(r.output);

    var ingredients = r.inputs.map(function (i) {
      return i.qty + '× ' + itemName(i.id);
    }).join(' · ');

    /* The met-the-dish gate (requiresItems): the ladder holds these rows back until
       you've actually eaten the thing, so a locked row says which dish it's waiting
       on rather than looking like bad luck. Named only while it's still unmet. */
    var waiting = (r.requires || []).filter(function (id) {
      return st('items', id) !== 'unlocked';
    });

    var method = known
      ? '<div class="method">' + ingredients + '</div>'
      : '<div class="method">method encrypted — won off a DECRYPTOGRAM, never sold' +
        (waiting.length
          ? ' · held back until you’ve met ' +
            waiting.map(itemName).join(' + ')
          : '') + '</div>';

    return '<div class="row ' + (known ? '' : 'locked') + '">' +
      '<img class="icon20 ' + (met ? '' : 'dim') + '" src="' +
      (met && dish ? dish.icon : D.meta.lockIcon) + '" alt="">' +
      '<div class="body"><div class="nm">' + itemName(r.output) +
      (dish ? rar(dish.rarity) : '') + chip(known ? 'known' : 'unknown') + '</div>' +
      '<div class="fx">' + (met && dish ? esc(dish.effect)
        : 'dish encrypted — get hold of one to read what it does') + '</div>' +
      (met && dish ? stats(dish.stats) : '') + method + '</div>' +
      (met && dish && dish.bits ? '<span class="bits">' + dish.bits + ' BITS</span>' : '') +
      '</div>';
  }

  /* PANTRY + RECIPES. Split by whether a recipe MAKES the food: a staple is a row you
     find, buy or forage, a dish is a row you cook — and the dish's own entry is its
     recipe, so listing it twice would say the same thing in the weaker place. */
  function vFood() {
    var cooked = {};
    D.recipes.forEach(function (r) { cooked[r.output] = true; });
    var foods = D.items.filter(function (i) { return i.type === 'FOOD'; });
    var pantry = foods.filter(function (i) { return !cooked[i.id]; });

    var metFoods = foods.filter(function (i) { return st('items', i.id) === 'unlocked'; }).length;
    var knownRecipes = D.recipes.filter(function (r) {
      return st('recipes', r.output) === 'known';
    }).length;

    /* Both ladders up front, because both are achievement rungs the device gives no
       other view of: the cuisine ladder counts foods ever held, the RecipesKnown ladder
       counts methods won. Dual-coded (fill + fraction) like the achievement bars. */
    var h = '<h2 class="sect">// THE KITCHEN</h2>';
    h += '<div class="card"><div class="body">' +
      '<div class="bar"><div class="rail"><div class="fill" style="width:' +
      (100 * metFoods / (foods.length || 1)) + '%"></div></div>' +
      '<span class="num">' + metFoods + '/' + foods.length + '</span>' +
      '<span class="lbl">FOODS MET</span></div>' +
      '<div class="bar"><div class="rail"><div class="fill" style="width:' +
      (100 * knownRecipes / (D.recipes.length || 1)) + '%"></div></div>' +
      '<span class="num">' + knownRecipes + '/' + D.recipes.length + '</span>' +
      '<span class="lbl">METHODS KNOWN</span></div>' +
      '</div></div>';

    var gotPantry = pantry.filter(function (i) { return st('items', i.id) === 'unlocked'; }).length;
    h += '<h2 class="sect">// PANTRY<span class="count">' + gotPantry + '/' + pantry.length +
      '</span></h2>';
    h += '<div class="note">raw staples · found, foraged or bought — edible as-is, ' +
      'better cooked</div>';
    h += pantry.map(itemRow).join('');

    h += '<h2 class="sect">// RECIPES<span class="count">' + knownRecipes + '/' +
      D.recipes.length + '</span></h2>';
    h += '<div class="note">cooked at the MERGE HUB · buy the hub in the RIG SHOP, ' +
      'then win the methods one solved DECRYPTOGRAM at a time</div>';
    h += D.recipes.map(recipeRow).join('');
    return h;
  }

  function vMoves() {
    var h = '';
    Object.keys(D.moveGroups).forEach(function (g) {
      var list = D.moves.filter(function (m) { return m.group === g; });
      if (!list.length) return;
      var got = list.filter(function (m) { return st('moves', m.id) === 'owned'; }).length;
      h += '<h2 class="sect">// ' + esc(D.moveGroups[g]) + '<span class="count">' + got + '/' + list.length + '</span></h2>';
      h += list.map(function (m) {
        var owned = st('moves', m.id) === 'owned';
        return '<div class="row ' + (owned ? '' : 'locked') + '">' +
          '<img class="icon20 ' + (owned ? '' : 'dim') + '" src="' + m.icon + '" alt="">' +
          '<div class="body"><div class="nm">' + (owned ? esc(m.name.toUpperCase()) : mask(m.name)) +
          '<span class="chip ' + (m.kind === 'ATK' ? 'crashed' : 'owned') + '">' + m.kind + '</span>' +
          (m.innate ? '<span class="chip enc">INNATE</span>' : '') + '</div>' +
          '<div class="fx">' + (owned ? esc(m.desc) : 'technique encrypted \u2014 learn it in MOVES') + '</div>' +
          (owned ? stats(m.stats) : '') + '</div>' +
          '<div class="stat"><b>' + m.power + '</b> PWR<br><b>' + m.turns + '</b>T \u00b7 ' + esc(m.minStage.toUpperCase()) + '+</div>' +
          '</div>';
      }).join('');
    });
    return h;
  }

  /* How far along one unfinished ladder is, as a rail + "value/goal".
     The device publishes achievement_progress only for the COUNTABLE rows
     (pedia_state.cpp) \u2014 a one-off moment has nothing meaningful between 0 and
     done \u2014 so a row with no entry simply gets no bar. Nothing is drawn for a
     completed row either: the COMPLETE chip already says it, and "60/60" under
     it is noise. Dual-coded on purpose, matching the device's rule: the fill
     is the glance, the fraction is the answer, so the bar survives being read
     in grayscale or by someone who can't separate the fill from the rail. */
  function achBar(key, state) {
    var p = S && S.achievement_progress && S.achievement_progress[key];
    if (!p || state === 'complete') return '';
    var goal = p.goal || 0, val = Math.max(0, Math.min(p.value || 0, goal));
    if (goal <= 0) return '';
    return '<div class="bar achbar">' +
      '<div class="rail"><div class="fill" style="width:' +
      (goal ? (val / goal) * 100 : 0) + '%"></div></div>' +
      '<div class="num">' + val + '/' + goal + '</div></div>';
  }

  function vAch() {
    var done = D.achievements.filter(function (a) { return st('achievements', a.key) === 'complete'; }).length;
    var h = '<h2 class="sect">// PERMANENT RECORD<span class="count">' + done + '/' + D.achievements.length + '</span></h2>';
    h += D.achievements.map(function (a) {
      var state = st('achievements', a.key) === 'complete' ? 'complete' : 'incomplete';
      var hidden = a.hidden && state !== 'complete';
      return '<div class="ach ' + state + '">' +
        '<div class="frame"><img src="' + a.icon + '" alt=""></div>' +
        '<div class="body"><div class="nm">' + (hidden ? '????????' : esc(a.name.toUpperCase())) + '</div>' +
        '<div class="tr">' + (hidden ? 'this one finds YOU. or\u2026 you find it. (F12)' : esc(a.trigger)) + '</div>' +
        (hidden ? '' : achBar(a.key, state)) + '</div>' +
        chip(state) + '</div>';
    }).join('');
    return h;
  }

  function vArchive() {
    var recs = (S && S.archive) || [];
    var h = '<h2 class="sect">// THE ARCHIVE<span class="count">' + recs.length + ' RECORDS</span></h2>';
    if (!recs.length) h += '<div class="note">no past pets on file \u2014 this generation is still running</div>';
    h += recs.map(function (r) {
      var c = D.creatures.find(function (x) { return x.id === r.species; });
      var crashed = r.cause === 'crashed';
      return '<div class="card rec ' + (crashed ? 'crashed' : '') + '">' +
        (c ? spr(c, 2) : '') +
        '<div class="body"><div class="name">' + esc(r.name) + chip(crashed ? 'crashed' : 'retired') + '</div>' +
        '<div class="meta">' + (c ? esc(c.name.toUpperCase()) + ' \u00b7 ' : '') + 'FINAL STAGE: ' + esc(r.stage) + '</div>' +
        '<div class="ctx">' + esc(r.record) + '</div>' +
        (crashed ? '<div class="ctx" style="color:var(--hot)">CAUSE: CRITICAL SYSTEM FAILURE</div>'
                 : '<div class="ctx" style="color:var(--calm)">CAUSE: HONORABLE RETIREMENT</div>') +
        '</div></div>';
    }).join('');
    return h;
  }

  function vTag() {
    var h = '<h2 class="sect">// HACKERTAG</h2>';
    h += '<div class="term">' +
      '<div class="line"><span class="prompt">pet@malwarium</span><span class="path">:~$</span> passwd --tag</div>' +
      '<form class="tagform" id="tagform">' +
      '<input id="taginput" type="text" maxlength="12" autocomplete="off" spellcheck="false" ' +
      'placeholder="' + esc((S && S.hacker_tag) || 'NEW_TAG') + '" aria-label="New HackerTag">' +
      '<button type="submit">WRITE</button></form>' +
      '<div class="formmsg" id="tagmsg"></div>' +
      '<div class="charset">charset A\u2013Z 0\u20139 _ \u00b7 max 12 \u00b7 same rule as the device editor</div>' +
      '</div>';

    // The home network. The firmware's own /setup page can still do this and is the
    // card-less path, but it only PRESENTS itself while the device is unprovisioned:
    // once a network is stored the captive-portal probes answer plain success instead
    // of redirecting (ap_server.h handleProbe), so a device that moved house has no
    // visible way back. This is that way back \u2014 same endpoints, somewhere findable.
    h += '<h2 class="sect">// HOME NETWORK</h2>';
    h += '<div class="term">' +
      '<div class="line"><span class="prompt">pet@malwarium</span><span class="path">:~$</span> iwconfig --join</div>' +
      '<form class="tagform netform" id="netform">' +
      '<select id="netssid" aria-label="Network"><option value="">scanning\u2026</option></select>' +
      '<button type="button" id="netrescan" title="scan again">\u21bb</button>' +
      '</form>' +
      '<form class="tagform" id="netpassform">' +
      '<input id="netpass" type="password" maxlength="63" autocomplete="off" ' +
      'spellcheck="false" placeholder="PASSWORD" aria-label="Network password">' +
      '<button type="submit">WRITE</button></form>' +
      '<div class="formmsg" id="netmsg"></div>' +
      '<div class="charset">the habitat only uses this to check for updates \u00b7 ' +
      'saving does not connect \u2014 run CHECK NOW on its UPDATES screen</div>' +
      '</div>';

    // The update source. Editable here because the compiled-in default lives in a
    // firmware image that only changes via the mechanism it points at \u2014 so if the
    // publish host moves, pasting the new address here is the ONLY way to tell a
    // device you cannot reflash.
    h += '<h2 class="sect">// UPDATE SOURCE</h2>';
    h += '<div class="term">' +
      '<div class="line"><span class="prompt">pet@malwarium</span><span class="path">:~$</span> set --update-url</div>' +
      '<form class="tagform" id="urlform">' +
      '<input id="urlinput" type="url" maxlength="159" autocomplete="off" spellcheck="false" ' +
      'placeholder="' + esc((S && S.update_url) || 'https://\u2026/manifest.json') + '" ' +
      'aria-label="Update manifest URL">' +
      '<button type="submit">WRITE</button></form>' +
      '<div class="formmsg" id="urlmsg"></div>' +
      '<div class="charset">full http:// or https:// address \u00b7 empty resets to the built-in one</div>' +
      '</div>';
    h += '<div class="note">where the device looks for updates. it still asks before installing anything \u2014 ' +
      'changing this only changes where it looks.</div>';
    return h;
  }

  function armTagForm() {
    var form = document.getElementById('tagform');
    if (!form) return;
    form.addEventListener('submit', function (e) {
      e.preventDefault();
      var input = document.getElementById('taginput');
      var msg = document.getElementById('tagmsg');
      var v = input.value.toUpperCase().trim();
      if (!/^[A-Z0-9_]{1,12}$/.test(v)) {
        msg.className = 'formmsg deny';
        msg.textContent = '>> DENIED: A\u2013Z 0\u20139 _ only, 1\u201312 chars.';
        return;
      }
      msg.className = 'formmsg';
      msg.textContent = '>> writing\u2026';
      fetch('/api/tag', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tag: v })
      }).then(function (r) {
        if (!r.ok) throw new Error();
        return apply(v);
      }).catch(function () { apply(v + ' (fixture mode \u2014 no device attached)'); });
      function apply(label) {
        S.hacker_tag = v;
        renderIdentity();
        msg.className = 'formmsg ok';
        msg.textContent = '>> OK: tag written \u2014 ' + label;
      }
    });
  }

  // Form-encoded, matching /api/wifi: this is arbitrary pasted text, and the
  // device's own form parsing decodes percent-escapes where its hand-rolled JSON
  // reader does not. An empty value is a legitimate submit — it clears the
  // override and falls back to the built-in address, so a bad paste is
  // recoverable from the same field that caused it.
  function armUrlForm() {
    var form = document.getElementById('urlform');
    if (!form) return;
    form.addEventListener('submit', function (e) {
      e.preventDefault();
      var input = document.getElementById('urlinput');
      var msg = document.getElementById('urlmsg');
      var v = input.value.trim();
      if (v !== '' && !/^https?:\/\/\S+$/i.test(v)) {
        msg.className = 'formmsg deny';
        msg.textContent = '>> DENIED: needs a full http:// or https:// address.';
        return;
      }
      msg.className = 'formmsg';
      msg.textContent = '>> writing…';
      fetch('/api/update-source', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'url=' + encodeURIComponent(v)
      }).then(function (r) {
        if (!r.ok) throw new Error();
        return r.text();
      }).then(function (resolved) {
        // Show what the device RESOLVED to, not what was typed — clearing the
        // override falls back to the built-in address, and that is what it
        // will actually use.
        S.update_url = resolved;
        input.value = '';
        input.placeholder = resolved || 'https://…/manifest.json';
        msg.className = 'formmsg ok';
        msg.textContent = resolved
          ? '>> OK: now checking ' + resolved
          : '>> OK: cleared — this build has no built-in address.';
      }).catch(function () {
        msg.className = 'formmsg deny';
        msg.textContent = '>> DENIED: the device refused it (fixture mode, or a bad address).';
      });
    });
  }

  // The network picker. Endpoints are the firmware's existing setup-portal pair
  // (GET /api/networks, POST /api/wifi) — nothing new device-side, so this ships as a
  // web-bundle update alone. The scan is ASYNC on the device: the first GET kicks a
  // sweep and answers {scanning:true}, so poll until it settles.
  //
  // Form-encoded like /setup's own submit, and for the same reason: a WPA2 passphrase
  // may contain quotes and backslashes, and the device's hand-rolled JSON reader has
  // no escape handling. The password is posted and then dropped — never stored in S,
  // never rendered back, never logged.
  function armNetForm() {
    var sel = document.getElementById('netssid');
    var form = document.getElementById('netpassform');
    if (!sel || !form) return;
    var msg = document.getElementById('netmsg');

    var polling = false;
    function scan() {
      if (polling) return;
      polling = true;
      sel.innerHTML = '<option value="">scanning\u2026</option>';
      poll();
    }
    function poll() {
      fetch('/api/networks').then(function (r) { return r.json(); }).then(function (d) {
        if (d.scanning) { setTimeout(poll, 1200); return; }
        polling = false;
        sel.innerHTML = '';
        if (!d.networks.length) {
          sel.innerHTML = '<option value="">no networks heard</option>';
          return;
        }
        d.networks.forEach(function (n) {
          var o = document.createElement('option');
          o.value = n.ssid;
          o.textContent = n.ssid + '  ' + n.rssi + 'dBm' + (n.open ? '  OPEN' : '');
          sel.appendChild(o);
        });
      }).catch(function () {
        // No device (desk preview against the fixture) — say so rather than
        // spinning on a scan that will never land.
        polling = false;
        sel.innerHTML = '<option value="">no habitat attached</option>';
      });
    }
    scan();
    var rescan = document.getElementById('netrescan');
    if (rescan) rescan.addEventListener('click', function () { scan(); });

    form.addEventListener('submit', function (e) {
      e.preventDefault();
      var pass = document.getElementById('netpass');
      var ssid = sel.value;
      if (!ssid) {
        msg.className = 'formmsg deny';
        msg.textContent = '>> DENIED: pick a network first.';
        return;
      }
      msg.className = 'formmsg';
      msg.textContent = '>> writing\u2026';
      fetch('/api/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass.value)
      }).then(function (r) {
        if (!r.ok) throw new Error();
        pass.value = '';
        msg.className = 'formmsg ok';
        msg.textContent = '>> OK: stored ' + ssid + ' \u2014 check for updates to use it.';
      }).catch(function () {
        pass.value = '';
        msg.className = 'formmsg deny';
        msg.textContent = '>> DENIED: the habitat refused it (fixture mode, or a bad passphrase).';
      });
    });
  }

  function vMissing() {
    return '<div class="card locked"><div class="body">' +
      '<div class="name masked">FILE ENCRYPTED</div>' +
      '<div class="ctx">no signature on record. go touch some \u2019net.</div></div></div>' +
      '<div class="backlink"><a href="#/index">\u25c2 INDEX</a></div>';
  }

  /* ---------- shell ---------- */
  var routes = {
    '': vLanding, 'index': vIndex, 'items': vItems, 'food': vFood, 'moves': vMoves,
    'ach': vAch, 'archive': vArchive, 'tag': vTag
  };

  function renderIdentity() {
    var el = document.getElementById('identity');
    el.innerHTML = '<span class="tag">' + esc((S && S.hacker_tag) || '\u2014') + '</span>' +
      '<span>RANK <b>' + ((S && S.hacker_rank) || 0) + '</b> \u00b7 <b>' +
      Number((S && S.currency_bits) || 0).toLocaleString('en-US') + '</b> BITS \u00b7 ' +
      '<a href="#/tag" aria-label="rename tag">edit</a></span>';
  }

  function render() {
    var hash = location.hash.replace(/^#\/?/, '');
    var view, arg = null;
    if (hash.indexOf('entry/') === 0) { view = function () { return vEntry(hash.slice(6)); }; }
    else if (hash.indexOf('beast/') === 0) { view = function () { return vBeast(hash.slice(6)); }; }
    else view = routes[hash] || vLanding;

    document.getElementById('view').innerHTML = view();
    var active = hash.split('/')[0] || 'home';
    document.querySelectorAll('nav.tabs a').forEach(function (a) {
      a.classList.toggle('active', a.getAttribute('data-tab') === active ||
        (active === 'entry' || active === 'beast' ? a.getAttribute('data-tab') === 'index' : false));
    });
    armHoneytoken();
    armTagForm();
    armNetForm();
    armUrlForm();
    window.scrollTo(0, 0);
  }

  /* ---------- boot: paint immediately, then upgrade to live state. ----------
     The fixture (fixtures/pedia_state.js) is a DESK-PREVIEW convenience and is
     deliberately not staged onto the card, so on-device that script 404s and
     PEDIA_STATE_FIXTURE is undefined — hence the {} floor. Without it every view
     that reads S directly throws, and the throw aborts the rest of this function
     before the live fetch below is ever wired up. */
  S = window.PEDIA_STATE_FIXTURE || {};
  renderIdentity();
  render();
  window.addEventListener('hashchange', render);

  /* Probe the live payload from any http(s) origin. It cannot be gated on the
     device's IP: the AP answers every DNS question with its own address, so the
     phone may arrive under any hostname it happened to ask for, and a miss here
     silently shows fixture (or empty) state as if it were the operator's save.
     Off-device the request just 404s and what is already painted stays. */
  var wantLive = /^https?:$/.test(location.protocol);
  if (wantLive) {
    window.addEventListener('load', function () {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', 'pedia_state.json');
      xhr.timeout = 4000;
      xhr.onload = function () {
        if (xhr.status !== 200) return;
        try {
          S = JSON.parse(xhr.responseText);
          renderIdentity();
          render();
        } catch (e) { /* malformed payload — fixture stays */ }
      };
      xhr.send();
    });
  }
})();
