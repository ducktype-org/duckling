import threading
import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc

view_lock = threading.Lock()
current_view = None
expanded = False

def update_view():
    global expanded
    components = []

    header_entry = view_pb2.TextEntry(
        text="Dynamic view example", 
        type=view_pb2.PLAIN,
        group_id=0
    )
    header_component = view_pb2.Component(
        text_component=view_pb2.TextComponent(entries=[header_entry]),
        id=0
    )
    components.append(header_component)

    toggle_entry = view_pb2.TextEntry(
        text="Click to toggle details", 
        type=view_pb2.PLAIN,
        group_id=1
    )
    toggle_component = view_pb2.Component(
        text_component=view_pb2.TextComponent(entries=[toggle_entry]),
        id=100
    )
    components.append(toggle_component)

    if expanded:
        details_entry = view_pb2.TextEntry(
            text="Detailed information is displayed here after clicking toggle.", 
            type=view_pb2.PLAIN,
            group_id=1
        )
        details_component = view_pb2.Component(
            text_component=view_pb2.TextComponent(entries=[details_entry]),
            id=0
        )
        components.append(details_component)

    group2_entry1 = view_pb2.TextEntry(
        text="Group 2: Entry A", 
        type=view_pb2.PLAIN,
        group_id=2
    )
    group2_entry2 = view_pb2.TextEntry(
        text="Group 2: Entry B", 
        type=view_pb2.PLAIN,
        group_id=2
    )
    group2_entry3 = view_pb2.TextEntry(
        text="print('Code in group 2')", 
        type=view_pb2.CODE,
        group_id=2
    )
    group2_component = view_pb2.Component(
        text_component=view_pb2.TextComponent(entries=[group2_entry1, group2_entry2, group2_entry3]),
        id=0
    )
    components.append(group2_component)

    nested_entry1 = view_pb2.TextEntry(
        text="Nested entry 1", 
        type=view_pb2.PLAIN,
        group_id=3
    )
    nested_entry2 = view_pb2.TextEntry(
        text="Nested entry 2", 
        type=view_pb2.PLAIN,
        group_id=3
    )
    nested_component1 = view_pb2.Component(
        text_component=view_pb2.TextComponent(entries=[nested_entry1]),
        id=0
    )
    nested_component2 = view_pb2.Component(
        text_component=view_pb2.TextComponent(entries=[nested_entry2]),
        id=0
    )
    nested_list = view_pb2.ListComponent(components=[nested_component1, nested_component2])
    nested_list_component = view_pb2.Component(
        list_component=nested_list,
        id=0
    )
    components.append(nested_list_component)

    list_component = view_pb2.ListComponent(components=components)
    root_component = view_pb2.Component(list_component=list_component, id=0)
    return root_component

class ViewServiceServicer(view_pb2_grpc.ViewServiceServicer):
    def __init__(self):
        global current_view, expanded
        expanded = False
        with view_lock:
            current_view = update_view()

    def GetView(self, request, context):
        global current_view
        with view_lock:
            return view_pb2.ViewResponse(component=current_view)

    def Click(self, request, context):
        global current_view, expanded
        if request.object_id == 100:
            with view_lock:
                expanded = not expanded
                current_view = update_view()
            print(f"Toggle: expanded={expanded}")
        else:
            print(f"Clicked object with id: {request.object_id}")
        return view_pb2.ClickResponse(status="OK")

def serve_grpc(port=50051):
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    view_pb2_grpc.add_ViewServiceServicer_to_server(ViewServiceServicer(), server)
    server.add_insecure_port(f'[::]:{port}')
    server.start()
    print(f"View Manager (gRPC) is running on port {port}")
    server.wait_for_termination()

if __name__ == "__main__":
    serve_grpc(port=50051)
