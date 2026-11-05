"""Focus policy, independent of GI. Never reads editor/terminal contents."""
from dataclasses import dataclass


@dataclass(frozen=True)
class State:
    active: bool
    method: str


OPENKEY = frozenset(("openkey-telex", "openkey-vni"))


def classify(role, attributes):
    """Only explicit DOM classes or the native terminal role are accepted.

    Names/descriptions may contain filenames or terminal output: do not read them.
    A plain textarea/text role is NOT evidence of a code editor.
    """
    classes = set(attributes.get("class", "").split())
    if role == "terminal" or "xterm-helper-textarea" in classes:
        return "terminal"
    if "inputarea" in classes and "monaco-mouse-cursor-text" in classes:
        return "editor"
    return "unknown"


class Policy:
    def __init__(self):
        self.reset()

    def reset(self):
        self.window = None
        self.region = "unknown"
        self.editor = None
        self.restore = None
        self.terminal = None

    def observe(self, window, region, state):
        """Return a desired state, not a focus operation. Caller must guard I/O.

        An editor observation is required before entering a terminal. Window
        changes and unknown widgets discard all restoration history.
        """
        if region not in ("editor", "terminal") or window is None:
            self.reset()
            return None
        if self.window != window:
            self.reset()
            self.window = window
        previous = self.region
        self.region = region
        if region == "terminal":
            if previous == "editor":
                self.restore = self.editor
                self.editor = None
                if self.restore and self.restore.active and self.restore.method not in OPENKEY:
                    self.restore = None
                    return None
                # Do not turn off an unrelated IM in the terminal either.
                if state.active and state.method not in OPENKEY:
                    self.restore = None
                    return None
                self.terminal = state
                if self.restore and state.active:
                    return State(False, state.method)
            elif self.terminal is not None and state != self.terminal:
                # User/context changed the input state while in the terminal.
                self.restore = None
                self.terminal = state
            return None
        restore = self.restore
        self.restore = None
        self.terminal = None
        self.editor = state
        if previous == "terminal" and restore and restore.active and restore.method in OPENKEY:
            if not state.active:
                return restore
        return None

    def confirmed(self, region, state):
        if region == "editor":
            self.editor = state
        else:
            self.terminal = state
