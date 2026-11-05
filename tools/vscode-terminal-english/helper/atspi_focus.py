#!/usr/bin/python3
"""Opt-in AT-SPI focus helper. Default is a read-only probe; no text APIs."""
import argparse
import asyncio
import fcntl
import json
import os
from pathlib import Path
import signal
import sys

from focus_core import Policy, State, classify


def emit(status, **fields):
    print(json.dumps({"status": status, **fields}), flush=True)


async def command(args, valid):
    if not valid():
        raise RuntimeError("focus changed")
    proc = await asyncio.create_subprocess_exec(
        "/usr/bin/fcitx5-remote", "--check", *args,
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
        limit=2048,
    )
    try:
        output, _ = await asyncio.wait_for(proc.communicate(), 1.2)
        if proc.returncode or len(output) > 1024 or not valid():
            raise RuntimeError("command not confirmed")
        return output.decode("ascii").strip()
    finally:
        if proc.returncode is None:
            proc.kill()
            await proc.wait()


async def read_state(valid):
    first = await command([], valid)
    method = await command(["-n"], valid)
    last = await command([], valid)
    if first != last or first not in ("1", "2") or not method or len(method) > 128:
        raise RuntimeError("unstable state")
    if not all(c.isalnum() or c in "_-" for c in method):
        raise RuntimeError("invalid method")
    return State(first == "2", method)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, help="AT-SPI application PID of local VS Code")
    parser.add_argument("--list", action="store_true", help="List local VS Code AT-SPI PIDs, without names/text")
    parser.add_argument("--switch", action="store_true", help="Allow Fcitx5 switching (default: probe only)")
    args = parser.parse_args()
    if args.list:
        try:
            import gi
            gi.require_version("Atspi", "2.0")
            from gi.repository import Atspi
            Atspi.set_timeout(250, 500)
            desktop = Atspi.get_desktop(0)
            found = set()
            for index in range(desktop.get_child_count()):
                app = desktop.get_child_at_index(index)
                pid = app.get_process_id()
                try:
                    if Path(f"/proc/{pid}/exe").resolve(strict=True) == Path("/usr/share/code/code"):
                        found.add(pid)
                except OSError:
                    continue
            for pid in sorted(found):
                emit("application", pid=pid)
            if not found:
                emit("no-code-application")
            return 0
        except Exception:
            emit("unavailable")
            return 1
    if args.pid is None or args.pid <= 1:
        parser.error("PID must be greater than 1")
    try:
        executable = Path(f"/proc/{args.pid}/exe").resolve(strict=True)
        # No automatic matching of other Electron apps or remote extension hosts.
        if executable != Path("/usr/share/code/code"):
            parser.error("Only local /usr/share/code/code is supported in this prototype")
        process_start = Path(f"/proc/{args.pid}/stat").read_text().rsplit(")", 1)[1].split()[19]
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi, GLib
    except (OSError, ImportError, ValueError):
        emit("unavailable")
        return 1

    # Single helper per desktop session, including probe, avoids competing writers.
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if not runtime or os.stat(runtime).st_uid != os.getuid():
        emit("runtime-unavailable")
        return 1
    lock_fd = os.open(str(Path(runtime) / "openkey-atspi-focus.lock"),
                      os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        os.close(lock_fd)
        emit("already-running")
        return 1
    Atspi.set_timeout(250, 500)
    policy = Policy()
    current = None
    generation = 0
    stopped = False
    task = None

    def inspect(source):
        # Read only role, selected DOM attributes, process and focus states.
        # Do not call get_name(), get_description(), Text or Value interfaces.
        if source.get_process_id() != args.pid:
            return None
        if not source.get_state_set().contains(Atspi.StateType.FOCUSED):
            return None
        region = classify(source.get_role_name(), source.get_attributes())
        node = source
        for _ in range(32):
            if node is None:
                break
            if node.get_role() == Atspi.Role.FRAME:
                if node.get_state_set().contains(Atspi.StateType.ACTIVE):
                    return (source, node, region)
                break
            node = node.get_parent()
        return None

    def valid(token, observation):
        if stopped or token != generation or observation is not current:
            return False
        try:
            stat = Path(f"/proc/{args.pid}/stat").read_text().rsplit(")", 1)[1].split()[19]
            if stat != process_start:
                return False
            checked = inspect(observation[0])
            return checked is not None and checked[1:] == observation[1:]
        except Exception:
            return False

    async def apply(token, observation):
        try:
            await asyncio.sleep(0.12)
            guard = lambda: valid(token, observation)
            if not guard():
                policy.reset()
                return
            state = await read_state(guard)
            desired = policy.observe(observation[1], observation[2], state)
            if desired is None:
                return
            if desired.active:
                # Policy only returns allowlisted OpenKey methods.
                await command(["-s", desired.method], guard)
                await command(["-o"], guard)
            else:
                await command(["-c"], guard)
            actual = await read_state(guard)
            if actual.active != desired.active or (desired.active and actual.method != desired.method):
                raise RuntimeError("readback mismatch")
            policy.confirmed(observation[2], actual)
            emit("switched", region=observation[2], active=actual.active)
        except asyncio.CancelledError:
            policy.reset()
            raise
        except Exception:
            policy.reset()
            emit("switch-failed")

    def event(event, *_):
        nonlocal generation, current, task
        try:
            # Focus loss is checked live by guard. Invalidate immediately on
            # external window deactivation; never restore in another app.
            if event.type.startswith("window:deactivate"):
                if current is None or event.source != current[1]:
                    return
                observation = None
            elif not event.detail1:
                return
            else:
                observation = inspect(event.source)
        except Exception:
            observation = None
        if observation is not None and current is not None and observation == current:
            return
        generation += 1
        current = observation
        if task:
            task.cancel()
        region = observation[2] if observation else "unknown"
        emit("focus", region=region, pid=args.pid)
        if region == "unknown":
            policy.reset()
        # Work is scheduled in the async loop, never inside a D-Bus callback.

    async def run():
        nonlocal task, stopped
        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, lambda: stop())
        listener = Atspi.EventListener.new(event, None)
        for kind in ("object:state-changed:focused", "window:deactivate"):
            if not listener.register(kind):
                raise RuntimeError("listener registration failed")
        emit("ready", mode="switch" if args.switch else "probe", pid=args.pid)
        handled = -1
        tick = loop.time()
        context = GLib.MainContext.default()
        try:
            while not stopped:
                # Bound each GLib batch to keep cancellation responsive.
                for _ in range(32):
                    if not context.pending():
                        break
                    context.iteration(False)
                now = loop.time()
                if args.switch and current and current[2] != "unknown":
                    if (generation != handled or now - tick >= 0.5) and (task is None or task.done()):
                        if task is not None:
                            await asyncio.gather(task, return_exceptions=True)
                        handled = generation
                        tick = now
                        task = asyncio.create_task(apply(generation, current))
                elif task and task.done():
                    await asyncio.gather(task, return_exceptions=True)
                    task = None
                await asyncio.sleep(0.02)
        finally:
            stopped = True
            if task:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)
            for kind in ("object:state-changed:focused", "window:deactivate"):
                listener.deregister(kind)
            policy.reset()
            # No exit-time restoration: another app may now own the focus.

    def stop():
        nonlocal stopped
        stopped = True

    try:
        asyncio.run(run())
    except Exception:
        emit("helper-failed")
        return 1
    finally:
        os.close(lock_fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
