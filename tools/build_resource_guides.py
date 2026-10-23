"""Render the beta's two maintained Markdown guides (pip install Markdown)."""
from pathlib import Path
import markdown
root=Path(__file__).resolve().parents[1]
for name in ['SD-RESOURCES','WARDRIVE-UPLOAD-DESIGN']:
    body=markdown.markdown((root/'docs'/f'{name}.md').read_text(encoding='utf-8'),extensions=['tables','fenced_code'])
    body=body.replace('href="WARDRIVE-UPLOAD-DESIGN.md"','href="WARDRIVE-UPLOAD-DESIGN.html"')
    page='<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>'+name+' · Dragon Detector</title><style>body{max-width:960px;margin:2rem auto;padding:1rem;background:#111820;color:#e8f2ee;font:16px/1.65 system-ui}pre{padding:1rem;background:#061b13;overflow:auto}code{overflow-wrap:anywhere}a{color:#81e1b2}table{border-collapse:collapse;width:100%;font-size:.9rem}td,th{padding:.6rem;border:1px solid #365047;text-align:left}h1,h2{line-height:1.2}</style></head><body><a href="../index.html#install">Back to installer</a>'+body+'</body></html>'
    out=root/'web/guides'/f'{name}.html';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(page,encoding='utf-8')
