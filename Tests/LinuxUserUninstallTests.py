"""Exercise user-local removal against fixtures; no real session command runs."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

source = Path(__file__).resolve().parents[1]/'scripts/uninstall-linux-user.py'
spec = importlib.util.spec_from_file_location('openkey_uninstall', source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class RemovalTests(unittest.TestCase):
    def test_backup_preserve_and_remove(self):
        with tempfile.TemporaryDirectory() as temporary:
            home = Path(temporary)
            for name in ('.local/bin/openkey-settings', '.config/openkey/ui.ini', '.config/fcitx5/conf/openkey.conf',
                         '.config/fcitx5/profile', '.config/other-app/settings'):
                path = home/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with patch.object(Path, 'home', return_value=home), patch('sys.argv', ['uninstall', '--apply']), \
                    patch.object(module.subprocess, 'run') as command, patch.object(module.os, 'kill'), \
                    patch.object(module.time, 'sleep'), patch.object(module.shutil, 'which', return_value='/mock/tool'):
                module.main()
                self.assertTrue(command.called)
            self.assertFalse((home/'.local/bin/openkey-settings').exists())
            self.assertFalse((home/'.config/openkey').exists())
            self.assertEqual((home/'.config/fcitx5/profile').read_text(), '.config/fcitx5/profile')
            self.assertTrue((home/'.config/other-app/settings').exists())
            manifests = list(home.glob('.local/state/openkey-uninstall/*/manifest.json'))
            self.assertEqual(len(manifests), 1)
            self.assertTrue((manifests[0].parent/'.local/bin/openkey-settings').is_file())
            self.assertTrue((manifests[0].parent/'.config/openkey/ui.ini').is_file())

    def test_redirected_parent_rejected_before_session_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            home = Path(temporary)/'home'; home.mkdir()
            elsewhere = Path(temporary)/'elsewhere'; elsewhere.mkdir()
            (elsewhere/'openkey-settings').write_text('preserve')
            (home/'.local').mkdir(); (home/'.local/bin').symlink_to(elsewhere)
            with patch.object(Path, 'home', return_value=home), patch('sys.argv', ['uninstall', '--apply']), \
                    patch.object(module.subprocess, 'run') as command:
                with self.assertRaises(RuntimeError): module.main()
                command.assert_not_called()
            self.assertEqual((elsewhere/'openkey-settings').read_text(), 'preserve')


if __name__ == '__main__':
    unittest.main()
