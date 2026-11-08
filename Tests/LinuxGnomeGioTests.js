// Private session bus only; exercise the actual GJS/Gio marshaler used by Shell.
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import System from 'system';
import {XML} from '../tools/gnome-app-identity/protocol.js';
import {FocusState} from '../tools/gnome-app-identity/focusState.js';

const loop = new GLib.MainLoop(null, false);
const state = new FocusState(GLib.uuid_string_random());
state.update('code.desktop', {});
const endpoint = Gio.DBusExportedObject.wrapJSObject(XML, {GetFocus: () => state.snapshot()});
endpoint.export(Gio.DBus.session, '/org/openkey/FocusedApplication');
let failed = false;
const finish = error => {
    if (error) { failed = true; printerr('GJS bridge protocol test failed'); }
    loop.quit();
};
const timeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 3000, () => { finish(true); return GLib.SOURCE_REMOVE; });
const owner = Gio.bus_own_name_on_connection(Gio.DBus.session, 'org.gnome.Shell', Gio.BusNameOwnerFlags.NONE, () => {
    endpoint.emit_signal('Changed', null);
    Gio.DBus.session.call('org.gnome.Shell', '/org/openkey/FocusedApplication',
        'org.openkey.FocusedApplication1', 'GetFocus', null, new GLib.VariantType('(ssu)'),
        Gio.DBusCallFlags.NO_AUTO_START, 1000, null, (connection, result) => {
            try {
                const [id, session, serial] = connection.call_finish(result).deep_unpack();
                if (id !== 'code.desktop' || session !== state.session || serial !== state.serial) throw new Error();
                state.update('', null, true);
                endpoint.unexport();
                finish(false);
            } catch { finish(true); }
        });
}, () => finish(true));
loop.run();
GLib.source_remove(timeout);
Gio.bus_unown_name(owner);
if (failed) System.exit(1);
print('Actual GJS/Gio focused-application endpoint marshaling passed');
