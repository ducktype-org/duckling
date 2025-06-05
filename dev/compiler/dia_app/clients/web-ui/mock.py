import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc
import time

class ViewServiceServicer(view_pb2_grpc.ViewServiceServicer):
    def __init__(self):
        self.side_info_counter = 0
        self.side_infos = {}
        self.clicked_components = set()

    def GetView(self, request, context):
        diagnostic = view_pb2.Diagnostic()
        info = diagnostic.infos.add()
        info.metadata.type = view_pb2.InfoType.Error
        info.metadata.code = 1234

        header_component = info.header.interactive_component
        header_component.component_id = 1
        header_component.status = view_pb2.VisibilityStatus.Primary
        header_component.primary_component.text_component.content = "Cannot move out of borrowed content"

        section = info.sections.add()
        code_section = section.code_section
        code_section.metadata.filename = "src/main.rs"
        code_section.metadata.line = 42
        code_section.metadata.column = 1

        lines = [
            (1, "fn process_data(data: &Vec<String>) {"),
            (2, "    let mut results = Vec::new();"),
            (3, "    for item in data {"),
            (4, "        let len = item.len();"),
            (5, "        if len > 5 {"),
            (6, "            results.push(process_item(item));"),
            (7, "        }"),
            (8, "        println!(\"{} has length {}\", item, len);"),
            (9, "    }"),
            (10, "}"),
            (11, ""),
            (12, "fn process_item(s: String) -> String {"),
            (13, "    s.to_uppercase()"),
            (14, "}")
        ]

        for line_num, content in lines:
            line = code_section.lines.add()
            line.line_number = line_num
            
            if line_num in [3, 4, 6, 8]:
                concat = line.content.concat_component
                
                if line_num == 6:
                    parts = ["            results.push(process_", "item", "(", "item", ");"]
                    for i, part in enumerate(parts):
                        if part == "item" and i == 3:  
                            code = concat.components.add()
                            code.code_component.content = "item"
                            code.code_component.hl_tags.append(1)
                        else:
                            text = concat.components.add()
                            text.code_component.content = part

                    msg = code_section.hl_messages.add()
                    msg.tag = 1
                    msg.priority = 1
                    msg.type = view_pb2.InfoType.Error
                    msg.message.text_component.content = "Cannot move out of borrowed content - 'item' is borrowed here but function expects owned String"
                else:
                    parts = content.split('item')
                    for i, part in enumerate(parts):
                        if part:
                            text = concat.components.add()
                            text.code_component.content = part
                        
                        if i < len(parts) - 1:
                            code = concat.components.add()
                            code.code_component.content = "item"
                            code.code_component.hl_tags.append(1)
            else:
                line.content.code_component.content = content

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

        self.side_info_counter += 1
        side_info = view_pb2.SideInfo()
        side_info.side_info_id = self.side_info_counter
        side_info.metadata.type = view_pb2.InfoType.Note
        side_info.metadata.code = 5678
        side_info.header.text_component.content = "Ownership Error Details"
        
        section = side_info.sections.add()
        section.text_section.text_component.content = "The error occurs because process_item expects to take ownership of the String, but 'item' is only borrowed in the for loop."

        edge1 = side_info.edges.add()
        edge1.edge_id = self.side_info_counter * 10 + 1
        edge1.description = "Show how to fix"

        edge2 = side_info.edges.add()
        edge2.edge_id = self.side_info_counter * 10 + 2
        edge2.description = "Learn about ownership"

        edge3 = side_info.edges.add()
        edge3.edge_id = self.side_info_counter * 10 + 3
        edge3.description = "See similar examples"

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

        self.side_info_counter += 1
        side_info = view_pb2.SideInfo()
        side_info.side_info_id = self.side_info_counter
        side_info.metadata.type = view_pb2.InfoType.Note
        
        if edge_type == 1: 
            side_info.metadata.code = 1001
            side_info.header.text_component.content = "How to Fix the Error"
            
            section = side_info.sections.add()
            section.code_section.metadata.filename = "src/main.rs"
            section.code_section.metadata.line = 1
            
            lines = [
                "// Option 1: Clone the borrowed string",
                "results.push(process_item(item.clone()));",
                "",
                "// Option 2: Change process_item to take a reference",
                "fn process_item(s: &String) -> String {",
                "    s.to_uppercase()",
                "}",
                "",
                "// Option 3: Change the loop to take ownership",
                "for item in data.iter() {",
                "    if item.len() > 5 {",
                "        results.push(process_item(item));",
                "    }",
                "}"
            ]
            
            for i, content in enumerate(lines):
                line = section.code_section.lines.add()
                line.line_number = i + 1
                line.content.code_component.content = content
            
            edge = side_info.edges.add()
            edge.edge_id = self.side_info_counter * 10 + 4
            edge.description = "Compare solutions"
            
        elif edge_type == 2: 
            side_info.metadata.code = 1002
            side_info.header.text_component.content = "Understanding Rust Ownership"
            
            text_section = side_info.sections.add()
            text_section.text_component.content = "In Rust, each value has a single owner. When you pass a value to a function, ownership is transferred unless:"
            
            code_section = side_info.sections.add()
            code_section.metadata.filename = "ownership_examples.rs"
            code_section.metadata.line = 1
            
            examples = [
                "// 1. The type implements Copy",
                "let x = 5;  // i32 implements Copy",
                "let y = x;  // x is still valid because i32 is Copy",
                "",
                "// 2. You pass a reference",
                "let s = String::from(\"hello\");",
                "let len = calculate_length(&s);  // s is borrowed, not moved",
                "",
                "// 3. You explicitly clone",
                "let s1 = String::from(\"hello\");",
                "let s2 = s1.clone();  // s1 is still valid because we cloned"
            ]
            
            for i, content in enumerate(examples):
                line = code_section.lines.add()
                line.line_number = i + 1
                line.content.code_component.content = content
            
        elif edge_type == 3: 
            side_info.metadata.code = 1003
            side_info.header.text_component.content = "Similar Ownership Errors"
            
            section = side_info.sections.add()
            section.code_section.metadata.filename = "common_errors.rs"
            section.code_section.metadata.line = 1
            
            examples = [
                "// Error 1: Moving out of a vector",
                "let v = vec![String::from(\"hello\")];",
                "let s = v[0];  // Error: cannot move out of index of Vec",
                "",
                "// Error 2: Moving in a loop",
                "let v = vec![String::from(\"hello\")];",
                "for s in &v {",
                "    take_ownership(*s);  // Error: cannot move out of borrow",
                "}",
                "",
                "// Error 3: Moving captured variable",
                "let s = String::from(\"hello\");",
                "thread::spawn(|| {",
                "    println!(\"{}\", s);  // Error: may outlive borrowed value",
                "});"
            ]
            
            for i, content in enumerate(examples):
                line = section.code_section.lines.add()
                line.line_number = i + 1
                line.content.code_component.content = content
            
        elif edge_type == 4:  
            side_info.metadata.code = 1004
            side_info.header.text_component.content = "Solution Performance Comparison"
            
            section = side_info.sections.add()
            section.text_section.text_component.content = """Performance implications of different solutions:

1. Using clone():
   + Simple to implement
   - Creates a new allocation
   - O(n) time complexity where n is string length
   
2. Using references:
   + No allocation needed
   + O(1) overhead
   - May need lifetime annotations
   - Might need to change function signatures
   
3. Taking ownership in loop:
   + No additional allocations
   + Clear ownership semantics
   - May need to restructure code
   - Might not always be possible"""

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
            time.sleep(86400)
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve() 