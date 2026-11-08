#!/usr/bin/env python3
"""Summarize OpenKey Direct diagnostics without exporting raw journal messages."""
import argparse
from collections import Counter, deque
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile

MAX_BYTES = 8 * 1024 * 1024
MAX_LINE = 16 * 1024
MAX_ROWS = 256
MAX_RECENT = 32
JOURNAL_LINES = 2000
CASES = ('unspecified', 'chrome-url', 'chrome-web', 'vscode-editor',
         'vscode-terminal', 'gtk', 'qt')
REASONS = frozenset('''context-ready pending-boundary fallback:commit-only-terminal
fallback:preedit-terminal ibus:enter-native-prime-armed ibus:post-enter-native-unconfirmed
ibus:native-start-unconfirmed ibus:word-prime-native ibus:post-enter-append-native
fallback:preedit-unreliable-surrounding fallback:preedit-word-in-progress
fallback:invalid-surrounding-text fallback:no-surrounding-capability
fallback:passthrough-no-preedit-capability native:passthrough native:url-completion-edit
edit-aborted bypass:unconfirmed-edit recovery:context-changed context-invalidated
context-reset context-wait boundary:enter-passthrough boundary:shortcut-passthrough boundary:nav-passthrough
direct:selection-start direct:ready direct:prime-native boundary-restored
bypass:sensitive-field'''.split())
CAUSES = frozenset('''none no-surrounding-capability terminal-context email-context
unreliable-program awaiting-surrounding-capability awaiting-surrounding-text
invalid-surrounding raw-word-in-progress awaiting-surrounding-update snapshot-mismatch
verified-prefix-selection surrounding-mismatch surrounding-invalid late-known-echo
ack-timeout context-changed capability-withdrawn key-snapshot-not-current
queue-overflow prepare-rejected'''.split())
REFERENCES = frozenset(('none', 'confirmed', 'expected', 'native-expected'))
SNAPSHOTS = frozenset(('invalid', 'untracked', 'url-completion', 'expected', 'unexpected',
                       'stale', 'initial', 'current', 'changed'))
FRONTENDS = frozenset(('ibus', 'dbus', 'wayland', 'x11', 'xim'))
FLAGS = ('pending', 'notified', 'comparable', 'text_changed', 'cursor_changed',
         'anchor_changed', 'ack_requires_update', 'native_waiting_update',
         'native_raw_word', 'native_url_completion')
# Require the fixed field order emitted by OpenKeyAddon::diagnose. No arbitrary
# text or identifiers from the line are copied into the resulting report.
HEADER = re.compile(r'OpenKey Direct: reason=([^\s]+) app=(.*?) frontend=([^\s]+) '
                    r'capabilities=([0-9]{1,20}) cause=([^\s]+) snapshot=([^\s]+)(?: |$)')
FLAG = re.compile(r'(?:^| )([a-z_]+)=([01])(?= |$)')
REFERENCE = re.compile(r'(?:^| )reference=([^\s]+)(?= |$)')


def app_family(value):
    return {'google-chrome': 'chrome', 'google-chrome-stable': 'chrome',
            'chromium': 'chrome', 'chromium-browser': 'chrome',
            'code': 'vscode', 'code-insiders': 'vscode', 'Code': 'vscode',
            'gnome-shell': 'gnome-bridge', 'firefox': 'firefox',
            'org.mozilla.firefox': 'firefox'}.get(value, 'other')


def summarize(data, case='unspecified', limited=False):
    rows = Counter()
    matched = rejected = dropped = 0
    flags = Counter()
    recent = deque(maxlen=MAX_RECENT)
    for raw in data.splitlines():
        if len(raw) > MAX_LINE:
            rejected += 1
            continue
        try:
            line = raw.decode('utf-8', errors='strict')
            if line.lstrip().startswith('{'):
                record = json.loads(line)
                line = record.get('MESSAGE', '') if isinstance(record, dict) else ''
            if not isinstance(line, str):
                continue
        except (ValueError, UnicodeError):
            rejected += 1
            continue
        match = HEADER.search(line)
        if not match:
            continue
        reason, app, frontend, caps, cause, snapshot = match.groups()
        if int(caps) > 2**64 - 1:
            rejected += 1
            continue
        matched += 1
        row = (app_family(app), frontend if frontend in FRONTENDS else 'other',
               reason if reason in REASONS else 'other', cause if cause in CAUSES else 'other',
               snapshot if snapshot in SNAPSHOTS else 'other', int(caps))
        if row in rows or len(rows) < MAX_ROWS:
            rows[row] += 1
        else:
            dropped += 1
        tail = line[match.end():]
        state = {key: bool(int(value)) for key, value in FLAG.findall(tail) if key in FLAGS}
        for key, value in state.items():
            if value:
                flags[key] += 1
        reference = REFERENCE.search(tail)
        reference = reference.group(1) if reference else None
        recent.append({
            'record': matched, 'app_family': row[0], 'frontend': row[1],
            'reason': row[2], 'cause': row[3], 'snapshot': row[4],
            'reference': 'unavailable' if reference is None else reference if reference in REFERENCES else 'other',
            'flags': state,
        })
    findings = []
    if not matched:
        findings.append('no-diagnostics-in-window')
    if any(row[0] == 'gnome-bridge' for row in rows):
        findings.append('bridge-does-not-identify-application')
    if any(row[3] in ('no-surrounding-capability', 'awaiting-surrounding-capability') or
           row[2] == 'fallback:no-surrounding-capability' for row in rows):
        findings.append('surrounding-capability-missing-at-observation')
    if any(row[3] in ('awaiting-surrounding-update', 'awaiting-surrounding-text',
                       'snapshot-mismatch', 'surrounding-mismatch', 'surrounding-invalid',
                       'key-snapshot-not-current', 'ack-timeout') for row in rows):
        findings.append('surrounding-synchronization-needs-review')
    if any(row[3] == 'capability-withdrawn' for row in rows):
        findings.append('surrounding-capability-withdrawn-at-observation')
    if any(row[2] == 'edit-aborted' for row in rows):
        findings.append('direct-edit-aborted-observed')
    if any(row[2] == 'context-reset' for row in rows):
        findings.append('context-reset-observed')
    if any(row[3] in ('unreliable-program', 'terminal-context') for row in rows):
        findings.append('frontend-policy-restricts-direct-editing')
    return {
        'format': 'openkey-linux-compat-report', 'version': 2,
        'declared_case': case, 'desktop_result': 'not-assessed',
        'diagnostic_records': matched, 'rejected_records': rejected,
        'input_limited': limited, 'dropped_groups': dropped,
        'findings': findings, 'true_flag_counts': dict(sorted(flags.items())),
        'sequence_scope': 'input-record-order-may-interleave-contexts',
        'recent_records': list(recent), 'omitted_recent_records': matched - len(recent),
        'groups': [dict(zip(('app_family', 'frontend', 'reason', 'cause', 'snapshot',
                             'capabilities', 'records'), (*row, count)))
                   for row, count in sorted(rows.items())],
        'notes': ['Counts describe deduplicated diagnostics, not keystrokes or failures.',
                  'Declared case is supplied by the tester, not detected from GNOME bridge.',
                  'Recent record order is not a per-context trace and contains no timing information.',
                  'Change flags are comparable only when comparable is explicitly true.',
                  'No log record proves successful desktop typing.'],
    }


def read_log(path):
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ValueError('regular-file-required')
        return stream.read(MAX_BYTES + 1)


def journal_data(minutes):
    # A temporary stream avoids buffering unbounded subprocess output in RAM.
    # It is never exported; only allowlisted aggregates leave this function.
    with tempfile.TemporaryFile() as stream, tempfile.TemporaryFile() as errors:
        result = subprocess.run(['journalctl', '--user', '--no-pager', '--output=json',
                        '--since', f'{minutes} minutes ago', '--lines', str(JOURNAL_LINES),
                        '--grep', 'OpenKey Direct:'], stdout=stream,
                       stderr=errors, check=False, timeout=15)
        # journalctl --grep returns 1 with empty stdout/stderr for no matches.
        # Any diagnostic on stderr with a failing status remains an error.
        empty_match = result.returncode == 1 and stream.tell() == 0 and errors.tell() == 0
        if result.returncode != 0 and not empty_match:
            raise ValueError('journal-unavailable')
        stream.seek(0)
        return stream.read(MAX_BYTES + 1)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--journal-file', type=Path, help='Read a local regular text/JSONL log instead of journalctl')
    parser.add_argument('--since-minutes', type=int, choices=range(1, 1441), default=10, metavar='1..1440')
    parser.add_argument('--case', choices=CASES, default='unspecified')
    parser.add_argument('--output', type=Path, help='Create a new report with mode 0600; never overwrite')
    args = parser.parse_args(argv)
    try:
        data = read_log(args.journal_file) if args.journal_file else journal_data(args.since_minutes)
        limited = len(data) > MAX_BYTES
        # Never parse a record cut by the byte limit.
        if limited:
            data = data[:MAX_BYTES].rsplit(b'\n', 1)[0] if b'\n' in data[:MAX_BYTES] else b''
        report = summarize(data, args.case, limited)
        if not args.journal_file and data.count(b'\n') >= JOURNAL_LINES:
            report['input_limited'] = True
        encoded = (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
        if args.output:
            fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC, 0o600)
            with os.fdopen(fd, 'wb') as out:
                out.write(encoded)
        else:
            sys.stdout.write(encoded.decode('utf-8'))
    except (OSError, ValueError, subprocess.SubprocessError):
        # Do not echo pathnames, raw messages, stderr or exception details.
        print('Cannot create report: check journal access, local log, timeout, and a new output path.', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
