import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { TerminalSwitcher } from './switcher';
import type { InputState, SwitchPort } from './switcher';

const telex = 'openkey-telex';
const vni = 'openkey-vni';
const english = (): InputState => ({ active: false, method: 'keyboard-us' });
const active = (method = telex): InputState => ({ active: true, method });

function gate() {
  let release!: () => void;
  const promise = new Promise<void>((resolve) => { release = resolve; });
  return { promise, release };
}

class FakePort implements SwitchPort {
  focused = true;
  target: 'terminal' | 'editor' = 'editor';
  editor = active();
  terminal = active();
  calls: string[] = [];
  reads: Array<InputState | Error> = [];
  hook: (call: string) => Promise<void> = async () => {};
  ignoreDeactivate = false;
  ignoreActivate = false;
  focusCheck = (): boolean => this.focused;

  isFocused(): boolean { return this.focusCheck(); }

  async focus(target: 'terminal' | 'editor'): Promise<void> {
    await this.record(`focus:${target}`);
    this.target = target;
  }

  async read(): Promise<InputState> {
    await this.record('read');
    const next = this.reads.shift();
    if (next instanceof Error) throw next;
    return { ...(next ?? this.state()) };
  }

  async deactivate(): Promise<void> {
    await this.record('deactivate');
    if (!this.ignoreDeactivate) this.state().active = false;
  }

  async activate(method: string): Promise<void> {
    await this.record(`activate:${method}`);
    if (!this.ignoreActivate) Object.assign(this.state(), active(method));
  }

  private state(): InputState { return this[this.target]; }

  private async record(call: string): Promise<void> {
    this.calls.push(call);
    assert.equal(this.focused, true, `Unfocused operation: ${call}`);
    await this.hook(call);
  }
}

function setup() {
  const port = new FakePort();
  return { port, switcher: new TerminalSwitcher(port) };
}

for (const method of [telex, vni]) {
  test(`${method} round trip verifies both mutations`, async () => {
    const { port, switcher } = setup();
    port.editor = active(method);
    assert.equal(await switcher.enter(), 'entered');
    assert.equal(port.terminal.active, false);
    port.editor = english();
    assert.equal(await switcher.leave(), 'restored');
    assert.deepEqual(port.editor, active(method));
    assert.deepEqual(port.calls, [
      'read', 'focus:terminal', 'read', 'deactivate', 'read',
      'read', 'focus:editor', 'read', `activate:${method}`, 'read',
    ]);
  });
}

test('prior English retains the editor state without any restoration', async () => {
  for (const editor of [english(), active(vni), active('other-im')]) {
    const { port, switcher } = setup();
    port.editor = english();
    port.terminal = english();
    assert.equal(await switcher.enter(), 'entered');
    port.editor = editor;
    assert.equal(await switcher.leave(), 'unchanged');
    assert.deepEqual(port.editor, editor);
    assert.equal(port.calls.some((call) => call === 'deactivate' || call.startsWith('activate:')), false);
  }
});

test('active other IM is unsupported before focus or mutation', async () => {
  const { port, switcher } = setup();
  port.editor = active('other-im');
  assert.equal(await switcher.enter(), 'unsupported');
  assert.deepEqual(port.calls, ['read']);
  assert.equal(await switcher.leave(), 'no-snapshot');
  assert.deepEqual(port.calls, ['read', 'focus:editor']);
});

test('duplicate enter keeps the original snapshot', async () => {
  const { port, switcher } = setup();
  assert.equal(await switcher.enter(), 'entered');
  const calls = [...port.calls];
  assert.equal(await switcher.enter(), 'already-terminal');
  assert.deepEqual(port.calls, calls);
  port.editor = english();
  assert.equal(await switcher.leave(), 'restored');
  assert.deepEqual(port.editor, active(telex));
});

test('rapid commands return busy and never queue', async () => {
  const { port, switcher } = setup();
  const blocked = gate();
  port.hook = () => blocked.promise;
  const entering = switcher.enter();
  assert.equal(await switcher.enter(), 'busy');
  assert.equal(await switcher.leave(), 'busy');
  assert.deepEqual(port.calls, ['read']);
  port.hook = async () => {};
  blocked.release();
  assert.equal(await entering, 'entered');
  const leavingGate = gate();
  port.hook = () => leavingGate.promise;
  const leaving = switcher.leave();
  assert.equal(await switcher.enter(), 'busy');
  assert.equal(await switcher.leave(), 'busy');
  port.hook = async () => {};
  leavingGate.release();
  assert.equal(await leaving, 'unchanged');
});

test('manual terminal activation forgets ownership and only focuses editor', async () => {
  const { port, switcher } = setup();
  await switcher.enter();
  port.terminal = active(vni);
  port.calls = [];
  assert.equal(await switcher.leave(), 'changed');
  assert.deepEqual(port.calls, ['read', 'focus:editor']);
  assert.equal(await switcher.leave(), 'no-snapshot');
});

test('editor active other IM is retained, including another OpenKey method', async () => {
  for (const method of ['other-im', vni]) {
    const { port, switcher } = setup();
    await switcher.enter();
    port.editor = active(method);
    port.calls = [];
    assert.equal(await switcher.leave(), 'changed');
    assert.deepEqual(port.calls, ['read', 'focus:editor', 'read']);
    assert.deepEqual(port.editor, active(method));
  }
});

test('editor already using original method needs no activation', async () => {
  const { port, switcher } = setup();
  await switcher.enter();
  port.calls = [];
  assert.equal(await switcher.leave(), 'unchanged');
  assert.deepEqual(port.calls, ['read', 'focus:editor', 'read']);
});

test('leave without ownership only focuses editor', async () => {
  const { port, switcher } = setup();
  assert.equal(await switcher.leave(), 'no-snapshot');
  assert.deepEqual(port.calls, ['focus:editor']);
});

for (const failure of ['initial-read', 'terminal-read', 'deactivate', 'readback', 'active-readback', 'focus']) {
  test(`enter ${failure} failure clears ownership without recovery or retry`, async () => {
    const { port, switcher } = setup();
    let read = 0;
    port.hook = async (call) => {
      if (call === 'read') read += 1;
      if ((failure === 'initial-read' && read === 1) ||
          (failure === 'terminal-read' && read === 2) ||
          (failure === 'readback' && read === 3) ||
          (failure === 'deactivate' && call === 'deactivate') ||
          (failure === 'focus' && call === 'focus:terminal')) throw new Error('unavailable');
    };
    port.ignoreDeactivate = failure === 'active-readback';
    assert.equal(await switcher.enter(), 'error');
    assert.equal(port.calls.some((call) => call.startsWith('activate:')), false);
    port.hook = async () => {};
    port.calls = [];
    assert.equal(await switcher.leave(), 'no-snapshot');
    assert.deepEqual(port.calls, ['focus:editor']);
  });
}

for (const failure of ['terminal-read', 'focus', 'editor-read', 'activate', 'readback', 'inactive-readback', 'wrong-method']) {
  test(`leave ${failure} failure clears snapshot without recovery`, async () => {
    const { port, switcher } = setup();
    await switcher.enter();
    port.editor = english();
    port.calls = [];
    let read = 0;
    port.hook = async (call) => {
      if (call === 'read') read += 1;
      if ((failure === 'terminal-read' && read === 1) ||
          (failure === 'editor-read' && read === 2) ||
          (failure === 'readback' && read === 3) ||
          (failure === 'focus' && call === 'focus:editor') ||
          (failure === 'activate' && call.startsWith('activate:'))) throw new Error('unavailable');
    };
    port.ignoreActivate = failure === 'inactive-readback';
    if (failure === 'wrong-method') port.reads = [english(), english(), active(vni)];
    assert.equal(await switcher.leave(), 'error');
    assert.equal(port.calls.filter((call) => call.startsWith('activate:')).length <= 1, true);
    port.hook = async () => {};
    port.calls = [];
    assert.equal(await switcher.leave(), 'no-snapshot');
    assert.deepEqual(port.calls, ['focus:editor']);
  });
}

for (const command of ['enter', 'leave'] as const) {
  test(`${command} checks focus again immediately before writing`, async () => {
    const { port, switcher } = setup();
    if (command === 'leave') {
      await switcher.enter();
      port.editor = english();
    }
    port.calls = [];
    let checksAfterRead = 0;
    port.focusCheck = () => {
      if (port.calls.length === 3) {
        checksAfterRead += 1;
        // The read completion guard passes, then focus is lost before the write.
        if (checksAfterRead === 2) port.focused = false;
      }
      return port.focused;
    };
    assert.equal(await switcher[command](), 'cancelled');
    assert.deepEqual(port.calls, command === 'enter'
      ? ['read', 'focus:terminal', 'read']
      : ['read', 'focus:editor', 'read']);
  });

  for (const stopAt of [1, 2, 3, 4, 5]) {
    for (const cancellation of ['focus-loss', 'invalidate', 'dispose']) {
      test(`${command} ${cancellation} during operation ${stopAt} stops all later operations`, async () => {
        const { port, switcher } = setup();
        if (command === 'leave') {
          await switcher.enter();
          port.editor = english();
        }
        port.calls = [];
        const reached = gate();
        const blocked = gate();
        port.hook = async () => {
          if (port.calls.length === stopAt) {
            reached.release();
            await blocked.promise;
          }
        };
        const pending = switcher[command]();
        await reached.promise;
        if (cancellation === 'focus-loss') port.focused = false;
        if (cancellation === 'invalidate') switcher.invalidate();
        if (cancellation === 'dispose') switcher.dispose();
        blocked.release();
        assert.equal(await pending, 'cancelled');
        assert.equal(port.calls.length, stopAt);
        port.focused = true;
        port.hook = async () => {};
        port.calls = [];
        assert.equal(await switcher.leave(), cancellation === 'dispose' ? 'cancelled' : 'no-snapshot');
        assert.deepEqual(port.calls, cancellation === 'dispose' ? [] : ['focus:editor']);
      });
    }
  }
}

test('commands check focus at entry and dispose permanently blocks operations', async () => {
  const { port, switcher } = setup();
  port.focused = false;
  assert.equal(await switcher.enter(), 'cancelled');
  assert.equal(await switcher.leave(), 'cancelled');
  port.focused = true;
  switcher.dispose();
  switcher.dispose();
  assert.equal(await switcher.enter(), 'cancelled');
  assert.equal(await switcher.leave(), 'cancelled');
  assert.deepEqual(port.calls, []);
});

test('invalidation clears an established snapshot', async () => {
  const { port, switcher } = setup();
  await switcher.enter();
  switcher.invalidate();
  port.calls = [];
  assert.equal(await switcher.leave(), 'no-snapshot');
  assert.deepEqual(port.calls, ['focus:editor']);
});

for (const message of ['Fcitx inactive state 0', 'No running daemon']) {
  test(`unavailable state read (${message}) never establishes ownership`, async () => {
    const { port, switcher } = setup();
    port.reads = [new Error(message)];
    assert.equal(await switcher.enter(), 'error');
    assert.deepEqual(port.calls, ['read']);
    assert.equal(await switcher.leave(), 'no-snapshot');
    assert.deepEqual(port.calls, ['read', 'focus:editor']);
  });
}
