"""Run mobile settings integration checks in separate Release processes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--qt-bin', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    storage = Path(tempfile.mkdtemp(prefix='storage-', dir=args.output.resolve()))
    fixture_hash = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    env = os.environ.copy()
    env['PATH'] = str(args.qt_bin.resolve()) + os.pathsep + env['PATH']
    env['QSG_RHI_BACKEND'] = 'opengl'
    env['QSG_RENDER_LOOP'] = 'basic'
    results = []
    for mode in ('seed', 'restart', 'invalid'):
        command = [str(args.executable.resolve()), '--preferences-smoke', mode,
                   '--storage-root', str(storage),
                   '--preferences-fixture', str(args.fixture.resolve())]
        with (args.output / f'{mode}.log').open('wb') as log:
            process = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=60, check=False)
        report_file = storage / f'preferences-{mode}.json'
        report = json.loads(report_file.read_text(encoding='utf-8')) if report_file.is_file() else {}
        results.append({'mode': mode, 'exitCode': process.returncode, 'report': report})
        if process.returncode or not report.get('passed'):
            break
    proof = {'passed': len(results) == 3 and all(r['exitCode'] == 0 and r['report'].get('passed') for r in results),
             'storageRoot': str(storage), 'separateProcesses': True,
             'sourceChartUnchanged': fixture_hash == hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
             'fixtureSha256': fixture_hash, 'results': results, 'P5Accepted': False,
             'pairedV2FidelityVerified': False, 'nativeAndroidVerified': False}
    (args.output / 'verification.json').write_text(json.dumps(proof, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in proof.items() if key != 'results'}))
    return 0 if proof['passed'] and proof['sourceChartUnchanged'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
