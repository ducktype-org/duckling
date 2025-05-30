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
        for info in diag.hl_messages:
            hl_info_map.setdefault(info.tag, []).append(info.message)
    return hl_info_map


@app.route('/')
def index():
    view_resp = view_stub.GetView(view_pb2.ViewRequest())
    return render_template(
        'base.html',
        hl_info_map=get_hl_info_map(view_resp),
        diagnostics=view_resp.diagnostics,
        side_notes=view_resp.side_notes
    )


@app.route('/click')
def handle_click():
    component_id = int(request.args.get('component_id', '0'))
    click_type = request.args.get('click_type', 'CLICK')
    try:
        click_enum = getattr(view_pb2, click_type)
    except AttributeError:
        click_enum = view_pb2.CLICK

    req = view_pb2.ClickRequest(component_id=component_id, click_type=click_enum)
    resp = view_stub.Click(req)
    return resp.status


@app.route('/close')
def handle_close():
    side_note_id = int(request.args.get('side_note_id', '0'))
    req = view_pb2.CloseSideNoteRequest(side_note_id=side_note_id)
    resp = view_stub.CloseSideNote(req)
    return resp.status


if __name__ == '__main__':
    app.run(host='', port=8080)
