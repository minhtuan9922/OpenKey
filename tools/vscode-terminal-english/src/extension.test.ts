import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { test } from 'node:test';
import { runInNewContext } from 'node:vm';
import { TerminalSwitcher, type InputState } from './switcher';

const enter = 'openkeyTerminalEnglish.focusTerminal';
const leave = 'openkeyTerminalEnglish.focusEditor';
const terminalFocus = 'workbench.action.terminal.focus';
const editorFocus = 'workbench.action.focusActiveEditorGroup';
const english = (): InputState => ({ active: false, method: 'keyboard-us' });
const vietnamese = (): InputState => ({ active: true, method: 'openkey-telex' });

function gate() {
  let release!: () => void;
  const promise = new Promise<void>(resolve => { release = resolve; });
  return { promise, release };
}

interface Terminal { name: string; exitStatus?: { code: number } }

function harness(platform = 'linux') {
  let enabled = false;
  let target: 'editor' | 'terminal' = 'editor';
  const commands = new Map<string, () => Promise<void>>();
  const focusListeners = new Set<(state: { focused: boolean }) => void>();
  const closeListeners = new Set<(terminal: Terminal) => void>();
  const configListeners = new Set<(event: { affectsConfiguration(key: string): boolean }) => void>();
  const focusCommands: string[] = [];
  const warnings: string[] = [];
  const statuses: string[] = [];
  const signals: AbortSignal[] = [];
  const checks: AbortSignal[] = [];
  const settles: AbortSignal[] = [];
  const calls: string[] = [];
  const state = { editor: vietnamese(), terminal: vietnamese() };
  let readError: Error | undefined;
  let readCount = 0;
  let blockedRead: { at: number; reached: ReturnType<typeof gate>; blocked: ReturnType<typeof gate> } | undefined;
  let cancellations = 0;

  class Disposable {
    private readonly callback: () => void;
    constructor(callback: () => void) { this.callback = callback; }
    dispose(): void { this.callback(); }
  }
  function listen<T>(listeners: Set<(value: T) => void>, callback: (value: T) => void) {
    listeners.add(callback);
    return new Disposable(() => { listeners.delete(callback); });
  }
  const window = {
    state: { focused: true },
    activeTerminal: { name: 'first' } as Terminal | undefined,
    onDidChangeWindowState: (callback: (value: { focused: boolean }) => void) => listen(focusListeners, callback),
    onDidCloseTerminal: (callback: (value: Terminal) => void) => listen(closeListeners, callback),
    showWarningMessage: async (message: string) => { warnings.push(message); },
    setStatusBarMessage: (message: string) => { statuses.push(message); return new Disposable(() => {}); },
  };
  const env = { uiKind: 1 };
  class FakeRemoteClient {
    private controller = new AbortController();
    private readonly allowed: () => boolean;
    constructor(allowed: () => boolean) { this.allowed = allowed; }
    signal(): AbortSignal {
      signals.push(this.controller.signal);
      return this.controller.signal;
    }
    check(signal: AbortSignal): void {
      checks.push(signal);
      if (signal.aborted || !this.allowed()) throw new Error('Cancelled fake operation');
    }
    cancel(): void {
      cancellations += 1;
      this.controller.abort();
      this.controller = new AbortController();
    }
    async read(): Promise<InputState> {
      const signal = this.signal();
      this.check(signal);
      calls.push('read');
      readCount += 1;
      if (blockedRead?.at === readCount) {
        blockedRead.reached.release();
        await blockedRead.blocked.promise;
      }
      this.check(signal);
      if (readError) throw readError;
      return { ...state[target] };
    }
    async deactivate(): Promise<void> {
      this.check(this.signal());
      calls.push('deactivate');
      state[target].active = false;
    }
    async activate(method: string): Promise<void> {
      this.check(this.signal());
      calls.push(`activate:${method}`);
      state[target] = { active: true, method };
    }
  }
  const vscode = {
    Disposable, window, env, UIKind: { Desktop: 1, Web: 2 },
    workspace: {
      getConfiguration: () => ({ get: (_key: string, fallback: boolean) => enabled || fallback }),
      onDidChangeConfiguration: (callback: (event: { affectsConfiguration(key: string): boolean }) => void) =>
        listen(configListeners, callback),
    },
    commands: {
      registerCommand: (id: string, callback: () => Promise<void>) => {
        commands.set(id, callback);
        return new Disposable(() => { commands.delete(id); });
      },
      executeCommand: async (id: string) => {
        assert.ok(id === terminalFocus || id === editorFocus);
        focusCommands.push(id);
        target = id === terminalFocus ? 'terminal' : 'editor';
        if (target === 'terminal' && !window.activeTerminal) window.activeTerminal = { name: 'created' };
      },
    },
  };
  const exports: { activate?: (context: { subscriptions: Disposable[] }) => void } = {};
  runInNewContext(readFileSync(join(__dirname, 'extension.js'), 'utf8'), {
    exports,
    process: { platform },
    require: (id: string): unknown => {
      if (id === 'vscode') return vscode;
      if (id === './remote') return { RemoteClient: FakeRemoteClient };
      if (id === './switcher') return { TerminalSwitcher };
      if (id === 'node:timers/promises') return {
        setTimeout: async (_delay: number, _value: unknown, options: { signal: AbortSignal }) => {
          settles.push(options.signal);
          await Promise.resolve();
          if (options.signal.aborted) throw new Error('Cancelled fake timer');
        },
      };
      throw new Error(`Unexpected compiled extension import: ${id}`);
    },
  }, { filename: join(__dirname, 'extension.js') });
  const subscriptions: Disposable[] = [];
  assert.ok(exports.activate);
  exports.activate({ subscriptions });
  return {
    window, env, state, calls, focusCommands, warnings, statuses, signals, checks, settles, subscriptions,
    get cancellations() { return cancellations; },
    async command(id: string) {
      const callback = commands.get(id);
      assert.ok(callback, `Command registered: ${id}`);
      await callback();
    },
    enable(value = true) { enabled = value; },
    configure(value: boolean, key = 'openkeyTerminalEnglish.enabled') {
      enabled = value;
      for (const callback of configListeners) callback({ affectsConfiguration: candidate => candidate === key });
    },
    focus(focused: boolean) {
      window.state.focused = focused;
      for (const callback of focusListeners) callback({ focused });
    },
    close(terminal: Terminal) { for (const callback of closeListeners) callback(terminal); },
    failRead(error: Error) { readError = error; },
    gateRead(at: number) {
      blockedRead = { at, reached: gate(), blocked: gate() };
      return blockedRead;
    },
  };
}

test('commands register but remain disabled by default', async () => {
  const h = harness();
  await h.command(enter);
  await h.command(leave);
  assert.equal(h.subscriptions.length, 6);
  assert.deepEqual(h.calls, []);
  assert.deepEqual(h.focusCommands, []);
  assert.deepEqual(h.signals, []);
  assert.deepEqual(h.warnings, []);
  assert.deepEqual(h.statuses, []);
});

test('supported round trip notifies, restores and clears owned terminal', async () => {
  const h = harness();
  h.enable();
  await h.command(enter);
  assert.equal(h.state.terminal.active, false);
  h.state.editor = english();
  await h.command(leave);
  assert.deepEqual(h.state.editor, vietnamese());
  assert.deepEqual(h.focusCommands, [terminalFocus, editorFocus]);
  assert.equal(h.statuses.length, 2);
  assert.equal(h.warnings.length, 0);
  assert.equal(h.settles.length, 2);
  for (const signal of h.settles) {
    assert.ok(h.signals.includes(signal));
    assert.ok(h.checks.filter(checked => checked === signal).length >= 2);
  }
  const cancellations = h.cancellations;
  assert.ok(h.window.activeTerminal);
  h.close(h.window.activeTerminal);
  assert.equal(h.cancellations, cancellations);
  h.calls.length = 0;
  await h.command(leave);
  assert.deepEqual(h.calls, []);
});

test('original English never activates an input method', async () => {
  const h = harness();
  h.enable();
  h.state.editor = english();
  h.state.terminal = english();
  await h.command(enter);
  await h.command(leave);
  assert.ok(h.calls.every(call => call === 'read'));
  assert.equal(h.statuses.length, 2);
});

test('lost window focus cancels a pending enter and clears ownership', async () => {
  const h = harness();
  h.enable();
  const blocked = h.gateRead(2);
  const pending = h.command(enter);
  await blocked.reached.promise;
  const signal = h.signals.at(-1);
  assert.ok(signal);
  h.focus(false);
  assert.equal(signal.aborted, true);
  blocked.blocked.release();
  await pending;
  await h.command(leave);
  assert.deepEqual(h.calls, ['read', 'read']);
  assert.equal(h.warnings.length, 0);
  assert.equal(h.statuses.length, 0);
  h.focus(true);
  h.calls.length = 0;
  await h.command(leave);
  assert.deepEqual(h.calls, []);
  assert.equal(h.focusCommands.at(-1), editorFocus);
});

test('changing terminal tabs drops the snapshot and only focuses editor', async () => {
  const h = harness();
  h.enable();
  await h.command(enter);
  h.window.activeTerminal = { name: 'second' };
  h.state.editor = english();
  h.calls.length = 0;
  h.focusCommands.length = 0;
  await h.command(leave);
  assert.equal(h.cancellations, 1);
  assert.deepEqual(h.calls, []);
  assert.deepEqual(h.focusCommands, [editorFocus]);
  assert.deepEqual(h.state.editor, english());
});

test('disabling configuration invalidates the snapshot across re-enabling', async () => {
  const h = harness();
  h.enable();
  await h.command(enter);
  h.configure(false);
  assert.equal(h.cancellations, 1);
  h.calls.length = 0;
  h.focusCommands.length = 0;
  await h.command(leave);
  assert.deepEqual(h.focusCommands, []);
  h.configure(true);
  await h.command(leave);
  assert.deepEqual(h.calls, []);
  assert.deepEqual(h.focusCommands, [editorFocus]);
});

test('closing an owned terminal cancels; closing another terminal does not', async () => {
  const h = harness();
  h.enable();
  await h.command(enter);
  h.close({ name: 'unrelated' });
  assert.equal(h.cancellations, 0);
  assert.ok(h.window.activeTerminal);
  h.close(h.window.activeTerminal);
  assert.equal(h.cancellations, 1);
  assert.ok(h.signals.some(signal => signal.aborted));
  h.calls.length = 0;
  await h.command(leave);
  assert.deepEqual(h.calls, []);
});

test('duplicate leave while enter is gated preserves ownership for close cancellation', async () => {
  const h = harness();
  h.enable();
  const blocked = h.gateRead(2);
  const pending = h.command(enter);
  await blocked.reached.promise;
  await h.command(leave);
  await h.command(leave);
  assert.deepEqual(h.focusCommands, [terminalFocus]);
  assert.equal(h.statuses.length, 0);
  assert.equal(h.warnings.length, 0);
  assert.ok(h.window.activeTerminal);
  h.close(h.window.activeTerminal);
  assert.equal(h.cancellations, 1);
  blocked.blocked.release();
  await pending;
  assert.deepEqual(h.calls, ['read', 'read']);
  await h.command(leave);
  assert.deepEqual(h.calls, ['read', 'read']);
});

test('Windows and web UI commands perform no remote or focus operations', async () => {
  const windows = harness('win32');
  const web = harness();
  web.env.uiKind = 2;
  for (const h of [windows, web]) {
    h.enable();
    await h.command(enter);
    await h.command(leave);
    assert.deepEqual(h.calls, []);
    assert.deepEqual(h.focusCommands, []);
    assert.deepEqual(h.signals, []);
    assert.deepEqual(h.statuses, []);
    assert.deepEqual(h.warnings, []);
  }
});

test('remote read errors produce a warning without leaking the error or retrying', async () => {
  const h = harness();
  h.enable();
  h.failRead(new Error('private remote diagnostic'));
  await h.command(enter);
  assert.deepEqual(h.calls, ['read']);
  assert.equal(h.warnings.length, 1);
  assert.equal(h.statuses.length, 0);
  assert.deepEqual(h.focusCommands, []);
  await h.command(leave);
  assert.deepEqual(h.calls, ['read']);
  assert.deepEqual(h.focusCommands, [editorFocus]);
  assert.equal(h.warnings.length, 1);
});
