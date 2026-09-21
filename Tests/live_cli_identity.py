from pathlib import Path
from cyue_mcp.connectors import ConnectorService
import json,tempfile
with tempfile.TemporaryDirectory(prefix='cyue-cli-identity-') as d:
 s=ConnectorService(d)
 try:
  r=s.open('identity-test','codex',transport='cli-jsonl',timeout=60)
  answer=s.send('identity-test',r['conversationId'],'Reply exactly CYUE_CLI_OK')
  assert answer['session']['model'],answer['session']
  assert 'CYUE_CLI_OK' in answer['text']
  result={'passed':True,'model':answer['session']['model'],'transport':answer['session']['transport']}
  Path('Reports/live-cli-identity.json').write_text(json.dumps(result,indent=2))
  print(result,flush=True)
 finally:s.shutdown()
