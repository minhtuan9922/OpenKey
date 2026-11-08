#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('compat_report', Path(__file__).resolve().parents[1] / 'scripts/linux-compat-report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


def diagnostic(app='google-chrome', cause='awaiting-surrounding-update', reason='native:passthrough', **flags):
    tail = ' '.join(f'{key}={value}' for key, value in flags.items())
    return (f'I2026-11-08 file.cpp:1] OpenKey Direct: reason={reason} app={app} '
            f'frontend=ibus capabilities=1048658 cause={cause} snapshot=unexpected {tail}').encode()


class CompatReportTests(unittest.TestCase):
    def run_main(self, args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = report.main(args)
        return code, out.getvalue(), err.getvalue()

    def test_json_and_raw_aggregate_without_content(self):
        line = diagnostic(native_waiting_update=1, native_raw_word=0)
        data = line + b'\n' + json.dumps({'MESSAGE': line.decode(), '_HOSTNAME': 'secret-host',
            '_CMDLINE': 'secret-command'}).encode() + b'\n'
        result = report.summarize(data, 'chrome-url')
        self.assertEqual(result['diagnostic_records'], 2)
        self.assertEqual(result['groups'][0]['records'], 2)
        self.assertEqual(result['true_flag_counts'], {'native_waiting_update': 2})
        self.assertEqual(result['desktop_result'], 'not-assessed')
        self.assertIn('surrounding-synchronization-needs-review', result['findings'])
        for secret in ('secret-host', 'secret-command', 'file.cpp', '2026-11-08'):
            self.assertNotIn(secret, json.dumps(result))

    def test_unknown_values_and_identifiers_never_exported(self):
        line = diagnostic(app='/home/private-browser secret-url', cause='secret-cause', reason='secret-reason')
        result = report.summarize(line.replace(b'snapshot=unexpected', b'snapshot=secret-snapshot') + b' secret_text=secret')
        self.assertEqual(result['groups'][0]['app_family'], 'other')
        self.assertEqual(result['groups'][0]['reason'], 'other')
        self.assertNotIn('secret', json.dumps(result))

    def test_gnome_case_is_declared_not_detected(self):
        result = report.summarize(diagnostic(app='gnome-shell', cause='awaiting-surrounding-capability'), 'vscode-terminal')
        self.assertEqual(result['declared_case'], 'vscode-terminal')
        self.assertEqual(result['groups'][0]['app_family'], 'gnome-bridge')
        self.assertIn('bridge-does-not-identify-application', result['findings'])
        self.assertIn('surrounding-capability-missing-at-observation', result['findings'])

    def test_transient_missing_capability_does_not_assert_permanent_failure(self):
        data = diagnostic(cause='no-surrounding-capability') + b'\n' + diagnostic(cause='none', reason='direct:ready')
        result = report.summarize(data)
        self.assertEqual(result['desktop_result'], 'not-assessed')
        self.assertEqual(result['diagnostic_records'], 2)
        self.assertEqual(len(result['groups']), 2)

    def test_invalid_json_utf8_oversize_and_non_message(self):
        data = b'not a diagnostic\n{broken\n\xff\n' + b'a' * (report.MAX_LINE + 1) + b'\n'
        data += b'{"MESSAGE": ["not a string"]}\n'
        result = report.summarize(data)
        self.assertEqual(result['diagnostic_records'], 0)
        self.assertEqual(result['rejected_records'], 3)
        self.assertEqual(result['findings'], ['no-diagnostics-in-window'])

    def test_numeric_bounds(self):
        result = report.summarize(diagnostic().replace(b'1048658', str(2**64).encode()))
        self.assertEqual(result['diagnostic_records'], 0)
        self.assertEqual(result['rejected_records'], 1)

    def test_abort_causes_are_preserved_for_diagnosis(self):
        for cause in ('capability-withdrawn', 'key-snapshot-not-current', 'queue-overflow',
                      'prepare-rejected', 'ack-timeout', 'surrounding-invalid'):
            with self.subTest(cause=cause):
                result = report.summarize(diagnostic(reason='edit-aborted', cause=cause, pending=1, notified=0))
                self.assertEqual(result['groups'][0]['cause'], cause)
                self.assertEqual(result['recent_records'][0]['cause'], cause)
                self.assertEqual(result['recent_records'][0]['flags'], {'pending': True, 'notified': False})
                self.assertIn('direct-edit-aborted-observed', result['findings'])
                self.assertEqual(result['desktop_result'], 'not-assessed')

    def test_reset_sequence_keeps_reference_and_comparability(self):
        ready = diagnostic(app='gnome-shell', reason='direct:ready', cause='none', comparable=1) + b' reference=confirmed'
        invalid = diagnostic(app='gnome-shell', reason='context-invalidated',
            cause='key-snapshot-not-current', comparable=1, text_changed=1) + b' reference=confirmed'
        reset = diagnostic(app='gnome-shell', reason='context-reset', cause='none',
            comparable=0, text_changed=0) + b' reference=none'
        result = report.summarize(b'\n'.join((ready, invalid, reset)))
        recent = result['recent_records']
        self.assertEqual([event['reason'] for event in recent],
                         ['direct:ready', 'context-invalidated', 'context-reset'])
        self.assertEqual([event['record'] for event in recent], [1, 2, 3])
        self.assertEqual([event['reference'] for event in recent], ['confirmed', 'confirmed', 'none'])
        self.assertIs(recent[-1]['flags']['comparable'], False)
        self.assertIn('context-reset-observed', result['findings'])
        self.assertIn('surrounding-synchronization-needs-review', result['findings'])
        self.assertEqual(result['sequence_scope'], 'input-record-order-may-interleave-contexts')

    def test_recent_records_are_bounded_and_do_not_group_contexts(self):
        data = b'\n'.join(diagnostic(app='gnome-shell' if i % 2 else 'code')
                          for i in range(report.MAX_RECENT + 9))
        result = report.summarize(data)
        self.assertEqual(len(result['recent_records']), report.MAX_RECENT)
        self.assertEqual(result['omitted_recent_records'], 9)
        self.assertEqual(result['recent_records'][0]['record'], 10)
        self.assertNotEqual(result['recent_records'][0]['app_family'], result['recent_records'][1]['app_family'])
        self.assertNotIn('context_id', json.dumps(result))

    def test_unknown_reference_and_unrecognized_flags_are_not_exported(self):
        result = report.summarize(diagnostic() + b' reference=private-reference private_key=1 pending=2')
        self.assertEqual(result['recent_records'][0]['reference'], 'other')
        self.assertEqual(result['recent_records'][0]['flags'], {})
        self.assertNotIn('private', json.dumps(result))

    def test_absent_flags_are_distinct_from_explicit_false(self):
        data = diagnostic(cause='ack-timeout') + b'\n' + diagnostic(cause='ack-timeout', notified=0) + b' reference=none'
        recent = report.summarize(data)['recent_records']
        self.assertNotIn('notified', recent[0]['flags'])
        self.assertIs(recent[1]['flags']['notified'], False)
        self.assertEqual(recent[0]['reference'], 'unavailable')
        self.assertEqual(recent[1]['reference'], 'none')

    def test_group_limit_and_flag_allowlist(self):
        data = b'\n'.join(diagnostic(pending=1, arbitrary=1).replace(b'1048658', str(i).encode())
                          for i in range(report.MAX_ROWS + 8))
        result = report.summarize(data)
        self.assertEqual(len(result['groups']), report.MAX_ROWS)
        self.assertEqual(result['dropped_groups'], 8)
        self.assertEqual(result['true_flag_counts'], {'pending': report.MAX_ROWS + 8})

    def test_output_private_never_overwrites_or_follows_symlink(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / 'source', Path(directory) / 'report'
            source.write_bytes(diagnostic())
            code, stdout, stderr = self.run_main(['--journal-file', str(source), '--output', str(output)])
            self.assertEqual((code, stdout, stderr), (0, '', ''))
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
            before = output.read_bytes()
            code, _, _ = self.run_main(['--journal-file', str(source), '--output', str(output)])
            self.assertEqual(code, 2)
            self.assertEqual(output.read_bytes(), before)
            alias = Path(directory) / 'alias'
            alias.symlink_to(source)
            self.assertEqual(self.run_main(['--journal-file', str(alias)])[0], 2)
            self.assertEqual(self.run_main(['--journal-file', str(source), '--output', str(alias)])[0], 2)
            self.assertEqual(source.read_bytes(), diagnostic())

    def test_fifo_and_missing_path_fail_without_details(self):
        with tempfile.TemporaryDirectory() as directory:
            fifo = Path(directory) / 'secret-path'
            os.mkfifo(fifo)
            for path in (fifo, Path(directory) / 'missing-secret'):
                code, stdout, stderr = self.run_main(['--journal-file', str(path)])
                self.assertEqual((code, stdout), (2, ''))
                self.assertNotIn(directory, stderr)
                self.assertNotIn('secret', stderr)

    def test_byte_limit_discards_partial_record(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            line = diagnostic() + b'\n'
            source.write_bytes(line + b'x' * 1000)
            with patch.object(report, 'MAX_BYTES', len(line) + 20):
                code, out, _ = self.run_main(['--journal-file', str(source)])
            self.assertEqual(code, 0)
            result = json.loads(out)
            self.assertTrue(result['input_limited'])
            self.assertEqual(result['diagnostic_records'], 1)

    def test_journal_invocation_and_failure_are_read_only(self):
        def fake_run(argv, **kwargs):
            self.assertEqual(argv[0], 'journalctl')
            self.assertIn('--user', argv)
            self.assertIn('--grep', argv)
            self.assertEqual(argv[-1], 'OpenKey Direct:')
            self.assertEqual(kwargs['timeout'], 15)
            kwargs['stdout'].write(json.dumps({'MESSAGE': diagnostic().decode()}).encode() + b'\n')
            return subprocess.CompletedProcess(argv, 0)
        with patch.object(report.subprocess, 'run', side_effect=fake_run):
            code, out, _ = self.run_main(['--case', 'chrome-url'])
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(out)['diagnostic_records'], 1)
        with patch.object(report.subprocess, 'run', side_effect=subprocess.TimeoutExpired('private-command', 15)):
            code, out, err = self.run_main([])
        self.assertEqual((code, out), (2, ''))
        self.assertNotIn('private-command', err)

    def test_empty_journal_match_is_a_valid_empty_report(self):
        with patch.object(report.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1)):
            code, out, err = self.run_main([])
        self.assertEqual((code, err), (0, ''))
        self.assertEqual(json.loads(out)['findings'], ['no-diagnostics-in-window'])

    def test_journal_access_failure_is_not_reported_as_no_events(self):
        def denied(argv, **kwargs):
            kwargs['stderr'].write(b'private permission diagnostic')
            return subprocess.CompletedProcess(argv, 1)
        with patch.object(report.subprocess, 'run', side_effect=denied):
            code, out, err = self.run_main([])
        self.assertEqual((code, out), (2, ''))
        self.assertNotIn('private permission', err)

    def test_no_record_does_not_imply_disabled_diagnostics(self):
        result = report.summarize(b'')
        self.assertEqual(result['diagnostic_records'], 0)
        self.assertEqual(result['desktop_result'], 'not-assessed')
        self.assertEqual(result['findings'], ['no-diagnostics-in-window'])


if __name__ == '__main__':
    unittest.main()
