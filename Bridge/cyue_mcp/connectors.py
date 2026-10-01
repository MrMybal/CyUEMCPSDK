"""Conversation ownership around the integrated provider transports.

No CLI command construction or provider JSON parsers live in the Unreal host.
One instance per authenticated owner/conversation; no implicit account resume.
"""
from collections import deque
from dataclasses import asdict
from pathlib import Path
from threading import Lock, Event, Thread
from uuid import uuid4

from cyue_mcp.providers import create_transport
from cyue_mcp.providers.core import ConnectorError, descriptors


class Conversation:
    def __init__(self, owner, connector, config, sink, factory):
        self.connector = connector
        self.owner = owner
        self.id = str(uuid4())
        self.lock = Lock()
        self.events = deque(maxlen=512)
        self.closed = False
        def receive(channel, record):
            if channel == "event":
                self.events.append(record)
                if sink:
                    sink(channel, record)
        self.transport = factory(connector, config, sink=receive)
        # Lab captures are test-oriented lists. Bound retained observations in the host.
        self.transport.events = deque(maxlen=512)
        self.transport.raw = deque(maxlen=128)

    def snapshot(self):
        session = self.transport.session
        return {"conversationId": self.id,
                "session": asdict(session) if session else None,
                "status": self.transport.get_status(),
                "capabilities": self.transport.get_capabilities(),
                "access": self.transport.config.get("access", "read-only"),
                "attachmentCapabilities": {"text": True, "pdfText": True, "images": hasattr(self.transport, 'send_multimodal') and (getattr(self.transport, 'name', '') == 'app-server' or getattr(self.transport, 'id', '') == 'claude-code'), "fileReferences": getattr(self.transport, "name", "") == "app-server", "video": False, "binaryMesh": False},
                "events": list(self.events)}

    def send(self, text, *, images=None, cancellation=None, sink=None):
        if not self.lock.acquire(blocking=False):
            raise ConnectorError("SessionBusy", "A turn is already running in this conversation")
        stop = Event()
        monitor = None
        previous_sink = self.transport.sink
        try:
            if self.closed:
                raise ConnectorError("SessionInvalid", "Conversation closed")
            if sink:
                def receive(channel, record):
                    previous_sink(channel, record)
                    sink(channel, record)
                self.transport.sink = receive
            if cancellation:
                cancellation()
                def watch():
                    while not stop.wait(.05):
                        try: cancellation()
                        except Exception:
                            self.transport.cancel()
                            return
                monitor = Thread(target=watch, daemon=True)
                monitor.start()
            if images:
                if not hasattr(self.transport, 'send_multimodal'):
                    raise ConnectorError('UnsupportedAttachment','This transport has no image input. Choose a vision-capable Codex or Claude connector.')
                answer = self.transport.send_multimodal(text, images)
            else:
                answer = self.transport.send(text)
            return {**self.snapshot(), "text": answer}
        finally:
            stop.set()
            if monitor: monitor.join()
            self.transport.sink = previous_sink
            self.lock.release()

    def set_access(self, access):
        if access not in {'read-only', 'workspace-write'}:
            raise ConnectorError('InvalidInput', 'Unknown access policy')
        if self.connector not in {'codex', 'claude-code'}:
            raise ConnectorError('Unsupported', 'This connector has no adjustable file editing policy')
        if not self.lock.acquire(blocking=False):
            raise ConnectorError('SessionBusy', 'Wait for the current turn before changing permissions')
        try:
            if self.closed:
                raise ConnectorError('SessionInvalid', 'Conversation closed')
            # These adapters build their enforced policy for each turn, preserving native history.
            self.transport.config['access'] = access
            return self.snapshot()
        finally:
            self.lock.release()

    def close(self):
        # Cancel wakes blocked IO; wait until send truly exits before disconnect/removal.
        self.closed = True
        self.transport.cancel()
        with self.lock:
            self.transport.disconnect()


class ConnectorService:
    def __init__(self, workspace, *, factory=create_transport, catalog=descriptors, project_root=None):
        self.workspace = Path(workspace).resolve()
        self.workspace.mkdir(parents=True, exist_ok=True)
        self.project_root = Path(project_root).resolve() if project_root else self.workspace
        self.factory, self.catalog = factory, catalog
        self._lock = Lock()
        self._conversations = {}
        self._closing = False

    def list_connectors(self):
        return [{**d, "available": any(t["implemented"] for t in d["transports"])}
                for d in self.catalog().values()]

    def _owned(self, owner, conversation_id):
        with self._lock:
            item = self._conversations.get(conversation_id)
        if item is None or item.owner != owner:
            raise ConnectorError("SessionInvalid", "Unknown conversation for this authenticated owner")
        return item

    def probe(self, connector, *, model="", transport="", executable="", endpoint="", api_key_env="", timeout=20):
        """Check discovery/authentication without creating a conversation or sending a turn."""
        import time
        started=time.monotonic()
        descriptor=self.catalog().get(connector)
        if not descriptor:
            return {"ready":False,"status":"Unsupported","message":"Unknown connector", "models":[]}
        selected=transport or descriptor['preferred_transport']
        if not any(t['id']==selected and t['implemented'] for t in descriptor['transports']):
            return {"ready":False,"status":"Unsupported","message":"This transport is not implemented", "models":[]}
        config={"cwd":str(self.workspace),"transport":selected,"timeout":timeout}
        config.update(endpoint=endpoint, api_key_env=api_key_env)
        if executable:config['executable']=executable
        instance=self.factory(connector,config,sink=lambda *_:None)
        try:
            discovery=instance.detect()
            if discovery.get('status')!='Installed':
                return {"ready":False,"status":discovery.get('status','Missing'),"message":discovery.get('error','Executable missing'),"discovery":discovery,"models":[]}
            connection=instance.connect()
            authenticated=connection.get('authenticated')
            ready=authenticated is True
            status='Ready' if ready else ('Sign in required' if authenticated is False else 'Authentication unverified')
            message='Connection verified; model execution is checked when sending.' if ready else 'Sign in with the provider CLI, then click Test again.'
            models=[]
            if ready or authenticated == "unknown":
                try:models=instance.list_models()
                except ConnectorError:
                    # Codex CLI has no catalog command. Query its installed App Server
                    # for models only; this does not create a conversation or infer.
                    if connector == 'codex' and selected == 'cli-jsonl':
                        discovery_transport = self.factory(connector,{**config,'transport':'app-server'},sink=lambda *_:None)
                        try:
                            if discovery_transport.connect().get('authenticated') is True:
                                models=discovery_transport.list_models()
                        except ConnectorError: pass
                        finally: discovery_transport.disconnect()
            return {"ready":ready,"status":status,"message":message,"discovery":discovery,"models":models,"elapsedMs":int((time.monotonic()-started)*1000)}
        except ConnectorError as exc:
            return {"ready":False,"status":exc.code,"message":str(exc),"models":[]}
        except (OSError,ValueError) as exc:
            return {"ready":False,"status":"Connection failed","message":str(exc),"models":[]}
        finally:
            instance.disconnect()

    def open(self, owner, connector, *, model="", transport="", executable="", endpoint="", api_key_env="", timeout=120, access="read-only", sink=None):
        if not owner:
            raise ConnectorError("AccessDenied", "Authenticated owner required")
        descriptor = self.catalog().get(connector)
        if not descriptor:
            raise ConnectorError("Unsupported", "Unknown connector")
        selected = transport or descriptor["preferred_transport"]
        if not any(t["id"] == selected and t["implemented"] for t in descriptor["transports"]):
            raise ConnectorError("Unsupported", "This connector transport is not implemented by the SDK")
        if access not in {'read-only', 'workspace-write'}:
            raise ConnectorError('InvalidInput', 'Unknown access policy')
        if access == 'workspace-write' and connector not in {'codex', 'claude-code'}:
            raise ConnectorError('Unsupported', 'This connector does not provide file editing tools')
        config = {"cwd": str(self.project_root), "timeout": timeout, "transport": selected, "access":access}
        config.update(endpoint=endpoint, api_key_env=api_key_env)
        if executable:
            config["executable"] = executable
        if connector == 'codex' and selected == 'cli-jsonl' and not model:
            info = self.probe(connector, transport=selected, executable=executable, timeout=min(timeout,20))
            default = next((m for m in info.get('models',[]) if m.get('isDefault')), None)
            if default: model = default.get('model') or default.get('id') or ''
        item = Conversation(owner, connector, config, sink, self.factory)
        try:
            discovery = item.transport.detect()
            if discovery.get("status") != "Installed":
                raise ConnectorError(discovery.get("code", "ExecutableMissing"), str(discovery.get("error", discovery)))
            connection = item.transport.connect()
            if connection.get("authenticated") is False:
                raise ConnectorError("AuthenticationRequired", "Sign in using the connector's native login")
            item.transport.start_session(model=model)
            with self._lock:
                if self._closing: raise ConnectorError("SessionInvalid", "Service shutting down")
                self._conversations[item.id] = item
            return {**item.snapshot(), "discovery": discovery, "connection": connection}
        except BaseException:
            item.close()
            raise

    def send(self, owner, conversation_id, text, **kwargs):
        return self._owned(owner, conversation_id).send(text, **kwargs)

    def set_access(self, owner, conversation_id, access):
        return self._owned(owner, conversation_id).set_access(access)

    def snapshot(self, owner, conversation_id):
        return self._owned(owner, conversation_id).snapshot()

    def cancel(self, owner, conversation_id):
        self._owned(owner, conversation_id).transport.cancel()

    def close(self, owner, conversation_id):
        item = self._owned(owner, conversation_id)
        item.close()
        with self._lock:
            self._conversations.pop(conversation_id, None)

    def shutdown(self):
        with self._lock:
            self._closing = True
            items = list(self._conversations.values())
        for item in items:
            self.close(item.owner, item.id)
