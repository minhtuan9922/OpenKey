#!/usr/bin/env python3
"""Verify the complete DEB without installing or activating it on the desktop."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def main():
    deb, build = (Path(arg).resolve() for arg in sys.argv[1:])
    assert run('dpkg-deb', '-f', str(deb), 'Package').strip() == 'openkey'
    assert run('dpkg-deb', '-f', str(deb), 'Version').strip() == '3.1.0~beta.1-1'
    deps = run('dpkg-deb', '-f', str(deb), 'Depends')
    for name in ('fcitx5', 'fcitx5-module-dbus', 'fcitx5-frontend-gtk3', 'fcitx5-frontend-qt6',
                 'qml6-module-qtquick-dialogs', 'libqt6core6', 'libsystemd0', 'python3'):
        assert name in deps, name
    for field in ('Conflicts', 'Replaces'):
        value = run('dpkg-deb', '-f', str(deb), field)
        assert 'fcitx5-openkey' in value and 'openkey-settings' in value
    expected = Path(str(deb) + '.sha256').read_text().split()[0]
    assert hashlib.sha256(deb.read_bytes()).hexdigest() == expected
    with tempfile.TemporaryDirectory(prefix='openkey-beta-verify-', dir=build) as temporary:
        root = Path(temporary)/'root'
        control = Path(temporary)/'control'
        run('dpkg-deb', '-x', str(deb), str(root))
        run('dpkg-deb', '-e', str(deb), str(control))
        for hook in ('preinst', 'postinst', 'prerm', 'postrm'):
            assert not (control/hook).exists(), 'Unexpected privileged hook: ' + hook
        required = ('usr/bin/openkey-settings', 'usr/bin/openkey-service', 'usr/bin/openkey-tray-autostart',
                    'usr/bin/openkey-tray-systemd', 'usr/bin/openkey-compat-report',
                    'usr/share/applications/org.openkey.Settings.desktop',
                    'usr/share/dbus-1/services/org.openkey.Settings.service',
                    'etc/xdg/autostart/openkey-tray.desktop', 'usr/share/doc/openkey/README.md')
        for path in required:
            assert (root/path).is_file(), path
        addons = list(root.glob('usr/lib/**/fcitx5/openkey.so'))
        assert len(addons) == 1
        libdir = str(addons[0].parent.parent.relative_to(root/'usr'))
        extension = root/'usr/share/gnome-shell/extensions/openkey-app-identity@openkey.local'
        assert {p.name for p in extension.iterdir()} == {'metadata.json', 'extension.js', 'focusState.js', 'protocol.js', 'LICENSE'}
        assert json.loads((extension/'metadata.json').read_text())['shell-version'] == ['50']
        for method in ('telex', 'vni', 'simple-telex1', 'simple-telex2'):
            assert (root/f'usr/share/fcitx5/inputmethod/openkey-{method}.conf').is_file()
        for desktop in ('usr/share/applications/org.openkey.Settings.desktop', 'etc/xdg/autostart/openkey-tray.desktop'):
            run('desktop-file-validate', str(root/desktop))
        environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QT_IM_MODULE='compose')
        run(str(root/'usr/bin/openkey-settings'), '--smoke-test', env=environment, timeout=15)
        run('dbus-run-session', '--', str(build/'Sources/OpenKey/linux/fcitx5/openkey_fcitx5_integration_tests'),
            str(root/'usr'), libdir, timeout=25)
        run(str(build/'Sources/OpenKey/linux/fcitx5/openkey_direct_tests'), str(addons[0]), timeout=20,
            env=dict(os.environ, XDG_CONFIG_HOME=temporary+'/config', XDG_DATA_HOME=temporary+'/data', XDG_CACHE_HOME=temporary+'/cache'))
        run(str(root/'usr/bin/openkey-compat-report'), '--help')
        assert (control/'conffiles').read_text().strip() == '/etc/xdg/autostart/openkey-tray.desktop'
        # Rootless payload lifecycle in a private dpkg database. There are no
        # maintainer scripts/triggers and no desktop services are started.
        private = Path(temporary)/'dpkg-root'
        database = private/'var/lib/dpkg'
        (database/'updates').mkdir(parents=True)
        (database/'status').write_text('')
        fixture = private/'home/test/.config/openkey/preserved.ini'
        fixture.parent.mkdir(parents=True)
        fixture.write_text('preserve-user-configuration')
        def dpkg(*args):
            return run('dpkg', '--force-not-root', '--root='+str(private), '--admindir='+str(database), *args)
        shutil.copytree(control, root/'DEBIAN')
        metadata = root/'DEBIAN/control'
        metadata.write_text(metadata.read_text().replace('Version: 3.1.0~beta.1-1\n', 'Version: 3.1.0~beta.1-1+qa1\n'))
        upgrade = Path(temporary)/'upgrade.deb'
        run('dpkg-deb', '--build', str(root), str(upgrade))
        for label, action, value in [('base', '--unpack', str(deb)), ('reinstall', '--unpack', str(deb)),
                                     ('upgrade', '--unpack', str(upgrade)), ('remove', '--remove', 'openkey'),
                                     ('restore', '--unpack', str(deb)), ('purge', '--purge', 'openkey')]:
            dpkg(action, value)
            assert fixture.read_text() == 'preserve-user-configuration'
            installed = action == '--unpack'
            assert (private/'usr/bin/openkey-settings').exists() == installed, label
            assert (private/'usr/bin/openkey-service').exists() == installed, label
            assert (private/'usr'/libdir/'fcitx5/openkey.so').exists() == installed, label
        assert not (private/'etc/xdg/autostart/openkey-tray.desktop').exists()
        print('Private dpkg payload lifecycle: base/reinstall/upgrade/remove/restore/purge PASS')
    print('Complete beta DEB: dependencies, conflicts, checksum, payload, desktop entries, QML, addon integration and Direct PASS')


if __name__ == '__main__':
    main()
