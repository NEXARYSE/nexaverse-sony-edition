const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const { parseHTML } = require('linkedom');
const { icon, expires } = require('./welcome-theme.js');

test('holiday tile expires at local midnight on January 1, 2027', () => {
  assert.equal(icon(new Date(2026, 11, 31, 23, 59, 59, 999).getTime()), 'assets/welcome-holiday-icon.png');
  assert.equal(icon(new Date(2027, 0, 1).getTime()), 'assets/launcher-icon.png');
  assert.equal(icon(new Date(2027, 5, 1).getTime()), 'assets/launcher-icon.png');
});

test('open launcher rolls its icon back and pauses snowfall when hidden', () => {
  const { document } = parseHTML('<body><div class="welcome-snow"></div><button class="tile welcome"><img></button></body>');
  let now = expires - 1000;
  document.hasFocus = () => true;
  document.hidden = false;
  const timers = [], events = {};
  const clock = class extends Date { static now() { return now; } };
  vm.runInNewContext(fs.readFileSync('welcome-theme.js', 'utf8'), {
    document, Date: clock,
    window: { addEventListener: (name, fn) => { events[name] = fn; } },
    setTimeout: (fn, delay) => timers.push({ fn, delay })
  });
  assert.equal(document.querySelectorAll('.welcome-snow i').length, 120);
  assert.equal(document.querySelector('.tile.welcome img').getAttribute('src'), 'assets/welcome-holiday-icon.png');
  assert.equal(timers[0].delay, 1000);
  now = expires;
  timers[0].fn();
  assert.equal(document.querySelector('.tile.welcome img').getAttribute('src'), 'assets/launcher-icon.png');
  document.hidden = true;
  events.blur();
  assert(document.body.classList.contains('welcome-snow-paused'));
});
