from pathlib import Path
import json,tempfile
from cyue_mcp.connectors import ConnectorService
out={}
for connector in ['codex','claude-code']:
 out[connector]={}
 for access in ['read-only','workspace-write']:
  with tempfile.TemporaryDirectory(prefix='cyue-permissions-') as directory:
   root=Path(directory); target=root/'permission-probe.txt'
   s=ConnectorService(root/'runtime',project_root=root)
   try:
    r=s.open('permission-test',connector,access=access,timeout=60)
    answer=s.send('permission-test',r['conversationId'],'Attempt once to create permission-probe.txt in the current working directory containing exactly CYUE_PERMISSION_OK. Use your file-writing tool if available; otherwise one direct shell write. If permissions deny writing, do not retry or seek alternatives; reply DENIED. Do not modify any other file.')
    written=target.is_file() and target.read_text().strip()=='CYUE_PERMISSION_OK'
    out[connector][access]={'written':written,'model':answer['session'].get('model'),'passed':written==(access=='workspace-write')}
    print(connector,access,out[connector][access],flush=True)
   finally:s.shutdown()
Path('Reports/live-permissions.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
assert all(v['passed'] for r in out.values() for v in r.values()),out
