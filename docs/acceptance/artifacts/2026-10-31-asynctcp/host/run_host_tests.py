"""Build and run the recovered deterministic Windows host cases without Git/network."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', type=Path, help='Path to an installed clang++ executable.')
args = parser.parse_args()
if os.name != 'nt':
    parser.error('This recovered harness requires Windows (SetErrorMode and NT exception codes).')
compiler = args.compiler
if compiler is None:
    found = shutil.which('clang++')
    compiler = Path(found) if found else Path(os.environ.get('ProgramFiles', r'C:\Program Files'))/'LLVM/bin/clang++.exe'
if not compiler.is_file():
    parser.error('clang++ not found; specify --compiler. No tool installation is performed.')
build = ROOT/'.build'
subprocess.run([sys.executable, str(ROOT/'prepare_harness.py'), '--output-dir', str(build)], check=True)
for version in ['old', 'stable']:
    log = build/(version+'-build.log')
    command = [str(compiler), '-std=c++17', '-O0', '-g', '-I', str(ROOT.parent/'upstream/esp32async-v3.5.0'), str(build/(version+'_harness.cpp')), '-o', str(build/(version+'_harness.exe'))]
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT)
    diagnostics = log.read_text(encoding='utf-8', errors='replace')
    print(version+' build: exit='+str(result.returncode)+' warnings='+str(diagnostics.count('warning:'))+' log='+str(log), flush=True)
    if result.returncode:
        print(diagnostics, file=sys.stderr)
        raise SystemExit(result.returncode)
subprocess.run([sys.executable, str(ROOT/'run_harness.py'), '--build-dir', str(build)], check=True)
