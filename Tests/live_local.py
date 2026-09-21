"""Real local connector check with existing weights, without copying them."""
import json,os,secrets,socket,subprocess,sys,tempfile,time
from pathlib import Path
from cyue_mcp.connectors import ConnectorService
from cyue_mcp.local import catalog,factory
root=Path(__file__).resolve().parents[1]
server,model=sys.argv[1:3]
with socket.socket() as sock:
 sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
token=secrets.token_urlsafe(32)
with tempfile.TemporaryDirectory(prefix='cyue-local-test-') as work:
 file=Path(work)/'local.json';file.write_text(json.dumps({'url':f'http://127.0.0.1:{port}','token':token}),'utf8')
 with (root/'Reports/local-server.log').open('w',encoding='utf8') as log:
  proc=subprocess.Popen([server,'--model',model,'--host','127.0.0.1','--port',str(port),'--n-gpu-layers','0','--ctx-size','4096','--parallel','1','--no-webui','--api-key',token],stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
  service=ConnectorService(Path(work)/'conversations',factory=factory(file),catalog=catalog)
  try:
   deadline=time.monotonic()+120
   while True:
    try:opened=service.open('test','local',timeout=60);break
    except Exception:
     if proc.poll() is not None or time.monotonic()>deadline:raise
     time.sleep(.5)
   cid=opened['conversationId'];started=time.monotonic()
   answer=service.send('test',cid,'Reply with exactly the word HELLO.')
   assert 'hello' in answer['text'].lower(),answer
   result={'passed':True,'model':Path(model).name,'backend':'CPU','text':answer['text'],'elapsedSeconds':round(time.monotonic()-started,2),'weightsCopied':False,'nativeLauncherTested':False}
   (root/'Reports/live-local.json').write_text(json.dumps(result,indent=2),'utf8')
   print('PASS real local inference: '+answer['text'],flush=True)
  finally:
   service.shutdown();proc.terminate();proc.wait(timeout=10)
