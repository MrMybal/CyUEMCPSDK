import threading
import time
from types import SimpleNamespace
from dataclasses import dataclass
import pytest
from cyue_mcp.providers.core import ConnectorError
from cyue_mcp.connectors import ConnectorService

CATALOG = {"fake": {"id":"fake","preferred_transport":"rpc", "transports":[{"id":"rpc","implemented":True}]},
           "future": {"id":"future","preferred_transport":"sdk", "transports":[{"id":"sdk","implemented":False}]}}
@dataclass
class Session:
    native_id: str = "native-exact"
class Fake:
    def __init__(self, connector, config, sink):
        self.config=config; self.sink=sink; self.session=None; self.calls=[]
        self.cancelled=threading.Event(); self.sending=threading.Event(); self.disconnected=False
    def detect(self): return {"status":"Installed"}
    def connect(self): self.calls.append("connect"); return {"authenticated":True}
    def start_session(self, model=""): self.session=Session(); self.calls.append("start")
    def get_status(self): return "Connected"
    def get_capabilities(self): return {"mcp":"unknown"}
    def send(self,text):
        self.sending.set()
        if text=="block": self.cancelled.wait(2)
        self.calls.append(text)
        self.sink("event", {"type":"TextDelta","text":text})
        return text
    def cancel(self): self.cancelled.set()
    def disconnect(self): self.disconnected=True

def service(tmp_path, factory=Fake):
    return ConnectorService(tmp_path, factory=factory, catalog=lambda:CATALOG)

def test_reuses_native_session_and_transport(tmp_path):
    s=service(tmp_path); opened=s.open("owner","fake"); cid=opened["conversationId"]
    s.send("owner",cid,"one"); result=s.send("owner",cid,"two")
    item=s._owned("owner",cid)
    assert item.transport.calls==["connect","start","one","two"]
    assert result["session"]["native_id"]=="native-exact"
    assert result["capabilities"]["mcp"]=="unknown"
    s.shutdown(); assert item.transport.disconnected

def test_other_owner_cannot_access(tmp_path):
    s=service(tmp_path); cid=s.open("owner","fake")["conversationId"]
    for method in (s.snapshot,s.cancel,s.close):
        with pytest.raises(ConnectorError,match="authenticated owner"): method("other",cid)
    s.shutdown()

def test_descriptor_is_not_implementation(tmp_path):
    s=service(tmp_path)
    assert not s.list_connectors()[1]["available"]
    with pytest.raises(ConnectorError) as error: s.open("owner","future")
    assert error.value.code=="Unsupported"

def test_concurrent_send_rejected_and_close_waits(tmp_path):
    s=service(tmp_path); cid=s.open("owner","fake")["conversationId"]; item=s._owned("owner",cid)
    t=threading.Thread(target=lambda:s.send("owner",cid,"block")); t.start()
    assert item.transport.sending.wait(1)
    with pytest.raises(ConnectorError) as error:s.send("owner",cid,"second")
    assert error.value.code=="SessionBusy"
    s.close("owner",cid); t.join(1)
    assert not t.is_alive() and item.transport.disconnected

def test_failed_connect_cleans_owned_transport(tmp_path):
    instances=[]
    class Broken(Fake):
        def __init__(self,*a,**kw): super().__init__(*a,**kw); instances.append(self)
        def connect(self): raise ConnectorError("ProtocolError","broken")
    s=service(tmp_path,Broken)
    with pytest.raises(ConnectorError): s.open("owner","fake")
    assert instances[0].disconnected and not s._conversations

def test_retention_bounded(tmp_path):
    s=service(tmp_path); cid=s.open("owner","fake")["conversationId"]
    for i in range(600): s.send("owner",cid,str(i))
    assert len(s.snapshot("owner",cid)["events"])==512
    s.shutdown()


def test_probe_checks_auth_without_conversation_and_retest(tmp_path):
    made=[]
    class Probe(Fake):
        logged_in=False
        def __init__(self,*a,**kw): super().__init__(*a,**kw); made.append(self)
        def connect(self): self.calls.append('connect'); return {'authenticated':self.logged_in}
        def list_models(self): return [{'id':'model-a'}]
    s=service(tmp_path,Probe)
    first=s.probe('fake'); assert not first['ready'] and first['status']=='Sign in required'
    Probe.logged_in=True
    second=s.probe('fake'); assert second['ready'] and second['models']==[{'id':'model-a'}]
    assert all(x.calls==['connect'] and x.disconnected for x in made)
    assert not s._conversations


def test_probe_missing_and_transport_errors_cleanup(tmp_path):
    made=[]
    class Missing(Fake):
        def __init__(self,*a,**kw): super().__init__(*a,**kw); made.append(self)
        def detect(self): return {'status':'Missing'}
    s=service(tmp_path,Missing)
    assert not s.probe('fake')['ready']
    assert made[0].disconnected and made[0].calls==[]
    assert s.probe('future')['status']=='Unsupported'
    class Broken(Missing):
        def detect(self): raise ConnectorError('Timeout','timed out')
    assert service(tmp_path,Broken).probe('fake')['status']=='Timeout'
    assert made[-1].disconnected
