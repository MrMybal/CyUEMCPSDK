"""Opt-in: existing agy account and existing Ollama model only, no downloads."""
import json, os, subprocess, socket, tempfile, time, urllib.request
from pathlib import Path
from cyue_mcp.connectors import ConnectorService
from cyue_mcp.providers.core import ConnectorError
root=Path(__file__).resolve().parents[1]
report={}
with tempfile.TemporaryDirectory(prefix='cyue-015-') as tmp:
    service=ConnectorService(tmp)
    for provider in ('antigravity','opencode','anthropic','openrouter'):
        probe=service.probe(provider,timeout=20)
        report[provider]={'probe':probe['status'],'modelCount':len(probe.get('models',[]))}
        if provider=='antigravity' and probe.get('models'):
            model=probe['models'][0]['id']
            try:
                cid=service.open('validation',provider,model=model,timeout=90)['conversationId']
                reply=service.send('validation',cid,'Reply exactly CYUE_BASE_OK. Do not use tools.')
                report[provider].update(livePassed='CYUE_BASE_OK' in reply['text'],model=reply['session']['model'])
                service.close('validation',cid)
            except ConnectorError as e:report[provider].update(livePassed=False,error=e.code)
    # Own an isolated Ollama server port, reuse installed weights, stop only this process tree.
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    env={**os.environ,'OLLAMA_HOST':f'127.0.0.1:{port}','OLLAMA_KEEP_ALIVE':'0'}
    log=open(Path(tmp)/'ollama.log','w',encoding='utf-8')
    proc=subprocess.Popen(['ollama','serve'],env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
    url=f'http://127.0.0.1:{port}'
    try:
        for _ in range(80):
            try:
                with urllib.request.urlopen(url+'/api/tags',timeout=1) as response:models=json.load(response)['models']
                break
            except OSError:time.sleep(.1)
        else:raise RuntimeError('Isolated Ollama server did not start')
        model=next((m['name'] for m in models if m['name']=='qwen3:0.6b'),None)
        if model:
            probe=service.probe('ollama',endpoint=url)
            cid=service.open('validation','ollama',endpoint=url,model=model,timeout=120)['conversationId']
            result=service.send('validation',cid,'Reply exactly CYUE_OLLAMA_OK. /no_think')
            report['ollama']={'probe':probe['status'],'model':result['session']['model'],'livePassed':'CYUE_OLLAMA_OK' in result['text']}
            service.close('validation',cid)
        else:report['ollama']={'livePassed':False,'reason':'No preinstalled small test model'}
    finally:
        service.shutdown()
        if proc.poll() is None:subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
        proc.wait(timeout=10);log.close()
(root/'Reports/live-base-connectors-015.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
