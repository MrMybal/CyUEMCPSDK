import json
from pathlib import Path
import pytest
from cyue_mcp.local import catalog
from cyue_mcp.connectors import ConnectorService
from cyue_mcp.providers.core import ConnectorError
from test_connectors import Fake


def test_every_connector_preset_uses_an_implemented_transport():
    definitions=list((Path(__file__).parents[1]/'Plugins/CyUEMCPSDK/Resources/Connectors').glob('*.json'))
    assert len(definitions)>=4
    ids=set()
    for file in definitions:
        data=json.loads(file.read_text())
        assert data['schemaVersion']==1 and data['id'] not in ids
        ids.add(data['id'])
        assert any(t['id']==data['transport'] and t['implemented'] for t in catalog()[data['provider']]['transports'])


def test_permissions_are_owned_by_the_conversation_and_use_host_project(tmp_path):
    descriptors=lambda:{'codex':{'id':'codex','preferred_transport':'app-server','transports':[{'id':'app-server','implemented':True}]}}
    project=tmp_path/'project';project.mkdir()
    service=ConnectorService(tmp_path/'runtime',project_root=project,factory=Fake,catalog=descriptors)
    try:
        read=service.open('owner','codex',access='read-only')
        write=service.open('owner','codex',access='workspace-write')
        assert read['conversationId']!=write['conversationId']
        assert service.snapshot('owner',read['conversationId'])['access']=='read-only'
        assert service.snapshot('owner',write['conversationId'])['access']=='workspace-write'
        assert service._owned('owner',write['conversationId']).transport.config['cwd']==str(project.resolve())
        service.close('owner',read['conversationId'])
        assert service.send('owner',write['conversationId'],'still here')['text']=='still here'
        with pytest.raises(ConnectorError): service.open('owner','codex',access='full-access')
    finally:service.shutdown()


def test_live_policy_change_preserves_history_and_is_owned(tmp_path):
    catalog=lambda:{'codex':{'preferred_transport':'app-server','transports':[{'id':'app-server','implemented':True}]}}
    service=ConnectorService(tmp_path,factory=Fake,catalog=catalog)
    try:
        cid=service.open('owner','codex')['conversationId']
        item=service._owned('owner',cid)
        native=item.transport.session
        service.send('owner',cid,'first')
        assert service.set_access('owner',cid,'workspace-write')['access']=='workspace-write'
        assert item.transport.session is native
        assert service.send('owner',cid,'second')['access']=='workspace-write'
        with pytest.raises(ConnectorError):service.set_access('other',cid,'read-only')
        with pytest.raises(ConnectorError):service.set_access('owner',cid,'full-access')
        item.lock.acquire()
        try:
            with pytest.raises(ConnectorError,match='current turn'):service.set_access('owner',cid,'read-only')
            assert item.transport.config['access']=='workspace-write'
        finally:item.lock.release()
        assert service.set_access('owner',cid,'read-only')['access']=='read-only'
        assert item.transport.calls==['connect','start','first','second']
    finally:service.shutdown()


def test_context_and_asset_references_are_metadata_not_copied_files(tmp_path):
    from types import SimpleNamespace
    from cyue_mcp.service import Companion
    from test_connectors import CATALOG
    companion=Companion.__new__(Companion)
    companion.connectors=ConnectorService(tmp_path,factory=Fake,catalog=lambda:CATALOG)
    owner='client:task'
    cid=companion.connectors.open(owner,'fake')['conversationId']
    ctx=SimpleNamespace(job_id='job',workspace=tmp_path,
        runtime=SimpleNamespace(store=SimpleNamespace(all=lambda _: {'job':{'clientId':'client','sessionId':'task'}})),
        cancellation=SimpleNamespace(check=lambda:None),emit=lambda *args:None)
    try:
        result=companion._assistant(ctx,'assistant_send',{'conversationId':cid,'text':'Explain this asset',
            'context':{'id':'plugin-a:document-a','label':'Material editor','asset':'/Game/Materials/Test.Test'},
            'references':[{'path':'/Game/Meshes/Cube.Cube','name':'Cube','class':'StaticMesh'}]})
        assert '/Game/Meshes/Cube.Cube' in result['text']
        assert 'plugin-a:document-a' in result['text']
        assert not list(tmp_path.rglob('*.uasset'))
        assert companion.connectors._owned(owner,cid).transport.config['cwd']==str(tmp_path.resolve())
    finally:companion.connectors.shutdown()
