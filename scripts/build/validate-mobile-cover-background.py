"""Observe real Android cover exports; query failures never mean completion."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time


def service_state(output):
    if ('ACTIVITY MANAGER SERVICES' not in output or 'DUMP TIMEOUT' in output
            or 'Permission Denial' in output):
        return None
    return 'org.miacode.android/.ChartExportService' in output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', type=Path, required=True)
    parser.add_argument('--serial', default='emulator-5554')
    parser.add_argument('--destination', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--baseline', type=Path, help='Continue observing the same existing Home test.')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if not output.is_relative_to((root / 'build-devtools').resolve()):
        parser.error('Evidence must stay in build-devtools.')
    if not args.destination.startswith('/sdcard/') or any(c in args.destination for c in "'\n\r"):
        parser.error('Expected an absolute Android public storage path.')
    if not 1 <= args.jobs <= 100 or not 10 <= args.timeout <= 1800:
        parser.error('Invalid job count or observation timeout.')
    output.mkdir(parents=True, exist_ok=True)
    prefix = [str(args.adb.resolve()), '-s', args.serial]
    queries = []

    def call(*command, timeout=15):
        try:
            result = subprocess.run(prefix + list(command), capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            queries.append({'command': list(command), 'timeout': True})
            return None
        if result.returncode:
            queries.append({'command': list(command), 'exitCode': result.returncode,
                            'error': result.stderr.decode('utf-8', errors='replace')})
            return None
        return result.stdout

    def pid():
        data = call('shell', 'pidof', 'org.miacode.android')
        return data.decode().strip() if data is not None else None

    def files():
        data = call('shell', 'find', args.destination, '-type', 'f')
        return sorted(data.decode('utf-8').splitlines()) if data is not None else None

    report = {'P5Accepted': False, 'realArmDeviceVerified': False, 'polls': [], 'queryFailures': queries,
              'destination': args.destination, 'expectedJobs': args.jobs, 'passed': False}
    if args.baseline:
        baseline = json.loads(args.baseline.read_text(encoding='utf-8'))
        if not baseline.get('queuePreparedBeforeHome'):
            raise RuntimeError('The existing queue and Home action must have authoritative evidence.')
        original_pid = baseline['originalPid']
        before = baseline['before']
        report['backgroundEstablishedBy'] = str(args.baseline.resolve())
        report['observationContinuesExistingProcess'] = True
    else:
        original_pid = pid()
        before = files()
        state_data = call('shell', 'dumpsys', 'activity', 'services', 'org.miacode.android')
        if not original_pid or before is None or state_data is None or service_state(state_data.decode()) is not False:
            raise RuntimeError('A running app and an authoritative idle export service are required.')
        existing_log = call('logcat', '-d', '--pid', original_pid, '-v', 'threadtime', timeout=30)
        if existing_log is None:
            raise RuntimeError('Cannot preserve the existing app log.')
        (output / 'before-start-logcat.txt').write_bytes(existing_log)
        if call('logcat', '-c') is None:
            raise RuntimeError('Cannot establish a fresh queue log.')
        subprocess.run([sys.executable, str(root / 'scripts/build/android-ui-qa.py'), 'click',
                        '\u5f00\u59cb\u5bfc\u51fa', '--adb', str(args.adb.resolve()),
                        '--serial', args.serial], check=True, timeout=45)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            data = call('logcat', '-d', '--pid', original_pid, '-v', 'brief')
            if data and re.search(r'queue prepared\s+' + str(args.jobs) + r'\b', data.decode(errors='replace')):
                break
            time.sleep(.5)
        else:
            raise RuntimeError('Expected queue was not observed; Home was not sent.')
        if pid() != original_pid or call('shell', 'input', 'keyevent', 'HOME') is None:
            raise RuntimeError('The same process and successful Home command are required.')
        report['queuePreparedBeforeHome'] = True
        report['homeActionObserved'] = True
    report.update(originalPid=original_pid, before=before)
    start = time.monotonic()
    evidence = output / 'verification.json'
    while time.monotonic() - start < args.timeout:
        current_files = files()
        state_data = call('shell', 'dumpsys', 'activity', 'services', 'org.miacode.android')
        observed_pid = pid()
        service = None if state_data is None else service_state(state_data.decode(errors='replace'))
        if state_data is not None:
            (output / ('service-' + str(len(report['polls'])) + '.txt')).write_bytes(state_data)
        added = None if current_files is None else sorted(set(current_files) - set(before))
        item = {'elapsedSeconds': round(time.monotonic()-start, 3), 'pid': observed_pid,
                'serviceActive': service, 'newFiles': added}
        report['polls'].append(item)
        evidence.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({**item, 'newFiles': None if added is None else len(added)}), flush=True)
        if observed_pid == original_pid and added is not None and len(added) == args.jobs and service is False:
            report['passed'] = True
            break
        # A query timeout/absence is an observation failure, not a process exit.
        time.sleep(2)
    log = call('logcat', '-d', '--pid', original_pid, '-v', 'threadtime', timeout=30)
    if log is not None:
        (output / 'background-logcat.txt').write_bytes(log)
        report['successfulPublications'] = len(re.findall(r'publication settled\s+\d+\s+true\s+false',
                                                         log.decode(errors='replace')))
    else:
        report['successfulPublications'] = None
    if report['passed']:
        artifacts = []
        for index, path in enumerate(report['polls'][-1]['newFiles']):
            data = call('exec-out', 'cat', path, timeout=30)
            if data is None or not data.startswith(b'\x89PNG\r\n\x1a\n'):
                report['passed'] = False
                break
            local = output / f'export-{index}.png'
            local.write_bytes(data)
            artifacts.append({'devicePath': path, 'localPath': str(local), 'size': len(data),
                              'sha256': hashlib.sha256(data).hexdigest()})
        report['artifacts'] = artifacts
        report['passed'] = (report['passed'] and len(artifacts) == args.jobs
                            and report['successfulPublications'] == args.jobs)
    screenshot = call('exec-out', 'screencap', '-p', timeout=30)
    if screenshot and screenshot.startswith(b'\x89PNG\r\n\x1a\n'):
        (output / 'home.png').write_bytes(screenshot)
    report['elapsedSeconds'] = round(time.monotonic()-start, 3)
    evidence.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'evidence': str(evidence),
                      'elapsedSeconds': report['elapsedSeconds'], 'P5Accepted': False}), flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
