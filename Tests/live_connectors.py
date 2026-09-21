import json,sys,tempfile
from pathlib import Path
from cyue_mcp.connectors import ConnectorService
root=Path(__file__).resolve().parents[1]
results={}
with tempfile.TemporaryDirectory(prefix='cyue-connectors-') as work:
 service=ConnectorService(work)
 try:
  for connector in sys.argv[1:] or ['codex']:
   opened=service.open('test-user',connector,timeout=90)
   cid=opened['conversationId']
   first=service.send('test-user',cid,'Reply exactly with CYUE_CONNECTOR_OK')
   second=service.send('test-user',cid,'Reply exactly with CYUE_RESUME_OK')
   assert first['text'].strip()=='CYUE_CONNECTOR_OK',first['text']
   assert second['text'].strip()=='CYUE_RESUME_OK',second['text']
   assert first['session']['native_id']==second['session']['native_id']
   results[connector]={'passed':True,'transport':second['session']['transport'], 'version':opened['discovery'].get('version'),'twoTurnsSameNativeSession':True,'textDeltaObserved':any(e['type']=='TextDelta' for e in second['events'])}
   service.close('test-user',cid)
   print('PASS '+connector+': two turns, same native session, clean disconnect',flush=True)
 finally:service.shutdown()
(root/'Reports/live-connectors.json').write_text(json.dumps(results,indent=2),encoding='utf8')
