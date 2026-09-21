from pathlib import Path
import json,tempfile
from PIL import Image,ImageDraw
from cyue_mcp.service import Companion
class Native:
 info={'pid':1}
 def describe(self):
  return {'schemaVersion':1,'protocolVersion':'1.0','id':'AttachmentTest','name':'Attachments','version':'0.1.2','description':'test','operations':[],'platforms':[{'os':'Windows','architecture':'x64','status':'Supported'}],'concurrency':{'policy':'Concurrent','maxWorkers':4}}
results={}
with tempfile.TemporaryDirectory(prefix='cyue-live-attachments-') as folder:
 root=Path(folder);image=Image.new('RGB',(256,256),'white');ImageDraw.Draw(image).rectangle((45,45,210,210),fill='blue');picture=root/'image outside tool.png';image.save(picture);note=root/'note.txt';note.write_text('attachment-check = CYUE_ATTACH_4972','utf-8')
 for provider in ['codex','claude-code']:
  c=Companion(Native(),root/provider)
  try:
   token=c.runtime.create_client('test',['assistant.use'])['token'];owner=c.runtime.authenticate(token);c.runtime.start();session=c.runtime.create_session(owner)['sessionId']
   def invoke(op,params):
    job=c.runtime.submit(owner,session,op,params);final=c.runtime.wait(owner,job['jobId'],180);assert final['state']=='Completed',final.get('error');return final
   opened=invoke('assistant_open',{'connector':provider,'timeout':150})['outputs'];cid=opened['conversationId']
   reply=invoke('assistant_send',{'conversationId':cid,'text':'Name the color of the square in the attached image, then the exact attachment-check value from the text file. One short line.','files':[{'path':str(picture),'name':picture.name},{'path':str(note),'name':note.name}]})
   text=reply['outputs']['text'];assert 'blue' in text.lower() and 'CYUE_ATTACH_4972' in text,text
   c.runtime.finish_task(owner,session);c.reap_conversations();assert picture.exists() and note.exists()
   results[provider]={'passed':True,'reply':text,'originalsPreserved':True,'attachmentCapabilities':opened['attachmentCapabilities']};print('PASS',provider,text,flush=True)
  finally:c.close()
Path('Reports/live-attachments.json').write_text(json.dumps(results,indent=2),'utf-8')
