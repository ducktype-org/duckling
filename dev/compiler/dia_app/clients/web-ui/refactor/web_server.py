from flask import Flask, request, render_template
import grpc
import view_pb2
import view_pb2_grpc
import logging

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

app = Flask(__name__)
channel = grpc.insecure_channel('localhost:50051')
_view_stub = view_pb2_grpc.ViewServiceStub(channel)

def log_warning(message, *args):
    logger.warning(message, *args)
    return ''  # Return empty string to not affect template output

def log_debug(message, *args):
    logger.debug(message, *args)
    return ''  # Return empty string to not affect template output

# Add logging filters to Jinja2 environment
app.jinja_env.filters['log_warning'] = log_warning
app.jinja_env.filters['log_debug'] = log_debug

# Add logger to Jinja2 environment
app.jinja_env.globals['logger'] = logger

class ViewServiceStubSpy:
    def __init__(self, real_stub):
        self._real_stub = real_stub
        self.calls = []

    def GetView(self, *args, **kwargs):
        result = self._real_stub.GetView(*args, **kwargs)
        print("result: ", result)
        return result

    def __getattr__(self, name):
        return getattr(self._real_stub, name)


view_stub = ViewServiceStubSpy(_view_stub)


def spy(g, on=True):
    def result_fun(*args):
        if on:
            print(*args)
        return g(*args)
    return result_fun

@app.template_filter('enumerate')
def enumerate_filter(iterable):
    return enumerate(iterable)


@app.template_filter('rjust')
def rjust_filter(value, width):
    return str(value).rjust(width)


def get_hl_info_map(response):
    hl_info_map = {}
    for diag in response.diagnostics:
        for info in diag.infos:
            for section in info.sections:
                if section.HasField('code_section'):
                    for hl_message in section.code_section.hl_messages:
                        # Convert NoHlComponent message to string representation
                        message_str = ""
                        if hl_message.message.HasField('text_component'):
                            message_str = hl_message.message.text_component.content
                        elif hl_message.message.HasField('code_component'):
                            message_str = hl_message.message.code_component.content
                        # Add the string message to the map
                        hl_info_map.setdefault(str(hl_message.tag), []).append(message_str)
    return hl_info_map


def extract_side_notes(side_paths):
    logger.debug(f"Extracting side notes from paths. Number of paths: {len(side_paths)}")
    side_notes = []
    for i, path in enumerate(side_paths):
        logger.debug(f"Processing path {i}: Number of infos: {len(path.infos)}")
        side_notes.extend(path.infos)
    logger.debug(f"Total number of extracted side notes: {len(side_notes)}")
    return side_notes

@app.route('/')
def index():
    logger.debug("Getting view from backend...")
    view_resp = view_stub.GetView(view_pb2.ViewRequest())
    logger.debug(f"Received view response. Number of side_paths: {len(view_resp.side_paths)}")
    
    side_notes = extract_side_notes(view_resp.side_paths)
    logger.debug("Side notes content:")
    for i, note in enumerate(side_notes):
        logger.debug(f"Note {i}:")
        if note.header:
            logger.debug(f"  Has header")
        logger.debug(f"  Number of sections: {len(note.sections)}")
        logger.debug(f"  Number of edges: {len(note.edges) if note.edges else 0}")
        logger.debug(f"  Side info ID: {note.side_info_id}")
    
    return render_template(
        'base.html',
        hl_info_map=get_hl_info_map(view_resp),
        diagnostics=view_resp.diagnostics,
        side_notes=side_notes
    )

@spy
@app.route('/click')
def handle_click():
    component_id = int(request.args.get('component_id', '0'))
    click_type = request.args.get('click_type', 'CLICK')
    try:
        click_enum = getattr(view_pb2.ClickType, click_type)
    except AttributeError:
        click_enum = view_pb2.ClickType.CLICK

    req = view_pb2.ClickRequest(component_id=component_id, click_type=click_enum)
    resp = view_stub.Click(req)
    return resp.status


@app.route('/close')
def handle_close():
    side_info_id = int(request.args.get('side_note_id', '0'))
    req = view_pb2.CloseSideInfoRequest(side_info_id=side_info_id)
    resp = view_stub.CloseSideInfo(req)
    return resp.status


@app.route('/edge')
def handle_edge():
    side_info_id = int(request.args.get('side_info_id', '0'))
    edge_id = int(request.args.get('edge_id', '0'))
    req = view_pb2.EdgeRequest(side_info_id=side_info_id, edge_id=edge_id)
    resp = view_stub.GetEdge(req)
    return resp.status


if __name__ == '__main__':
    app.run(host='', port=8080)
