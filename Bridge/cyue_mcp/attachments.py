"""Bounded, job-private attachment decoding. No network fetches or silent truncation."""
import base64
import io
import json
from pathlib import Path
from PIL import Image, ImageOps, UnidentifiedImageError
from pypdf import PdfReader
from .providers.core import ConnectorError

MAX_FILE = 32 * 1024 * 1024
MAX_TEXT = 180000
Image.MAX_IMAGE_PIXELS = 20000000

def prepare(files, workspace, *, file_references=False):
    texts, images, generated = [], [], []
    try:
        for index, item in enumerate(files):
            path = Path(item['path'])
            if not path.is_file() or path.stat().st_size > MAX_FILE:
                raise ConnectorError('AttachmentSize', 'Attachments must be regular files smaller than 32 MiB.')
            name = Path(item.get('name') or path.name).name
            suffix = path.suffix.lower()
            if suffix in {'.png','.jpg','.jpeg','.webp','.gif','.bmp'}:
                try:
                    with Image.open(path) as image:
                        image.seek(0)
                        if image.width * image.height > 20000000:
                            raise ConnectorError('AttachmentSize', 'Image exceeds 20 million pixels.')
                        image = ImageOps.exif_transpose(image).convert('RGB')
                        target = Path(workspace) / f'attachment-image-{index}.png'
                        generated.append(target)
                        image.save(target, 'PNG')
                    images.append({'path': str(target), 'mime': 'image/png', 'name': name})
                    texts.append(f'Attached image: {name}' + (' (first frame only)' if suffix == '.gif' else ''))
                except (UnidentifiedImageError, OSError, Image.DecompressionBombError) as exc:
                    raise ConnectorError('InvalidImage', f'Cannot decode image {name}: {exc}') from exc
            elif suffix == '.pdf':
                reader = PdfReader(path)
                if reader.is_encrypted:
                    raise ConnectorError('EncryptedDocument', 'Decrypt the PDF before sending it.')
                chunks=[]
                for page in reader.pages:
                    chunks.append(page.extract_text() or '')
                    if sum(map(len,chunks)) > MAX_TEXT:
                        raise ConnectorError('AttachmentSize', 'PDF text exceeds 180000 characters. Split it before sending.')
                content='\n'.join(chunks)
                if not content.strip():
                    raise ConnectorError('UnsupportedAttachment', 'This PDF has no extractable text; send its pages as images.')
                texts.append(f'PDF {name} (extracted text, no page images):\n{content}')
            else:
                raw=path.read_bytes()
                try:
                    content=raw.decode('utf-8-sig')
                    if '\0' in content:raise ValueError('binary content')
                except (UnicodeError,ValueError) as exc:
                    if file_references:
                        texts.append(f'Original attached file {name}: {path.resolve()} (read-only local reference; content not extracted).')
                        continue
                    raise ConnectorError('UnsupportedAttachment', f'{name}: this connector cannot ingest this binary format. Images, UTF-8 text/code and text PDFs are supported; video/audio/3D previews do not imply model support.') from exc
                texts.append(f'File {name}:\n{content}')
        if sum(map(len,texts)) > MAX_TEXT:
            raise ConnectorError('AttachmentSize', 'Combined attachment text exceeds 180000 characters; split the files.')
        return '\n\n'.join(texts), images, generated
    except BaseException:
        for file in generated:file.unlink(missing_ok=True)
        raise

def claude_message(text, images):
    content=[{'type':'text','text':text}]
    for image in images:
        data=Path(image['path']).read_bytes()
        if len(data)>5*1024*1024:
            raise ConnectorError('AttachmentSize','Claude image exceeds 5 MiB after decoding. Use a smaller image.')
        content.append({'type':'image','source':{'type':'base64','media_type':image['mime'],'data':base64.b64encode(data).decode('ascii')}})
    return json.dumps({'type':'user','message':{'role':'user','content':content}})+'\n'
