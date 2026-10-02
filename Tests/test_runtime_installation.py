"""Offline bootstrap tests run only on Windows, without touching the real shared pool."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import zipfile
import pytest

ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / 'Plugins/CyUEMCPSDK/Resources/AssistantRuntime'
PS = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
pytestmark = pytest.mark.skipif(os.name != 'nt', reason='Windows offline runtime')

def run_setup(folder, pool, report, action='Check', *extra):
    command = [str(PS), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', str(folder / 'SetupRuntime.ps1'), '-Action', action, '-Report', str(report), *extra]
    if pool is not None:
        command.extend(['-StorageRoot', str(pool)])
    process = subprocess.run(command, capture_output=True, text=True, timeout=180)
    result = json.loads(report.read_text(encoding='utf-8-sig'))
    return process.returncode, result

@pytest.fixture(scope='module')
def installed(tmp_path_factory):
    work = tmp_path_factory.mktemp('assistant-runtime')
    code, result = run_setup(RESOURCES, work / 'pool', work / 'install.json', 'Install')
    assert code == 0 and result['state'] == 'Ready', result
    return work, result

def test_missing_check_does_not_install(tmp_path):
    code, result = run_setup(RESOURCES, tmp_path / 'pool', tmp_path / 'check.json')
    assert code == 0 and result['state'] == 'Missing'
    assert not (tmp_path / 'pool').exists()

def test_offline_python_and_exact_dependencies(installed):
    work, result = installed
    process = subprocess.run([result['python'], '-B', str(Path(result['root']) / 'verify-runtime.py')], capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    check = json.loads(process.stdout)
    assert check == {'python': '3.13.16', 'sdk': '0.1.7'}
    assert Path(result['script']).is_file()
    assert not list(Path(result['root']).rglob('direct_url.json'))
    assert not list(Path(result['root']).rglob('__pycache__'))

def test_reuse_from_another_plugin_copy(installed, tmp_path):
    work, original = installed
    copy = tmp_path / 'AnotherPlugin/Resources/AssistantRuntime'
    copy.mkdir(parents=True)
    for name in ('SetupRuntime.ps1', 'bundle.json'):
        shutil.copyfile(RESOURCES / name, copy / name)
    # A ready installation is reused without even opening the bundled archive.
    code, result = run_setup(copy, work / 'pool', tmp_path / 'check.json')
    assert code == 0 and result['reused'] and result['root'] == original['root']

def test_local_mode_and_cancellation(tmp_path):
    cancel = tmp_path / 'cancel'
    cancel.touch()
    code, result = run_setup(RESOURCES, tmp_path / 'local', tmp_path / 'report.json', 'Install', '-Mode', 'Local', '-CancelFile', str(cancel))
    assert code == 2 and result['state'] == 'Cancelled' and result['mode'] == 'Local'
    assert not (tmp_path / 'local').exists()

def test_tampered_archive_rejected(tmp_path):
    copy = tmp_path / 'bundle'
    copy.mkdir()
    shutil.copyfile(RESOURCES / 'SetupRuntime.ps1', copy / 'SetupRuntime.ps1')
    shutil.copyfile(RESOURCES / 'bundle.json', copy / 'bundle.json')
    manifest = json.loads((copy / 'bundle.json').read_text())
    (copy / manifest['archive']).write_bytes(b'not the offline archive')
    code, result = run_setup(copy, tmp_path / 'pool', tmp_path / 'report.json', 'Install')
    assert code == 1 and 'checksum' in result['message']
    assert not list((tmp_path / 'pool').glob('stage-*'))

def test_zip_traversal_rejected(tmp_path):
    copy = tmp_path / 'bundle'
    copy.mkdir()
    shutil.copyfile(RESOURCES / 'SetupRuntime.ps1', copy / 'SetupRuntime.ps1')
    manifest = json.loads((RESOURCES / 'bundle.json').read_text())
    archive = copy / manifest['archive']
    with zipfile.ZipFile(archive, 'w') as bundle:
        bundle.writestr('../outside.txt', 'unsafe')
    manifest.update(sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), extractedBytes=6, fileCount=1)
    (copy / 'bundle.json').write_text(json.dumps(manifest))
    code, result = run_setup(copy, tmp_path / 'pool', tmp_path / 'report.json', 'Install')
    assert code == 1 and 'path' in result['message'].lower()
    assert not (tmp_path / 'outside.txt').exists()
    assert not list((tmp_path / 'pool').glob('stage-*'))

def test_simultaneous_installs_share_one_runtime(tmp_path):
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        futures = [executor.submit(run_setup, RESOURCES, tmp_path / 'pool', tmp_path / f'report-{i}.json', 'Install') for i in range(2)]
        results = [f.result() for f in futures]
    assert all(code == 0 and result['state'] == 'Ready' for code, result in results), results
    assert results[0][1]['root'] == results[1][1]['root']
    assert sum(result['reused'] for _, result in results) == 1
    assert len(list((tmp_path / 'pool').glob('*/.complete.json'))) == 1

def test_repair_preserves_existing_runtime_and_uses_new_directory(installed):
    work, original = installed
    file = Path(original['root']) / 'companion/run.py'
    original_bytes = file.read_bytes()
    file.write_bytes(b'# corrupt companion')
    try:
        code, check = run_setup(RESOURCES, work / 'pool', work / 'damaged.json')
        assert code == 0 and check['state'] == 'Missing'
        code, repair = run_setup(RESOURCES, work / 'pool', work / 'repair.json', 'Install')
        assert code == 0 and repair['state'] == 'Ready', repair
        assert repair['root'] != original['root']
        assert file.read_bytes() == b'# corrupt companion'
        code, reused = run_setup(RESOURCES, work / 'pool', work / 'reused.json')
        assert code == 0 and reused['root'] == repair['root']
    finally:
        file.write_bytes(original_bytes)


def test_real_plugin_local_install(tmp_path):
    plugin = tmp_path / 'Plugin'
    plugin.mkdir()
    code, result = run_setup(RESOURCES, None, tmp_path / 'local.json', 'Install', '-Mode', 'Local', '-PluginRoot', str(plugin))
    assert code == 0 and result['state'] == 'Ready', result
    assert Path(result['root']).is_relative_to(plugin / 'Runtime/Assistant')
    code, reused = run_setup(RESOURCES, None, tmp_path / 'local-check.json', 'Check', '-Mode', 'Local', '-PluginRoot', str(plugin))
    assert code == 0 and reused['reused'] and reused['root'] == result['root']


def test_cancel_during_extraction_cleans_staging(tmp_path):
    report = tmp_path / 'report.json'
    cancel = tmp_path / 'cancel'
    pool = tmp_path / 'pool'
    command = [str(PS), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', str(RESOURCES / 'SetupRuntime.ps1'), '-Action', 'Install', '-StorageRoot', str(pool), '-Report', str(report), '-CancelFile', str(cancel)]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 60
    try:
        while process.poll() is None and time.monotonic() < deadline:
            if report.exists():
                try:
                    state = json.loads(report.read_text(encoding='utf-8-sig'))
                except (OSError, ValueError):
                    continue
                if state['state'] == 'Installing' and state['progress'] >= 5:
                    cancel.touch()
                    break
            time.sleep(.02)
        process.communicate(timeout=60)
        result = json.loads(report.read_text(encoding='utf-8-sig'))
        assert process.returncode == 2 and result['state'] == 'Cancelled', result
        assert not list(pool.glob('stage-*'))
        assert not list(pool.glob('*/.complete.json'))
    finally:
        if process.poll() is None:
            cancel.touch()
            process.communicate(timeout=60)
