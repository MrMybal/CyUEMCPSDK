import json
from pathlib import Path
import pytest
from PIL import Image
from cyue_mcp.attachments import prepare, claude_message
from cyue_mcp.providers.core import ConnectorError
from cyue_mcp.connectors import ConnectorService
from test_connectors import Fake, CATALOG

def test_image_normalization_and_text(tmp_path):
 source=tmp_path/'source';source.mkdir();work=tmp_path/'job';work.mkdir()
 pic=source/'image.png';Image.new('RGB',(8,8),'blue').save(pic)
 text=source/'unicode é.txt';text.write_text('Bonjour 世界','utf-8')
 note,images,generated=prepare([{'path':str(pic)},{'path':str(text)}],work)
 assert 'Bonjour 世界' in note and pic.exists()
 assert Path(images[0]['path']).parent==work
 data=json.loads(claude_message('question',images))
 assert data['message']['content'][1]['source']['media_type']=='image/png'
 for file in generated:file.unlink()
 assert pic.exists()

def test_failed_attachment_removes_normalized_image(tmp_path):
 pic=tmp_path/'input.png';Image.new('RGB',(8,8)).save(pic)
 bad=tmp_path/'movie.mp4';bad.write_bytes(b'\0binary')
 with pytest.raises(ConnectorError,match='cannot ingest'):
  prepare([{'path':str(pic)},{'path':str(bad)}],tmp_path)
 assert not (tmp_path/'attachment-image-0.png').exists()
 assert pic.exists() and bad.exists()

def test_no_silent_truncation(tmp_path):
 text=tmp_path/'long.txt';text.write_text('a'*180001)
 with pytest.raises(ConnectorError,match='exceeds'):prepare([{'path':str(text)}],tmp_path)

def test_false_image_and_empty_pdf(tmp_path):
 pic=tmp_path/'false.png';pic.write_text('not an image')
 with pytest.raises(ConnectorError,match='Cannot decode'):prepare([{'path':str(pic)}],tmp_path)

def test_binary_reference_is_explicit_and_opt_in(tmp_path):
 file=tmp_path/'mesh.glb';file.write_bytes(b'glTF\0binary')
 text,images,_=prepare([{'path':str(file)}],tmp_path,file_references=True)
 assert str(file.resolve()) in text and 'content not extracted' in text and not images
 with pytest.raises(ConnectorError):prepare([{'path':str(file)}],tmp_path)

def test_image_on_text_only_transport_and_other_owner(tmp_path):
 s=ConnectorService(tmp_path,factory=Fake,catalog=lambda:CATALOG)
 try:
  cid=s.open('owner','fake')['conversationId']
  with pytest.raises(ConnectorError,match='no image input'):s.send('owner',cid,'question',images=[{'path':'x'}])
  with pytest.raises(ConnectorError,match='authenticated owner'):s.send('other',cid,'question',images=[{'path':'x'}])
 finally:s.shutdown()
