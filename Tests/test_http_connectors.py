import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import pytest
from cyue_mcp.connectors import ConnectorService
from cyue_mcp.providers.core import ConnectorError

@pytest.fixture
def endpoint(monkeypatch):
    monkeypatch.setenv('CYUE_TEST_KEY','test-key-not-a-real-account')
    calls=[]; mode={'broken':False,'auth':True}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def reply(self,body,status=200):
            self.send_response(status);self.end_headers();self.wfile.write(json.dumps(body).encode())
        def do_GET(self):
            calls.append(('GET',self.path,None))
            if self.path=='/key' and not mode['auth']:self.reply({},401);return
            if self.path=='/api/tags':self.reply({'models':[{'name':'test-model','model':'test-model'}]})
            else:self.reply({'data':[{'id':'test-model'}]})
        def do_POST(self):
            body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            calls.append(('POST',self.path,body));self.send_response(200);self.end_headers()
            if self.path=='/api/chat':
                frames=[{'message':{'content':'hello'},'done':False}]
                if not mode['broken']:frames+=[{'done':True,'message':{'content':''}}]
                stream=''.join(json.dumps(f)+'\n' for f in frames)
            elif self.path=='/messages':
                frames=[{'type':'message_start','message':{'model':'resolved-model'}},{'type':'content_block_delta','delta':{'type':'text_delta','text':'hello'}}]
                if not mode['broken']:frames+=[{'type':'message_stop'}]
                stream=''.join('data: '+json.dumps(f)+'\n\n' for f in frames)
            else:
                stream='data: '+json.dumps({'model':'resolved-model','choices':[{'delta':{'content':'hello'}}]})+'\n\n'
                if not mode['broken']:stream+='data: [DONE]\n\n'
            self.wfile.write(stream.encode())
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    yield f'http://127.0.0.1:{server.server_port}',calls,mode
    server.shutdown();server.server_close();thread.join()

@pytest.mark.parametrize('provider',['ollama','anthropic','openrouter'])
def test_http_probe_stream_history_and_failure_cleanup(provider,endpoint,tmp_path):
    url,calls,mode=endpoint
    service=ConnectorService(tmp_path)
    args={'endpoint':url,'api_key_env':'CYUE_TEST_KEY'}
    try:
        assert service.probe(provider,**args)['ready']
        assert not any(c[0]=='POST' for c in calls)
        first=service.open('owner',provider,model='test-model',**args)['conversationId']
        other=service.open('owner',provider,model='test-model',**args)['conversationId']
        assert service.send('owner',first,'first')['text']=='hello'
        assert service.send('owner',first,'second')['text']=='hello'
        body=[c[2] for c in calls if c[0]=='POST'][-1]
        assert [m['role'] for m in body['messages']]==['user','assistant','user']
        assert service.send('owner',other,'independent')['text']=='hello'
        assert len([c[2] for c in calls if c[0]=='POST'][-1]['messages'])==1
        mode['broken']=True
        with pytest.raises(ConnectorError) as error:service.send('owner',first,'broken')
        assert error.value.code=='UnexpectedEOF'
        assert len(service._owned('owner',first).transport.messages)==4
        with pytest.raises(ConnectorError):service.send('stranger',first,'no')
    finally:service.shutdown()

def test_openrouter_public_catalog_does_not_prove_auth(endpoint,tmp_path):
    url,calls,mode=endpoint;mode['auth']=False
    service=ConnectorService(tmp_path)
    result=service.probe('openrouter',endpoint=url,api_key_env='CYUE_TEST_KEY')
    assert not result['ready'] and result['status']=='AuthenticationRequired'
    assert calls==[('GET','/key',None)]
    service.shutdown()

def test_http_configuration_and_unsupported_rights(tmp_path,monkeypatch):
    monkeypatch.delenv('ANTHROPIC_API_KEY',raising=False)
    service=ConnectorService(tmp_path)
    assert service.probe('anthropic')['status']=='Configuration required'
    assert service.probe('ollama',endpoint='http://example.com')['status']=='InvalidEndpoint'
    assert service.probe('ollama',endpoint='https://secret:password@example.com')['status']=='InvalidEndpoint'
    with pytest.raises(ConnectorError):service.open('owner','ollama',access='workspace-write')
    service.shutdown()
