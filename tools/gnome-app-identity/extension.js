import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {FocusState} from './focusState.js';
import {XML} from './protocol.js';

export default class OpenKeyAppIdentity extends Extension {
    enable() {
        this._state = new FocusState(GLib.uuid_string_random());
        this._signals = [];
        try {
            this._object = Gio.DBusExportedObject.wrapJSObject(XML, this);
            this._object.export(Gio.DBus.session, '/org/openkey/FocusedApplication');
            this._connect(global.display, 'notify::focus-window');
            this._connect(Main.sessionMode, 'updated');
            this._connect(Main.overview, 'showing');
            this._connect(Main.overview, 'hidden');
            this._refresh(true);
        } catch (error) {
            this.disable();
            throw error;
        }
    }
    _connect(object, signal) {
        this._signals.push([object, object.connect(signal, () => this._refresh(true))]);
    }
    _refresh(emit) {
        const blocked = Main.sessionMode.isLocked || Main.sessionMode.isGreeter || !Main.sessionMode.hasWindows ||
            Main.overview.visible || Main.modalCount > 0;
        const window = blocked ? null : global.display.focus_window;
        const app = window ? Shell.WindowTracker.get_default().get_window_app(window) : null;
        if (this._state.update(app?.get_id() ?? '', window, blocked) && emit)
            this._object?.emit_signal('Changed', null);
    }
    GetFocus() {
        // Re-read at request time. Do not recursively invalidate this request
        // with a Changed signal caused by its own refresh.
        this._refresh(false);
        return this._state.snapshot();
    }
    disable() {
        for (const [object, signal] of this._signals ?? [])
            object.disconnect(signal);
        this._signals = [];
        this._object?.emit_signal('Changed', null);
        this._object?.unexport();
        this._object = null;
        this._state = null;
    }
}
