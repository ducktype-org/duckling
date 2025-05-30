from flask import Flask, request, render_template
import grpc
import view_pb2
import view_pb2_grpc

app = Flask(__name__)
channel = grpc.insecure_channel('localhost:50051')
view_stub = view_pb2_grpc.ViewServiceStub(channel)


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
    side_notes = []
    for path in side_paths:
        side_notes.extend(path.infos)
    return side_notes


@app.route('/')
def index():
    view_resp = view_stub.GetView(view_pb2.ViewRequest())
    return render_template(
        'base.html',
        hl_info_map=get_hl_info_map(view_resp),
        diagnostics=view_resp.diagnostics,
        side_notes=extract_side_notes(view_resp.side_paths)
    )


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
