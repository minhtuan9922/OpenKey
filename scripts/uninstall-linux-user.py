#!/usr/bin/env python3
"""Back up and remove only this account's OpenKey installation. Never removes Fcitx5."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Perform removal after backing up the exact paths listed')
    args = parser.parse_args()
    home = Path.home()
    paths = [home/path for path in (
        '.local/bin/openkey-settings', '.local/bin/openkey-service', '.local/bin/openkey-tray-autostart',
        '.local/bin/openkey-tray-systemd', '.local/share/dbus-1/services/org.openkey.Settings.service',
        '.local/share/applications/org.openkey.Settings.desktop', '.config/autostart/openkey-tray.desktop',
        '.config/autostart/openkey-tray.desktop.before-systemd', '.config/systemd/user/openkey-tray.service',
        '.config/systemd/user/graphical-session.target.wants/openkey-tray.service',
        '.local/share/gnome-shell/extensions/openkey-app-identity@openkey.local',
        '.config/openkey', '.config/fcitx5/conf/openkey.conf', '.config/fcitx5/conf/openkey-app-memory.conf')]
    paths = [p for p in paths if p.exists() or p.is_symlink()]
    for path in paths:
        if path.parent.resolve() != path.parent:
            raise RuntimeError('Refusing redirected parent: ' + str(path))
    if not args.apply:
        print('\n'.join(str(p) for p in paths))
        return
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = home/'.local/state/openkey-uninstall'/stamp
    backup.mkdir(parents=True, mode=0o700)
    os.chmod(backup, 0o700)
    # Complete the backup before stopping or removing anything.
    for path in paths + [home/'.config/fcitx5/profile', home/'.xinputrc']:
        if not (path.exists() or path.is_symlink()):
            continue
        target = backup/path.relative_to(home)
        target.parent.mkdir(parents=True, exist_ok=True)
        if path.is_symlink(): target.symlink_to(os.readlink(path))
        elif path.is_dir(): shutil.copytree(path, target, symlinks=True)
        else: shutil.copy2(path, target)
    (backup/'manifest.json').write_text(json.dumps({'remove': [str(p) for p in paths],
        'preserve_shared': ['~/.config/fcitx5/profile', '~/.xinputrc']}, indent=2))
    def command(*arguments):
        if not shutil.which(arguments[0]): return
        try:
            return subprocess.run(arguments, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
        except subprocess.TimeoutExpired:
            print('Timed out:', arguments[0])
    command('fcitx5-remote', '-s', 'keyboard-us')
    command('gdbus', 'call', '--session', '--dest', 'org.fcitx.Fcitx5', '--object-path', '/controller',
            '--method', 'org.fcitx.Fcitx.Controller1.SetConfig', 'fcitx://config/addon/openkey',
            "<{'RememberInputMethodPerApplication': <'False'>, 'UseGnomeApplicationIdentity': <'False'>}>")
    command('gnome-extensions', 'disable', 'openkey-app-identity@openkey.local')
    command('systemctl', '--user', 'disable', '--now', 'openkey-tray.service')
    # Only processes executing one of the exact OpenKey UI/tray binaries.
    binaries = {str(home/'.local/bin/openkey-settings'), str(home/'.local/bin/openkey-service'),
                '/usr/bin/openkey-settings', '/usr/bin/openkey-service'}
    for process in Path('/proc').iterdir():
        if not process.name.isdigit(): continue
        try:
            if process.stat().st_uid == os.getuid() and os.readlink(process/'exe').removesuffix(' (deleted)') in binaries:
                os.kill(int(process.name), signal.SIGTERM)
        except (OSError, ProcessLookupError):
            pass
    time.sleep(.2)
    for path in paths:
        # Reject redirected parent directories; the leaf symlink itself is removable.
        if path.parent.resolve() != path.parent:
            raise RuntimeError('Refusing redirected parent: ' + str(path))
        if path.is_symlink(): path.unlink()
        elif path.is_dir(): shutil.rmtree(path)
        elif path.exists(): path.unlink()
    command('systemctl', '--user', 'daemon-reload')
    print('Removed user-local OpenKey; backup:', backup)
    print('System DEBs require: sudo apt remove fcitx5-openkey openkey-settings')


if __name__ == '__main__':
    main()
