"""Validate the actual mobile background page and cold restart on the host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--qt-bin', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--size', action='append', help='Logical WIDTHxHEIGHT; repeat for a matrix.')
    args = parser.parse_args()
    build_root = Path(__file__).resolve().parents[2] / 'build-devtools'
    if not args.output.resolve().is_relative_to(build_root.resolve()):
        parser.error('Validation artifacts must stay in build-devtools.')
    sizes = args.size or ['1280x720', '960x720', '1120x480']
    for size in sizes:
        if not re.fullmatch(r'[1-9][0-9]*x[1-9][0-9]*', size):
            parser.error('Invalid WIDTHxHEIGHT: ' + size)
        width, height = map(int, size.split('x'))
        if width * height > 16_777_216 or min(width, height) < 64:
            parser.error('Unsupported validation geometry: ' + size)
    args.output.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='run-', dir=args.output.resolve()))
    fixture = run_root / 'fixture'
    fixture.mkdir()
    original_hash = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    shutil.copy2(args.fixture, fixture / 'maidata.txt')
    env = os.environ.copy()
    env['PATH'] = str(args.qt_bin.resolve()) + os.pathsep + env['PATH']
    env['QSG_RHI_BACKEND'] = 'opengl'
    env['QSG_RENDER_LOOP'] = 'basic'
    cases = []
    for index, size in enumerate(sizes):
        storage = run_root / f'{index}-{size}'
        storage.mkdir()
        results = []
        for mode in ('seed', 'restart'):
            command = [str(args.executable.resolve()), '--background-ui-smoke', mode,
                       '--size', size, '--storage-root', str(storage),
                       '--fixture', str(fixture / 'maidata.txt')]
            with (storage / f'{mode}.log').open('wb') as log:
                process = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                         timeout=60, check=False)
            report_file = storage / f'background-{mode}-proof.json'
            report = json.loads(report_file.read_text(encoding='utf-8')) if report_file.is_file() else {}
            results.append({'mode': mode, 'exitCode': process.returncode, 'report': report})
            if process.returncode or not report.get('passed'):
                break
        captures = []
        for image in sorted(storage.glob('*.png')):
            if image.name == 'background-fixture.png':
                continue
            header = image.read_bytes()[:24]
            if len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n':
                raise RuntimeError('Invalid capture: ' + str(image))
            captures.append({'path': str(image), 'pixelSize': list(struct.unpack('>II', header[16:24]))})
        cases.append({'logicalSize': size, 'results': results, 'captures': captures,
                      'passed': len(results) == 2 and all(r['exitCode'] == 0 and r['report'].get('passed') for r in results)})
        if not cases[-1]['passed']:
            break
    proof = {'passed': len(cases) == len(sizes) and all(case['passed'] for case in cases),
             'runRoot': str(run_root), 'sourceChartUnchanged': original_hash == hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
             'sourceChartSha256': original_hash, 'separateProcesses': True, 'cases': cases,
             'nativeAndroidVerified': False, 'SAFPickerVerified': False,
             'P5Accepted': False, 'pairedV2FidelityVerified': False}
    payload = json.dumps(proof, indent=2)
    (run_root / 'verification.json').write_text(payload, encoding='utf-8')
    (args.output / 'latest-verification.json').write_text(payload, encoding='utf-8')
    print(json.dumps({key: value for key, value in proof.items() if key != 'cases'}))
    return 0 if proof['passed'] and proof['sourceChartUnchanged'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
