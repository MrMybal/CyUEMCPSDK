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
