from pathlib import Path
import pytest
from cytools_core.errors import CyToolError
from cytools_core.runtime import Runtime
from cyue_mcp.service import NativeClient

class Context:
    job_id='job-fixed'
    workspace=Path('workspace')
    class cancellation:
        @staticmethod
        def check(): pass

def test_retry_keeps_exact_receipt_id(monkeypatch):
    n=NativeClient({'url':'http://127.0.0.1:39001/cyue/v1','pid':1,'token':'test'})
    calls=[]
    def call(method,**params):
        calls.append((method,params))
        if len(calls)==1:raise TimeoutError('response lost after mutation')
        return {'result':{'done':True}}
    monkeypatch.setattr(n,'call',call);monkeypatch.setattr(n,'alive',lambda:True)
    assert n.execute(Context(),'mutation',{'x':1})=={'done':True}
    assert calls[0]==calls[1]
    assert calls[2][0]=='ack'

def test_remote_native_endpoint_rejected():
    with pytest.raises(ValueError):NativeClient({'url':'http://example.com/cyue/v1'})

def test_editor_death_ends_wait(monkeypatch):
    n=NativeClient({'url':'http://127.0.0.1:39001/cyue/v1','pid':1,'token':'test'})
    def call(*a,**k):raise OSError('gone')
    monkeypatch.setattr(n,'call',call);monkeypatch.setattr(n,'alive',lambda:False)
    with pytest.raises(CyToolError) as error:n.execute(Context(),'read',{})
    assert error.value.code=='EditorClosed'


def test_finish_task_closes_conversation(tmp_path):
    from cyue_mcp.service import Companion
    from cyue_mcp.connectors import ConnectorService
    from test_connectors import Fake,CATALOG
    class Native:
        info={'pid':1}
        def describe(self):
            return {'schemaVersion':1,'protocolVersion':'1.0','id':'TestUE','name':'Test UE','version':'0.1.0','description':'test','operations':[],
                    'platforms':[{'os':'Windows','architecture':'x64','status':'Supported'}],
                    'concurrency':{'policy':'Concurrent','maxWorkers':4}}
    c=Companion(Native(),tmp_path/'runtime')
    c.connectors=ConnectorService(tmp_path/'conversations',factory=Fake,catalog=lambda:CATALOG)
    try:
        user=c.runtime.create_client('test',['assistant.use'])
        owner=c.runtime.authenticate(user['token']);c.runtime.start()
        session=c.runtime.create_session(owner)
        job=c.runtime.submit(owner,session['sessionId'],'assistant_open',{'connector':'fake'})
        final=c.runtime.wait(owner,job['jobId'],5)
        assert final['state']=='Completed',final
        item=next(iter(c.connectors._conversations.values()))
        c.runtime.finish_task(owner,session['sessionId'])
        c.reap_conversations()
        assert item.transport.disconnected
        assert not c.connectors._conversations
    finally:c.close()
