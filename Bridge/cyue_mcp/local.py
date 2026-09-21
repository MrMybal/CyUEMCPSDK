"""Optional llama.cpp HTTP transport using the SDK's integrated connector contract.

The Unreal LocalInference module owns the server process. No external connector
package or application is needed.
"""
import copy
import json
import urllib.request
from pathlib import Path
from cyue_mcp.providers.core import ConnectorError, Transport, descriptors
from cyue_mcp.providers import create_transport

class LocalTransport(Transport):
    name='llama-http'
    def __init__(self, descriptor, config, sink, connection_file):
        super().__init__(descriptor,config,sink)
        self.connection_file=Path(connection_file)
        self.info=None
        self.messages=[]
    def detect(self):
        if not self.connection_file.is_file():
            return {'status':'Executable Missing','code':'ExecutableMissing','error':'Start a model in the Unreal Local inference panel first'}
        self.info=json.loads(self.connection_file.read_text('utf-8-sig'))
        from urllib.parse import urlparse
        u=urlparse(self.info['url'])
        if u.scheme!='http' or u.hostname!='127.0.0.1':
            raise ConnectorError('AccessDenied','Local inference must use loopback')
        return {'status':'Installed','transport':self.name,'version':'server-reported at connect'}
    def request(self,path,body=None):
        self.check()
        data=None if body is None else json.dumps(body).encode('utf8')
        req=urllib.request.Request(self.info['url']+path,data,{'Content-Type':'application/json','Authorization':'Bearer '+self.info['token']})
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self,*args,**kw):return None
        try:
            with urllib.request.build_opener(NoRedirect()).open(req,timeout=float(self.config.get('timeout',120))) as response:
                result=json.load(response)
        except OSError as e:raise ConnectorError('ProviderError',str(e)) from e
        self.check()
        return result
    def connect(self):
        self.begin()
        if self.info is None:self.detect()
        models=self.list_models()
        self.status='Connected'
        self.emit('Connected')
        return {'authenticated':True,'models':models,'transport':self.name}
    def list_models(self):
        self.begin()
        return self.request('/v1/models').get('data',[])
    def start_session(self,model='',native_id=None):
        if native_id:raise ConnectorError('Unsupported','Local native resume is unavailable; history belongs to this conversation')
        self.messages=[]
        return super().start_session(model or Path(self.info.get("model", "local")).name)
    def send(self,prompt):
        self.begin()
        if not self.session:raise ConnectorError('SessionInvalid','Open a conversation first')
        messages=[*self.messages,{'role':'user','content':prompt}]
        result=self.request('/v1/chat/completions',{'model':self.session.model or 'local','messages':messages,'max_tokens':512,'stream':False})
        choices=result.get('choices',[])
        if not choices or not isinstance(choices[0].get('message',{}).get('content'),str):
            raise ConnectorError('ProtocolError','Missing assistant completion')
        text=choices[0]['message']['content']
        self.messages=[*messages,{'role':'assistant','content':text}]
        self.emit('TextSnapshot',text)
        self.emit('TurnCompleted',text,usage=result.get('usage'))
        return text
    def disconnect(self):
        # Never stop an externally owned server. The owning Unreal module stops it.
        self.status='Disconnected';self.messages=[]

def catalog():
    result=copy.deepcopy(descriptors())
    result['local']['preferred_transport']='llama-http'
    result['local']['transports']=[{'id':'llama-http','implemented':True,'evidence':'CyUEMCPSDK extension; see validation report'}]
    result['local']['capabilities']={'sessions':'declared','resume':'unsupported','streaming':'unsupported','models':'declared','tools':'unsupported','mcp':'unsupported'}
    return result

def factory(connection_file):
    def create(connector,config,sink):
        if connector=='local':return LocalTransport(catalog()['local'],config,sink,connection_file)
        return create_transport(connector,config,sink)
    return create
