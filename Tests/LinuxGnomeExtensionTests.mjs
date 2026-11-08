import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {XML} from '../tools/gnome-app-identity/protocol.js';
import {FocusState, validAppId} from '../tools/gnome-app-identity/focusState.js';
for (const id of ['', 'window:12', '../chrome.desktop', 'gnome-shell', 'a.desktop' + String.fromCharCode(10), 'x'.repeat(121) + '.desktop'])
    assert.equal(validAppId(id), false);
assert.equal(validAppId('google-chrome.desktop'), true);
const state = new FocusState('session');
const first = {}, second = {};
assert.equal(state.update('google-chrome.desktop', first), true);
const serial = state.serial;
assert.equal(state.update('google-chrome.desktop', first), false);
assert.equal(state.serial, serial);
assert.equal(state.update('google-chrome.desktop', second), true);
state.update('code.desktop', second, true);
assert.equal(state.appId, '');
assert.equal(state.window, null);
state.serial = 0xffffffff;
state.update('code.desktop', first);
assert.equal(state.serial, 1);

class Signals {
    next = 1; handlers = new Map();
    connect(signal, callback) { const id = this.next++; this.handlers.set(id, {signal, callback}); return id; }
    disconnect(id) { assert(this.handlers.delete(id)); }
    fire(signal) { for (const handler of this.handlers.values()) if (handler.signal === signal) handler.callback(); }
}
const display = new Signals();
display.focus_window = first;
const mode = new Signals();
Object.assign(mode, {currentMode: 'ubuntu', isLocked: false, isGreeter: false, hasWindows: true});
const overview = new Signals(); overview.visible = false;
let exported = 0, unexported = 0, changed = 0;
const source = fs.readFileSync(new URL('../tools/gnome-app-identity/extension.js', import.meta.url), 'utf8')
    .replace(/^import .*;\n/gm, '').replace('export default class', 'globalThis.TestExtension = class');
const sandbox = {
    Gio: {DBus: {session: {}}, DBusExportedObject: {wrapJSObject(xml) {
        assert(xml.includes('GetFocus')); assert(xml.includes('type="u"'));
        return {export() { exported++; }, unexport() { unexported++; }, emit_signal() { changed++; }};
    }}},
    GLib: {uuid_string_random: () => '12345678-1234-1234-1234-123456789abc'},
    Shell: {WindowTracker: {get_default: () => ({get_window_app: () => ({get_id: () => 'code.desktop'})})}},
    Extension: class {}, Main: {sessionMode: mode, overview, modalCount: 0},
    global: {display}, FocusState, XML,
};
vm.runInNewContext(source, sandbox);
const extension = new sandbox.TestExtension(); extension.enable();
assert.equal(exported, 1);
assert.equal(extension.GetFocus()[0], 'code.desktop');
overview.visible = true; overview.fire('showing');
assert.equal(extension.GetFocus()[0], '');
overview.visible = false; overview.fire('hidden');
mode.isLocked = true; mode.fire('updated');
assert.equal(extension.GetFocus()[0], '');
mode.isLocked = false; mode.fire('updated');
sandbox.Main.modalCount = 1;
assert.equal(extension.GetFocus()[0], '');
sandbox.Main.modalCount = 0;
assert.equal(extension.GetFocus()[0], 'code.desktop');
assert(changed > 0);
extension.disable();
assert.equal(unexported, 1);
assert.equal(display.handlers.size + mode.handlers.size + overview.handlers.size, 0);
console.log('GNOME metadata state, focus/window identity, lock/overview/modal and extension lifecycle passed');
