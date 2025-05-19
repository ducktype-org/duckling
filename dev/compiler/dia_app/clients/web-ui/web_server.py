import grpc
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import view_pb2
import view_pb2_grpc

channel = grpc.insecure_channel('localhost:50051')
view_stub = view_pb2_grpc.ViewServiceStub(channel)


def generate_html(response):
    hl_info_map = {}
    for diag in response.diagnostics:
        for info in diag.hl_messages:
            hl_info_map.setdefault(info.tag, []).append(info.message)

    # TODO: consider caching the result to avoid disk access
    with open("template.html", "r", encoding="utf-8") as f:
        html = f.read()
        # TODO: consider using templating engine such as jinja2
        html = html.replace("HLINFO_PLACEHOLDER", json.dumps(hl_info_map))

    main_sections = []
    for diag in response.diagnostics:
        main_sections.append('<div class="diagnostic">')
        meta = []
        if diag.metadata.error_code:
            meta.append(f"Error: {diag.metadata.error_code}")
        if diag.metadata.file_info:
            meta.append(f"File: {diag.metadata.file_info}")
        if meta:
            main_sections.append('<div class="metadata">' + ' | '.join(meta) + '</div>')
        for sec in diag.sections:
            main_sections.append('<div class="section">')
            if sec.HasField('text_section'):
                main_sections.append(render_component(sec.text_section.root))
            elif sec.HasField('no_hl_text_section'):
                main_sections.append(render_nohl_component(sec.no_hl_text_section.root))
            elif sec.HasField('code_section'):
                main_sections.append(render_code_section(sec.code_section))
            main_sections.append('</div>')
        main_sections.append('</div>')
    main_html = "\n".join(main_sections)

    side_sections = []
    for idx, note in enumerate(response.side_notes):
        side_sections.append(f'<div class="side-note" id="side-{idx}">')
        for diag in note.diagnostics:
            for sec in diag.sections:
                side_sections.append('<div class="section">')
                if sec.HasField('text_section'):
                    side_sections.append(render_component(sec.text_section.root))
                elif sec.HasField('no_hl_text_section'):
                    side_sections.append(render_nohl_component(sec.no_hl_text_section.root))
                elif sec.HasField('code_section'):
                    side_sections.append(render_code_section(sec.code_section))
                side_sections.append('</div>')
        side_sections.append(f'<span class="clickable close-note" onclick="closeSide({idx})">Close Note</span>')
        side_sections.append('</div>')
    side_html = "\n".join(side_sections)

    html = html.replace('<div id="main">', f'<div id="main">\n{main_html}')
    html = html.replace('<div id="side">', f'<div id="side">\n{side_html}')

    return html


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
        return ''.join(render_component(c, tags) for c in comp.concat_component.components)

    if kind == 'interactive_component':
        ic = comp.interactive_component
        inner = render_component(ic.primary_component, inherited_tags)
        return f'<span class="clickable" onclick="handleClick(event, {ic.component_id})">{inner}</span>'

    if kind == 'side_entry_component':
        se = comp.side_entry_component
        grp_attr = f' data-groups="{' '.join(str(t) for t in inherited_tags)}"' if inherited_tags else ''
        return f'<span class="clickable" onclick="handleClick(event, {se.side_entry_id})"{grp_attr}>[note]</span>'

    return '<span>Unknown Component</span>'


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
        return f'<span class="clickable" onclick="handleClick(event, {ic.component_id})">{inner}</span>'
    if kind == 'side_entry_component':
        se = comp.side_entry_component
        return f'<span class="clickable" onclick="handleClick(event, {se.side_entry_id})">[note]</span>'
    return '<span>Unknown NoHl</span>'


def render_code_section(code_sec):
    html = ['<div class="code-section">']
    for line in code_sec.lines:
        num = f"{line.line_number or ''}".rjust(4)
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
            ct = params.get('click_type', ['CLICK'])[0]
            try:
                click_enum = getattr(view_pb2, ct)
            except AttributeError:
                click_enum = view_pb2.CLICK

            req = view_pb2.ClickRequest(component_id=cid, click_type=click_enum)
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