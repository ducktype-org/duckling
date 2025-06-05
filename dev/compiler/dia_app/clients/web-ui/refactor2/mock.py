import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc
import time

class ViewServiceServicer(view_pb2_grpc.ViewServiceServicer):
    def __init__(self):
        self.side_info_counter = 0
        self.side_infos = {}  # id -> info
        self.clicked_components = set()

    def GetView(self, request, context):
        # Create a sample diagnostic with interactive code
        diagnostic = view_pb2.Diagnostic()
        info = diagnostic.infos.add()
        info.metadata.type = view_pb2.InfoType.Note
        info.metadata.code = 1234

        # Add header
        info.header.text_component.content = "Sample interactive code"

        # Add code section
        section = info.sections.add()
        code_section = section.code_section
        code_section.metadata.filename = "example.txt"
        code_section.metadata.line = 1
        code_section.metadata.column = 1

        # Add some lines with interactive components
        for i in range(1, 4):
            line = code_section.lines.add()
            line.line_number = i
            
            # Create a concat component for the line
            concat = line.content.concat_component
            
            # Add some regular code
            code = concat.components.add()
            code.code_component.content = f"let x{i} = "
            
            # Add interactive component
            interactive = concat.components.add()
            interactive.interactive_component.component_id = i
            interactive.interactive_component.status = view_pb2.VisibilityStatus.Primary
            interactive.interactive_component.primary_component.code_component.content = f"value{i}"
            if i in self.clicked_components:
                interactive.interactive_component.primary_component.code_component.hl_tags.append(i)
            
            # Add semicolon
            end = concat.components.add()
            end.code_component.content = ";"

            # If component was clicked, add a message
            if i in self.clicked_components:
                msg = code_section.hl_messages.add()
                msg.tag = i
                msg.priority = 1
                msg.type = view_pb2.InfoType.Note
                msg.message.text_component.content = f"This value was clicked!"

        # Add side infos if any exist
        response = view_pb2.ViewResponse()
        response.diagnostics.append(diagnostic)

        if self.side_infos:
            side_path = response.side_paths.add()
            for info in self.side_infos.values():
                side_path.infos.append(info)

        return response

    def Click(self, request, context):
        comp_id = request.component_id
        self.clicked_components.add(comp_id)

        # Create a new side info
        self.side_info_counter += 1
        side_info = view_pb2.SideInfo()
        side_info.side_info_id = self.side_info_counter
        side_info.metadata.type = view_pb2.InfoType.Note
        side_info.metadata.code = 5678
        side_info.header.text_component.content = f"Clicked component {comp_id}"
        
        # Add a section with some details
        section = side_info.sections.add()
        section.text_section.text_component.content = f"This is value{comp_id}. Click edges to learn more!"

        # Add edges that will show more information
        edge1 = side_info.edges.add()
        edge1.edge_id = self.side_info_counter * 10 + 1
        edge1.description = "Show type information"

        edge2 = side_info.edges.add()
        edge2.edge_id = self.side_info_counter * 10 + 2
        edge2.description = "Show usage examples"

        self.side_infos[self.side_info_counter] = side_info
        return view_pb2.ClickResponse(status="ok")

    def CloseSideInfo(self, request, context):
        if request.side_info_id in self.side_infos:
            del self.side_infos[request.side_info_id]
        return view_pb2.CloseSideInfoResponse(status="ok")

    def GetEdge(self, request, context):
        edge_id = request.edge_id
        base_id = edge_id // 10
        edge_type = edge_id % 10

        # Create a new side info based on the edge type
        self.side_info_counter += 1
        side_info = view_pb2.SideInfo()
        side_info.side_info_id = self.side_info_counter
        side_info.metadata.type = view_pb2.InfoType.Note
        
        if edge_type == 1:  # Type information
            side_info.metadata.code = 1001
            side_info.header.text_component.content = "Type Information"
            
            section = side_info.sections.add()
            section.code_section.metadata.filename = "types.txt"
            section.code_section.metadata.line = 1
            
            line = section.code_section.lines.add()
            line.content.code_component.content = f"type value{base_id} = i32;"
            
            # Add edge to show more type details
            edge = side_info.edges.add()
            edge.edge_id = self.side_info_counter * 10 + 3
            edge.description = "Show type constraints"
            
        elif edge_type == 2:  # Usage examples
            side_info.metadata.code = 1002
            side_info.header.text_component.content = "Usage Examples"
            
            section = side_info.sections.add()
            section.code_section.metadata.filename = "examples.txt"
            section.code_section.metadata.line = 1
            
            for i in range(2):
                line = section.code_section.lines.add()
                line.line_number = i + 1
                line.content.code_component.content = f"let example{i} = value{base_id} + {i};"
            
            # Add edge to show more examples
            edge = side_info.edges.add()
            edge.edge_id = self.side_info_counter * 10 + 4
            edge.description = "Show more examples"
            
        elif edge_type == 3:  # Type constraints
            side_info.metadata.code = 1003
            side_info.header.text_component.content = "Type Constraints"
            
            section = side_info.sections.add()
            section.text_section.text_component.content = f"value{base_id} must be a valid 32-bit signed integer"
            
        elif edge_type == 4:  # More examples
            side_info.metadata.code = 1004
            side_info.header.text_component.content = "More Examples"
            
            section = side_info.sections.add()
            section.code_section.metadata.filename = "more_examples.txt"
            section.code_section.metadata.line = 1
            
            for i in range(2):
                line = section.code_section.lines.add()
                line.line_number = i + 1
                line.content.code_component.content = f"let complex_example{i} = value{base_id} * {i+2} + value{base_id};"

        self.side_infos[self.side_info_counter] = side_info
        return view_pb2.EdgeResponse(status="ok")

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    view_pb2_grpc.add_ViewServiceServicer_to_server(ViewServiceServicer(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("Mock server started on port 50051")
    try:
        while True:
            time.sleep(86400)  # One day in seconds
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve() 