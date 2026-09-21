import importlib.util
import json
import subprocess
from pathlib import Path


def test_installed_package_does_not_need_connector_lab():
    root=Path(__file__).resolve().parents[1]
    python=root/'Runtime/python/Scripts/python.exe'
    code = """
import importlib.util, importlib.metadata, json, sys
assert importlib.util.find_spec('cyai_connectors') is None
assert importlib.util.find_spec('cyai_lab') is None
from cyue_mcp.providers import descriptors, create_transport
from cyue_mcp.service import Companion
from cyue_mcp.local import catalog
assert create_transport('codex').__class__.__module__.startswith('cyue_mcp.providers.')
assert create_transport('claude-code').__class__.__module__.startswith('cyue_mcp.providers.')
assert 'local' in catalog()
assert not any(name.startswith('cyai_') for name in sys.modules)
requirements=importlib.metadata.requires('cyue-mcp') or []
assert not any('connector-lab' in item for item in requirements)
print(json.dumps({'version':importlib.metadata.version('cyue-mcp'),'connectors':len(descriptors()),'externalPackageAbsent':True}))
"""
    result=subprocess.run([str(python),'-I','-c',code],capture_output=True,text=True,check=True)
    assert json.loads(result.stdout)['externalPackageAbsent']
