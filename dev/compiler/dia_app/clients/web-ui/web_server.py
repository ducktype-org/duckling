import grpc
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import view_pb2
import view_pb2_grpc

channel = grpc.insecure_channel('localhost:50051')
view_stub = view_pb2_grpc.ViewServiceStub(channel)

import json

def generate_html(response):
    hl_info_map = {}
    for diag in response.diagnostics:
        for info in diag.hl_messages:
            hl_info_map.setdefault(info.tag, []).append(info.message)

    html = [
        '<!DOCTYPE html>',
        '<html>',
        '<head>',
        '  <meta charset="utf-8">',
        '  <title>View from Web Server</title>',
        '  <style>',
        '    html, body { height: 100%; margin: 0; }',
        '    body { font-family: Arial, sans-serif; display: flex; height: 100vh; }',
        '    #main { flex: 3; padding: 20px; background: #f7f7f7; overflow-y: auto; }',
        '    #side { flex: 1; padding: 20px; background: #eef; border-left: 1px solid #ccc; overflow-y: auto; }',
        '    .section { margin-bottom: 10px; }',
        '    .code { background: #f0f0f0; padding: 5px; border-radius: 3px; font-family: monospace; white-space: pre; display: inline-block; }',
        '    .code-line { margin: 0; padding: 2px 0; }',
        '    .lineno { color: #999; padding-right: 10px; }',
        '    .clickable { cursor: pointer; }',
        '    .clickable:hover { text-decoration: underline; }',
        '    .highlight { background-color: #fff176; }',
        '    .hl-message { background-color: #ffcdd2; padding: 4px 6px; border-radius: 3px; margin: 2px 0; font-size: 0.9em; }',
        '    .close-note { display: inline-block; margin-top: 5px; color: inherit; }',
        '    .side-note:not(:last-child) { border-bottom: 1px solid #ccc; margin-bottom: 10px; padding-bottom: 10px; }',
        '  </style>',
        '  <script>',
        f'    const hlInfo = {json.dumps(hl_info_map)};',
        '    document.addEventListener("DOMContentLoaded", () => {',
        '      document.querySelectorAll("[data-groups]").forEach(el => {',
        '        // Mouseover: highlight & show messages',
        '        el.addEventListener("mouseover", () => {',
        '          let groups = el.getAttribute("data-groups")',
        '                       .split(" ")',
        '                       .map(g => parseInt(g, 10))',
        '                       .filter((v, i, a) => a.indexOf(v) === i)',
        '                       .sort((a, b) => a - b)',
        '                       .map(String);',
        '          // Highlight each element in the groups',
        '          groups.forEach(g => {',
        '            document.querySelectorAll(`[data-groups~="${g}"]`).forEach(x => x.classList.add("highlight"));',
        '          });',
        '          // Collect and display messages',
        '          let allMsgs = [];',
        '          groups.forEach(g => { if (hlInfo[g]) allMsgs = allMsgs.concat(hlInfo[g]); });',
        '          if (allMsgs.length > 0) {',
        '            const sectionEl = el.closest(".section");',
        '            const msgSection = document.createElement("div");',
        '            msgSection.className = "section";',
        '            msgSection.setAttribute("data-groups", groups.join(" "));',
        '            allMsgs.forEach(m => {',
        '              const dm = document.createElement("div");',
        '              dm.className = "hl-message";',
        '              dm.textContent = m;',
        '              msgSection.appendChild(dm);',
        '            });',
        '            sectionEl.parentNode.insertBefore(msgSection, sectionEl.nextSibling);',
        '          }',
        '        });',
        '        // Mouseout: remove highlights & messages',
        '        el.addEventListener("mouseout", () => {',
        '          let groups = el.getAttribute("data-groups")',
        '                       .split(" ")',
        '                       .map(g => parseInt(g, 10))',
        '                       .filter((v, i, a) => a.indexOf(v) === i)',
        '                       .sort((a, b) => a - b)',
        '                       .map(String);',
        '          groups.forEach(g => {',
        '            document.querySelectorAll(`[data-groups~="${g}"]`).forEach(x => x.classList.remove("highlight"));',
        '          });',
        '          document.querySelectorAll(`.section[data-groups~="${groups[0]}"]`).forEach(n => n.remove());',
        '        });',
        '      });',
        '    });',
        '    function handleClick(id) {',
        '      fetch(`/click?component_id=${id}`)',
        '        .then(r => r.text())',
        '        .then(_ => window.location.reload());',
        '    }',
        '    function closeSide(id) {',
        '      fetch(`/close?side_note_id=${id}`)',
        '        .then(r => r.text())',
        '        .then(_ => window.location.reload());',
        '    }',
        '  </script>',
        '</head>',
        '<body>',
        '<div id="main">'
    ]

    for diag in response.diagnostics:
        html.append('<div class="diagnostic">')
        meta = []
        if diag.metadata.error_code:
            meta.append(f"Error: {diag.metadata.error_code}")
        if diag.metadata.file_info:
            meta.append(f"File: {diag.metadata.file_info}")
        if meta:
            html.append('<div class="metadata">' + ' | '.join(meta) + '</div>')
        for sec in diag.sections:
            html.append('<div class="section">')
            if sec.HasField('text_section'):
                html.append(render_component(sec.text_section.root))
            elif sec.HasField('no_hl_text_section'):
                html.append(render_nohl_component(sec.no_hl_text_section.root))
            elif sec.HasField('code_section'):
                html.append(render_code_section(sec.code_section))
            html.append('</div>')
        html.append('</div>')

    html.append('</div>')

    html.append('<div id="side">')
    for idx, note in enumerate(response.side_notes):
        html.append(f'<div class="side-note" id="side-{idx}">')
        for diag in note.diagnostics:
            for sec in diag.sections:
                html.append('<div class="section">')
                if sec.HasField('text_section'):
                    html.append(render_component(sec.text_section.root))
                elif sec.HasField('no_hl_text_section'):
                    html.append(render_nohl_component(sec.no_hl_text_section.root))
                elif sec.HasField('code_section'):
                    html.append(render_code_section(sec.code_section))
                html.append('</div>')
        html.append(f'<span class="clickable close-note" onclick="closeSide({idx})">Close Note</span>')
        html.append('</div>')
    html.append('</div>')


    html.append('</body></html>')
    return '\n'.join(html)

def render_component(comp, inherited_tags=None):
    """
    Render a component, propagating any inherited highlight tags down into nested elements.
    """
    if inherited_tags is None:
        inherited_tags = []

    kind = comp.WhichOneof('component')
    if kind == 'text_component':
        txt = comp.text_component.content
        own_tags = list(comp.text_component.hl_tags)
        tags = inherited_tags + own_tags
        grp_attr = f' data-groups="{' '.join(str(t) for t in tags)}"' if tags else ''
        return f'<span{grp_attr}>{txt}</span>'

    if kind == 'code_component':
        code = comp.code_component.content
        own_tags = list(comp.code_component.hl_tags)
        tags = inherited_tags + own_tags
        grp_attr = f' data-groups="{' '.join(str(t) for t in tags)}"' if tags else ''
        return f'<span class="code"{grp_attr}>{code}</span>'

    if kind == 'concat_component':
        own_tags = list(comp.concat_component.hl_tags)
        tags = inherited_tags + own_tags
        html_pieces = []
        for child in comp.concat_component.components:
            html_pieces.append(render_component(child, tags))
        return ''.join(html_pieces)

    if kind == 'interactive_component':
        ic = comp.interactive_component
        own_tags = []
        tags = inherited_tags + own_tags
        inner = render_component(ic.primary_component, tags)
        return f'<span class="clickable" onclick="handleClick({ic.component_id})">{inner}</span>'

    if kind == 'side_entry_component':
        se = comp.side_entry_component
        grp_attr = f' data-groups="{' '.join(str(t) for t in inherited_tags)}"' if inherited_tags else ''
        return f'<span class="clickable" onclick="handleClick({se.side_entry_id})"{grp_attr}>[note]</span>'

    return '<span>Unknown</span>'

def render_nohl_component(comp):
    kind = comp.WhichOneof('component')
    if kind == 'text_component':
        return f'<span>{comp.text_component.content}</span>'

    if kind == 'code_component':
        return f'<span class="code">{comp.code_component.content}</span>'

    if kind == 'concat_component':
        return ''.join(render_nohl_component(c) for c in comp.concat_component.components)

    if kind == 'interactive_component':
        ic = comp.interactive_component
        inner = render_nohl_component(ic.primary_component)
        return f'<span class="clickable" onclick="handleClick({ic.component_id})">{inner}</span>'

    if kind == 'side_entry_component':
        se = comp.side_entry_component
        return f'<span class="clickable" onclick="handleClick({se.side_entry_id})">[note]</span>'

    return '<span>Unknown NoHl</span>'

def render_code_section(code_sec):
    html = ['<div class="code-section">']
    for line in code_sec.lines:
        num = f'{line.line_number or ""}'.rjust(4)
        content = render_component(line.content.root)
        html.append(f'<div class="code-line"><span class="lineno">{num}</span>{content}</div>')
    html.append('</div>')
    return ''.join(html)

class WebRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/click':
            params = parse_qs(parsed.query)
            cid = int(params.get('component_id', ['0'])[0])
            req = view_pb2.ClickRequest(component_id=cid, click_type=view_pb2.CLICK)
            resp = view_stub.Click(req)
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(resp.status.encode())
            return

        if parsed.path == '/close':
            params = parse_qs(parsed.query)
            sid = int(params.get('side_note_id', ['0'])[0])
            req = view_pb2.CloseSideNoteRequest(side_note_id=sid)
            resp = view_stub.CloseSideNote(req)
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(resp.status.encode())
            return

        view_resp = view_stub.GetView(view_pb2.ViewRequest())
        html = generate_html(view_resp)
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

def run_server(port=8080):
    server = HTTPServer(('', port), WebRequestHandler)
    print(f"Server running on port {port}")
    server.serve_forever()

if __name__ == '__main__':
    run_server(8080)
