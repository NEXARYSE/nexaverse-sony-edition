// Read-only controller diagnostics. No device data leaves the launcher.
(() => {
  const card = document.querySelector('.card.controller');
  card.id = 'controllerTester';
  card.setAttribute('aria-label', 'Open controller tester');
  const labels = ['Cross / A', 'Circle / B', 'Square / X', 'Triangle / Y', 'L1 / LB', 'R1 / RB', 'L2 / LT', 'R2 / RT', 'Create / Back', 'Options / Start', 'L3', 'R3', 'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right', 'Home', 'Touchpad'];
  card.onclick = () => {
    const root = modal('Controller tester');
    document.querySelector('#panel').classList.add('gamepad-dialog');
    const status = block('p', 'pad-connection');
    status.setAttribute('role', 'status');
    const select = document.createElement('select');
    select.setAttribute('aria-label', 'Select controller');
    const note = block('p', 'pad-note', 'Connect your controller and press a button to detect it. Move each stick and press buttons to test input.');
    const sticks = block('div', 'pad-sticks');
    const views = ['Left analog stick', 'Right analog stick'].map(label => {
      const section = block('section', 'pad-stick');
      section.append(block('h3', '', label));
      const axis = block('div', 'pad-axis');
      axis.setAttribute('role', 'img');
      const dot = block('i', 'pad-dot');
      const values = block('p', 'pad-values');
      axis.append(dot); section.append(axis, values); sticks.append(section);
      return {axis, dot, values, label};
    });
    const buttons = block('div', 'pad-buttons');
    buttons.setAttribute('aria-label', 'Controller buttons and triggers');
    const raw = block('p', 'pad-values');
    const mapping = block('p', 'pad-note');
    root.append(status, select, note, sticks, buttons, raw, mapping);
    let selectedIndex = null, deviceSignature = '', buttonCount = -1, frame;
    const nodes = [];
    select.onchange = () => { selectedIndex = Number(select.value); };
    const number = value => Number.isFinite(value) ? Math.max(-1, Math.min(1, value)) : 0;
    const format = value => (Math.abs(value) < .0005 ? 0 : value).toFixed(3);
    function draw() {
      if (!document.querySelector('#panel').open || !root.isConnected) { cleanup(); return; }
      let pads = [];
      try { pads = Array.from(navigator.getGamepads?.() || []).filter(p => p?.connected); } catch {}
      const signature = JSON.stringify(pads.map(p => [p.index, p.id]));
      if (signature !== deviceSignature) {
        deviceSignature = signature;
        if (!pads.some(p => p.index === selectedIndex)) selectedIndex = pads[0]?.index ?? null;
        select.replaceChildren();
        for (const pad of pads) {
          const option = document.createElement('option'); option.value = pad.index;
          option.textContent = (pad.index + 1) + ' · ' + pad.id;
          select.append(option);
        }
        select.value = selectedIndex ?? '';
      }
      const pad = pads.find(p => p.index === selectedIndex);
      select.hidden = !pad;
      status.textContent = pad ? pad.id : 'No controller detected';
      mapping.textContent = !pad ? 'USB and Bluetooth controllers supported by Windows can be tested here.' : pad.mapping === 'standard' ? 'Standard layout · Left stick: axes 0/1 · Right stick: axes 2/3' : 'Device layout · Axis pairs 0/1 and 2/3 shown. Button positions and stick axes can vary for this controller.';
      for (const [i, view] of views.entries()) {
        const available = pad && pad.axes.length >= i * 2 + 2;
        const x = number(pad?.axes[i * 2]), y = number(pad?.axes[i * 2 + 1]);
        view.dot.style.transform = `translate(${x * 80}px, ${y * 80}px)`;
        view.axis.setAttribute('aria-label', view.label + (available ? ` X ${format(x)}, Y ${format(y)}` : ' unavailable'));
        view.values.textContent = available ? `X ${format(x)}   Y ${format(y)}` : 'No stick input';
        view.axis.classList.toggle('unavailable', !available);
      }
      const count = pad?.buttons.length || 0, layoutKey = (pad?.mapping || '') + ':' + count;
      if (layoutKey !== buttonCount) {
        buttonCount = layoutKey; buttons.replaceChildren(); nodes.length = 0;
        for (let i = 0; i < count; i++) {
          const el = block('div', 'pad-button');
          el.append(block('span', '', pad.mapping === 'standard' ? labels[i] || 'Button ' + i : 'Button ' + i), block('b', ''));
          const meter = block('i'); el.append(meter); buttons.append(el); nodes.push(el);
        }
      }
      nodes.forEach((el, i) => {
        const input = pad.buttons[i], value = Math.max(0, Math.min(1, number(input.value)));
        el.classList.toggle('pressed', input.pressed || value > .05);
        el.querySelector('b').textContent = value.toFixed(2);
        el.querySelector('i').style.width = value * 100 + '%';
      });
      raw.textContent = pad && pad.axes.length > 4 ? 'Additional axes · ' + Array.from(pad.axes).slice(4).map((v, i) => `${i + 4}: ${format(number(v))}`).join('   ') : '';
      frame = requestAnimationFrame(draw);
    }
    function cleanup() {
      cancelAnimationFrame(frame);
      document.querySelector('#panel').classList.remove('gamepad-dialog');
      document.querySelector('#panel').removeEventListener('close', cleanup);
    }
    document.querySelector('#panel').addEventListener('close', cleanup);
    draw();
  };
})();
