import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { RemoteClient, type Runner } from './remote';

test('query is bounded and uses only checked read arguments', async () => {
  const recorded: string[][] = [];
  const outputs = ['2', 'openkey-telex', '2'];
  const client = new RemoteClient(() => true, async args => {
    recorded.push([...args]); return outputs.shift()!;
  });
  assert.deepEqual(await client.read(), { active: true, method: 'openkey-telex' });
  assert.deepEqual(recorded, [['--check'], ['--check', '-n'], ['--check']]);
});
test('missing daemon, invalid name and changing state are rejected', async () => {
  for (const outputs of [['0'], ['2', 'bad;command'], ['2', 'openkey-vni', '1']]) {
    const client = new RemoteClient(() => true, async () => outputs.shift()!);
    await assert.rejects(client.read());
  }
});
test('no process call when not allowed; no arbitrary method switch', async () => {
  let calls = 0;
  const runner: Runner = async () => { calls++; return ''; };
  const denied = new RemoteClient(() => false, runner);
  await assert.rejects(denied.deactivate());
  const allowed = new RemoteClient(() => true, runner);
  await assert.rejects(allowed.activate('keyboard-us;echo bad'));
  assert.equal(calls, 0);
});
test('restore uses fixed method arguments, never shell, reload or daemon launch', async () => {
  const recorded: string[][] = [];
  const client = new RemoteClient(() => true, async args => { recorded.push([...args]); return ''; });
  await client.deactivate();
  await client.activate('openkey-vni');
  assert.deepEqual(recorded, [['--check', '-c'], ['--check', '-s', 'openkey-vni'], ['--check', '-o']]);
});
test('cancelling a restore prevents subsequent activation even after focus returns', async () => {
  const recorded: string[][] = [];
  let client: RemoteClient;
  client = new RemoteClient(() => true, async args => {
    recorded.push([...args]); client.cancel(); return '';
  });
  await assert.rejects(client.activate('openkey-telex'));
  assert.deepEqual(recorded, [['--check', '-s', 'openkey-telex']]);
});
test('manifest is local UI, disabled by default and has focus-scoped shortcuts', () => {
  const manifest = JSON.parse(readFileSync(new URL('../package.json', `file://${__dirname}/`), 'utf8'));
  assert.deepEqual(manifest.extensionKind, ['ui']);
  assert.equal(manifest.contributes.configuration.properties['openkeyTerminalEnglish.enabled'].default, false);
  const keys = manifest.contributes.keybindings;
  assert.equal(keys.length, 2);
  for (const key of keys) assert.match(key.when, /isLinux && config.openkeyTerminalEnglish.enabled/);
  assert.match(keys[0].when, /editorTextFocus/);
  assert.match(keys[1].when, /terminalFocus/);
});
