"""Package verified firmware and local HTML guides for a static HTTPS host."""
from pathlib import Path
import argparse
import hashlib
import html
import json
import posixpath
import re
import shutil
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit

import markdown

ROOT = Path(__file__).resolve().parents[1]
REPO = 'https://github.com/valleytechsolutions/Dragon-Detector'
MAX_FILE_BYTES = 25 * 1024 * 1024
VERSION = re.search(r'#define DRAGON_VERSION "([0-9.]+)"', (ROOT / 'firmware/include/version.h').read_text()).group(1)


def validate_links(output):
    class Links(HTMLParser):
        def handle_starttag(self, tag, attributes):
            for name, value in attributes:
                if name not in ('href', 'src') or not value:
                    continue
                parsed = urlsplit(value)
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                path = unquote(parsed.path)
                target = (output / path.lstrip('/')) if path.startswith('/') else self.source.parent / path
                target = target.resolve()
                if not target.is_relative_to(output) or not target.exists():
                    raise ValueError(f'Broken local link in {self.source.relative_to(output)}: {value}')

    for source in output.rglob('*.html'):
        parser = Links()
        parser.source = source
        parser.feed(source.read_text(encoding='utf-8'))


def build(output):
    output = output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('Choose an empty output directory.')
    if output == ROOT or ROOT in output.parents and output.relative_to(ROOT).parts[0] not in ('.build', '.work'):
        raise ValueError('Use an output directory outside source folders, such as .build/site.')
    output.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'web').rglob('*'):
        if not source.is_file() or source.is_symlink():
            continue
        if source.name == 'factory-0x0.bin' and source.parent.name != 'stopwatch':
            continue
        relative = source.relative_to(ROOT / 'web')
        if any(part.startswith('.') for part in relative.parts):
            continue
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    # USB installation uses split images. Keep the C152 merged image for M5Burner.
    for folder in (output / 'firmware').iterdir():
        records = {}
        for line in (folder / 'SHA256SUMS.txt').read_text().splitlines():
            digest, name = line.split()
            if name == 'factory-0x0.bin' and not (folder / name).exists():
                continue
            payload = (folder / name).read_bytes()
            if hashlib.sha256(payload).hexdigest() != digest:
                raise ValueError(f'Firmware checksum mismatch: {folder.name}/{name}')
            records[name] = digest
        (folder / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name, digest in records.items()))
        for mode in ('factory', 'update'):
            manifest = json.loads((folder / f'{mode}.json').read_text())
            for part in manifest['builds'][0]['parts']:
                if part['path'] not in records:
                    raise ValueError(f'Unverified firmware part: {part["path"]}')

    sources = [ROOT / 'README.md', ROOT / 'CREDITS.md', ROOT / 'SECURITY.md', *sorted((ROOT / 'docs').rglob('*.md'))]
    guides = {source.resolve(): Path('guides') / f'{source.stem}.html' for source in sources}

    def rewrite_link(value, source, destination):
        value = html.unescape(value)
        original = value
        value = value.replace('https://git.churchofmalware.org/Valleytech/Dragon-Detector', REPO).replace('/src/branch/main/', '/blob/main/')
        parsed = urlsplit(value)
        for site in ('https://valleytechsolutions.github.io/Dragon-Detector/', 'https://valleytech-dragon-detector.pages.dev/'):
            if value.startswith(site):
                target = urlsplit(value[len(site):])
                result = posixpath.relpath(target.path or 'index.html', destination.parent.as_posix())
                if target.query:
                    result += '?' + target.query
                if target.fragment:
                    result += '#' + target.fragment
                return html.escape(result, quote=True)
        if value.startswith(REPO + '/blob/main/'):
            path = ROOT / unquote(value.split('/blob/main/', 1)[1].split('#')[0])
        elif parsed.path in ('/valleytechsolutions/Dragon-Detector/releases', '/valleytechsolutions/Dragon-Detector/releases/latest', f'/valleytechsolutions/Dragon-Detector/releases/tag/v{VERSION}') and parsed.netloc == 'github.com':
            path = ROOT / f'docs/RELEASE-{VERSION}.md'
        elif value.startswith(REPO + '/wiki/Clock-Setup'):
            path = ROOT / 'docs/CLOCK-SETUP.md'
        elif value.startswith(REPO + '/wiki/Spotpear-Clock-Setup'):
            path = ROOT / 'docs/SPOTPEAR-CLOCK.md'
        elif parsed.scheme or parsed.netloc or not parsed.path:
            return html.escape(original, quote=True)
        else:
            path = source.parent / unquote(parsed.path)
        path = path.resolve()
        if not path.is_relative_to(ROOT) or not path.is_file():
            return html.escape(original, quote=True)
        if path in guides:
            target = guides[path]
        elif path.is_relative_to(ROOT / 'web'):
            target = path.relative_to(ROOT / 'web')
        elif path.stat().st_size <= MAX_FILE_BYTES and not path.is_symlink():
            target = Path('reference') / path.relative_to(ROOT)
            (output / target).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, output / target)
        else:
            return html.escape(original, quote=True)
        result = posixpath.relpath(target.as_posix(), destination.parent.as_posix())
        if parsed.fragment:
            result += '#' + parsed.fragment
        return html.escape(result, quote=True)

    def rewrite_links(content, source, destination):
        return re.sub(r'\b(href|src)="([^"]*)"', lambda m: f'{m[1]}="{rewrite_link(m[2], source, destination)}"', content)

    css = '''body{margin:0;background:#07120e;color:#d9e8e0;font:16px/1.7 system-ui,sans-serif}main,nav{max-width:1000px;margin:auto;padding:24px}nav{border-bottom:1px solid #345344}a{color:#bde965}h1,h2,h3{color:#f4fff7;line-height:1.2}h1{font-size:2.5rem}h2{margin-top:2.4rem}img{max-width:100%;height:auto}table{display:block;overflow:auto;border-collapse:collapse}th,td{padding:12px;border:1px solid #345344}pre{overflow:auto;background:#11231a;padding:16px}code{font-size:.9em}blockquote{border-left:3px solid #bde965;margin-left:0;padding-left:18px}nav a{margin-right:20px}'''
    (output / 'guides').mkdir(exist_ok=True)
    for source, destination in guides.items():
        text = source.read_text(encoding='utf-8-sig')
        body = markdown.markdown(text, extensions=['tables', 'fenced_code', 'toc'])
        body = rewrite_links(body, source, destination)
        title = next((line[2:].strip() for line in text.splitlines() if line.startswith('# ')), source.stem)
        page = f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(title)} · Dragon Detector</title><style>{css}</style></head><body><nav><a href="../#install">Install firmware</a><a href="README.html">Overview</a><a href="HOSTING-STATUS.html">Hosting status</a><a href="../#theme-studio">Custom theme</a></nav><main>{body}</main></body></html>'
        (output / destination).write_text(page, encoding='utf-8')
    index = output / 'index.html'
    index.write_text(rewrite_links(index.read_text(encoding='utf-8'), ROOT / 'web/index.html', Path('index.html')), encoding='utf-8')
    downloads = ['<h1>Firmware downloads</h1><p>Versions are listed separately for each board; beta and stable packages may differ. Use the web flasher for automatic installation and clock verification. Compare downloaded files with the SHA-256 list for the same board.</p>']
    for folder in sorted((output / 'firmware').iterdir()):
        info = json.loads((folder / 'factory.json').read_text())
        downloads.append('<h2>' + html.escape(info['name'].removesuffix(' (factory)')) + '</h2>')
        downloads.append('<p>Version ' + html.escape(info['version']) + '</p>')
        metadata = json.loads((folder / 'build-info.json').read_text(encoding='utf-8'))
        if metadata.get('hardware_validation') in ('pending','new-sd-gps-features-pending'):
            downloads.append('<p><strong>Beta — new SD/GPS features await physical testing.</strong> '
                             '<a href="WAVESHARE-AMOLED-175.html">Installation and arrival checklist</a>.</p>')
        downloads.append('<ul>')
        for file in sorted(folder.iterdir()):
            downloads.append(f'<li><a href="../firmware/{folder.name}/{file.name}">{html.escape(file.name)}</a> ({file.stat().st_size:,} bytes)</li>')
        downloads.append('</ul>')
    (output / 'guides/DOWNLOADS.html').write_text(f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Firmware downloads · Dragon Detector</title><style>{css}</style></head><body><nav><a href="../#install">Install firmware</a><a href="FLASHING.html">Installation guide</a></nav><main>{"".join(downloads)}</main></body></html>', encoding='utf-8')
    too_large = [str(p.relative_to(output)) for p in output.rglob('*') if p.is_file() and p.stat().st_size > MAX_FILE_BYTES]
    if too_large:
        raise ValueError(f'Files exceed static host limit: {too_large}')
    validate_links(output)
    print(f'Prepared {len(guides)} guides and verified firmware packages in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    build(parser.parse_args().output)
