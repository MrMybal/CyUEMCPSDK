"""HTTP conversation transports. No tools, no automatic generation retries, no stored keys."""
import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from .core import ConnectorError, Transport

DEFAULTS = {
    'ollama': ('http://127.0.0.1:11434', ''),
    'openrouter': ('https://openrouter.ai/api/v1', 'OPENROUTER_API_KEY'),
    'anthropic': ('https://api.anthropic.com/v1', 'ANTHROPIC_API_KEY'),
}

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None

class HttpTransport(Transport):
    def __init__(self, descriptor, config=None, sink=None):
        super().__init__(descriptor, config, sink)
        self.name = descriptor['preferred_transport']
        self.messages = []
        self.models = []
        self.url = (self.config.get('endpoint') or DEFAULTS[self.id][0]).rstrip('/')
        self.key_env = self.config.get('api_key_env') or DEFAULTS[self.id][1]
        self.opener = urllib.request.build_opener(NoRedirect())

    def detect(self):
        self.begin()
        u = urllib.parse.urlparse(self.url)
        if not u.hostname or u.username or u.password or u.query or u.fragment:
            raise ConnectorError('InvalidEndpoint', 'Use a server URL without credentials, query or fragment')
        if u.scheme != 'https' and not (u.scheme == 'http' and u.hostname in {'127.0.0.1', 'localhost', '::1'}):
            raise ConnectorError('InvalidEndpoint', 'Remote servers require HTTPS; HTTP is supported on loopback')
        if self.key_env and not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', self.key_env):
            raise ConnectorError('InvalidInput', 'Enter the API key environment variable NAME, not the key')
        if self.id != 'ollama' and not os.environ.get(self.key_env):
            return {'status': 'Configuration required', 'code': 'AuthenticationRequired',
                    'error': f'Set {self.key_env} on this computer, restart Unreal, then test again'}
        self.status = 'Installed'
        return {'status': self.status, 'transport': self.name, 'endpoint': self.url}

    def request(self, path, body=None):
        self.check()
        headers = {'Content-Type': 'application/json'}
        key = os.environ.get(self.key_env, '') if self.key_env else ''
        if key:
            if key not in self.redactor.values: self.redactor.values.append(key)
            headers['x-api-key' if self.id == 'anthropic' else 'Authorization'] = key if self.id == 'anthropic' else 'Bearer '+key
        if self.id == 'anthropic': headers['anthropic-version'] = '2023-06-01'
        req = urllib.request.Request(self.url+path, None if body is None else json.dumps(body).encode(), headers)
        try:
            return self.opener.open(req, timeout=max(.1, min(120, self.deadline-time.monotonic())))
        except urllib.error.HTTPError as exc:
            code = 'AuthenticationRequired' if exc.code in (401, 403) else 'RateLimited' if exc.code == 429 else 'ProviderError'
            # Error bodies may echo account details or keys. Log only status.
            exc.close()
            raise ConnectorError(code, f'{self.descriptor["name"]} returned HTTP {exc.code}') from exc
        except OSError as exc:
            raise ConnectorError('ConnectionFailed', self.redactor.clean(str(exc))) from exc

    def get(self, path):
        with self.request(path) as response:
            result = json.load(response)
        self.check()
        return result

    def connect(self):
        self.begin()
        discovery = self.detect()
        if discovery['status'] != 'Installed':
            raise ConnectorError(discovery.get('code','ConnectionFailed'), discovery['error'])
        # OpenRouter's public models endpoint alone does not authenticate a key.
        if self.id == 'openrouter': self.get('/key')
        self.models = self.list_models()
        self.status = 'Connected'
        self.emit('Connected')
        return {'authenticated': True, 'transport': self.name, 'models': self.models}

    def list_models(self):
        self.begin()
        if self.id == 'ollama':
            rows = self.get('/api/tags').get('models', [])
            return [{'id': m.get('model') or m['name'], 'name': m['name']} for m in rows]
        rows=[]; path='/models'; cursors=set()
        while True:
            result=self.get(path)
            rows.extend(result.get('data',[]))
            if self.id!='anthropic' or not result.get('has_more'): break
            cursor=result.get('last_id')
            if not cursor or cursor in cursors: raise ConnectorError('ProtocolError','Invalid model catalog pagination')
            cursors.add(cursor); path='/models?after_id='+urllib.parse.quote(cursor,safe='')
        return rows

    def start_session(self, model='', native_id=None):
        if native_id: raise ConnectorError('Unsupported', 'HTTP conversation history is owned by this chat, not a native session')
        if not model:
            if self.id == 'ollama' and len(self.models)==1: model=self.models[0]['id']
            else: raise ConnectorError('ModelRequired', 'Choose a model in Settings > Connections after Test connection')
        self.messages=[]
        return super().start_session(model)

    def frames(self, response):
        data=[]
        for raw in response:
            self.check()
            line=raw.decode('utf-8').strip()
            if self.id=='ollama':
                if line: yield json.loads(line)
            elif not line:
                if data:
                    payload='\n'.join(data); data=[]
                    yield {'_done':True} if payload=='[DONE]' else json.loads(payload)
            elif line.startswith('data:'): data.append(line[5:].strip())
        if data:
            payload='\n'.join(data)
            yield {'_done':True} if payload=='[DONE]' else json.loads(payload)

    def send(self, prompt):
        self.begin()
        if self.session is None: raise ConnectorError('SessionInvalid','Open a conversation first')
        self.turn_started=time.monotonic(); self.session.status='running'
        self.emit('TurnStarted')
        messages=[*self.messages, {'role':'user','content':prompt}]
        body={'model':self.session.model,'messages':messages,'stream':True}
        if self.id!='ollama': body['max_tokens']=4096
        path='/api/chat' if self.id=='ollama' else '/messages' if self.id=='anthropic' else '/chat/completions'
        answer=''; completed=False
        try:
            with self.request(path,body) as response:
                for data in self.frames(response):
                    if data.get('error') or data.get('type')=='error':
                        raise ConnectorError('ProviderError', self.redactor.clean(str(data.get('error','Stream failed'))))
                    resolved=data.get('model') or data.get('message',{}).get('model')
                    if resolved and resolved!=self.session.model:
                        self.session.model=resolved; self.emit('ModelResolved',resolved)
                    text=''
                    if self.id=='ollama':
                        text=data.get('message',{}).get('content',''); completed=data.get('done') is True
                    elif self.id=='anthropic':
                        delta=data.get('delta',{})
                        if delta.get('type')=='text_delta': text=delta.get('text','')
                        completed=data.get('type')=='message_stop'
                    else:
                        choices=data.get('choices',[])
                        if choices: text=choices[0].get('delta',{}).get('content') or ''
                        completed=data.get('_done') is True
                    if text: answer+=text; self.emit('TextDelta',text)
                    if completed: break
            self.check()
            if not completed: raise ConnectorError('UnexpectedEOF','Stream ended without a terminal provider event')
            self.messages=[*messages,{'role':'assistant','content':answer}]
            self.session.status='completed'; self.emit('TurnCompleted',answer)
            return answer
        except Exception:
            self.session.status='cancelled' if self.cancelled.is_set() else 'error'
            raise

    def disconnect(self):
        self.messages=[]; self.status='Disconnected'
