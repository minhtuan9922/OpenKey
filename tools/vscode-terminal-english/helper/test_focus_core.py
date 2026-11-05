import asyncio
import unittest
from unittest.mock import patch

from focus_core import Policy, State, classify
from atspi_focus import command, read_state


VI = State(True, "openkey-telex")
EN = State(False, "openkey-telex")
VNI = State(True, "openkey-vni")


class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.p = Policy()

    def enter(self, original=VI):
        self.assertIsNone(self.p.observe("a", "editor", original))
        desired = self.p.observe("a", "terminal", original)
        self.p.confirmed("terminal", State(False, original.method))
        return desired

    def test_telex_round_trip(self):
        self.assertEqual(self.enter(), EN)
        self.assertEqual(self.p.observe("a", "editor", EN), VI)

    def test_vni_round_trip(self):
        self.enter(VNI)
        self.assertEqual(self.p.observe("a", "editor", EN), VNI)

    def test_english_stays_english(self):
        self.assertIsNone(self.enter(EN))
        self.assertIsNone(self.p.observe("a", "editor", EN))

    def test_initial_terminal_does_nothing(self):
        self.assertIsNone(self.p.observe("a", "terminal", VI))

    def test_unrelated_im_untouched(self):
        self.assertIsNone(self.enter(State(True, "pinyin")))
        self.assertIsNone(self.p.observe("a", "editor", EN))

    def test_terminal_unrelated_im_untouched(self):
        self.p.observe("a", "editor", VI)
        self.assertIsNone(self.p.observe("a", "terminal", State(True, "pinyin")))

    def test_manual_terminal_switch_disables_restore(self):
        self.enter()
        self.p.observe("a", "terminal", VNI)
        self.assertIsNone(self.p.observe("a", "editor", EN))

    def test_editor_active_not_overwritten(self):
        self.enter()
        self.assertIsNone(self.p.observe("a", "editor", VNI))

    def test_unknown_forgets_restore(self):
        self.enter()
        self.p.observe(None, "unknown", EN)
        self.assertIsNone(self.p.observe("a", "editor", EN))

    def test_other_window_forgets_restore(self):
        self.enter()
        self.assertIsNone(self.p.observe("b", "editor", EN))

    def test_repeat_terminal_retains_restore(self):
        self.enter()
        for _ in range(5):
            self.assertIsNone(self.p.observe("a", "terminal", EN))
        self.assertEqual(self.p.observe("a", "editor", EN), VI)

    def test_repeated_round_trips(self):
        for _ in range(3):
            self.assertEqual(self.enter(), EN)
            self.assertEqual(self.p.observe("a", "editor", EN), VI)
            self.p.confirmed("editor", VI)

    def test_reset_forgets(self):
        self.enter()
        self.p.reset()
        self.assertIsNone(self.p.observe("a", "editor", EN))

    def test_terminal_context_already_english(self):
        self.p.observe("a", "editor", VI)
        self.assertIsNone(self.p.observe("a", "terminal", EN))
        self.assertEqual(self.p.observe("a", "editor", EN), VI)

    def test_classifier(self):
        self.assertEqual(classify("text", {"class": "xterm-helper-textarea"}), "terminal")
        self.assertEqual(classify("terminal", {}), "terminal")
        self.assertEqual(classify("text", {"class": "inputarea monaco-mouse-cursor-text"}), "editor")

    def test_generic_inputs_and_names_rejected(self):
        for role, attrs in [("text", {}), ("entry", {"tag": "textarea"}),
                            ("text", {"name": "Terminal input"}),
                            ("text", {"class": "inputarea"})]:
            self.assertEqual(classify(role, attrs), "unknown")


class BridgeTests(unittest.IsolatedAsyncioTestCase):
    async def test_stable_state(self):
        async def fake(args, valid):
            return "openkey-telex" if args else "2"
        with patch("atspi_focus.command", fake):
            self.assertEqual(await read_state(lambda: True), VI)

    async def test_unstable_rejected(self):
        replies = iter(("2", "openkey-telex", "1"))
        async def fake(args, valid):
            return next(replies)
        with patch("atspi_focus.command", fake):
            with self.assertRaises(RuntimeError):
                await read_state(lambda: True)

    async def test_no_launch_without_focus(self):
        with patch("asyncio.create_subprocess_exec") as spawn:
            with self.assertRaises(RuntimeError):
                await command(["-c"], lambda: False)
            spawn.assert_not_called()

    async def test_shell_free_checked_binary(self):
        class Process:
            returncode = 0
            async def communicate(self):
                return b"2\n", None
        async def spawn(*args, **kwargs):
            self.assertEqual(args, ("/usr/bin/fcitx5-remote", "--check", "-c"))
            self.assertNotIn("shell", kwargs)
            return Process()
        with patch("asyncio.create_subprocess_exec", spawn):
            self.assertEqual(await command(["-c"], lambda: True), "2")

    async def test_cancel_kills_child(self):
        ready = asyncio.Event()
        class Process:
            returncode = None
            killed = False
            async def communicate(self):
                ready.set()
                await asyncio.Event().wait()
            def kill(self):
                self.killed = True
            async def wait(self):
                self.returncode = -9
        proc = Process()
        async def spawn(*args, **kwargs):
            return proc
        with patch("asyncio.create_subprocess_exec", spawn):
            task = asyncio.create_task(command([], lambda: True))
            await ready.wait()
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task
        self.assertTrue(proc.killed)


if __name__ == "__main__":
    unittest.main()
