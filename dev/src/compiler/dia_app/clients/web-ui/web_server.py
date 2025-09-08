"""
Flask web application for interacting with compilation errors.
"""

import traceback
from collections import defaultdict
from concurrent import futures
from dataclasses import dataclass
from typing import Dict, List, Set

import grpc
from flask import Flask, jsonify, render_template, request
from google.protobuf.json_format import MessageToDict

import view_pb2
import view_pb2_grpc


@dataclass
class MessageInfo:
    """Container for highlight message information."""
    tag: int
    priority: int
    type: int
    content: str
    last_line: int


class EnhancedCodeSection:
    """
    Enhanced wrapper for gRPC CodeSection, helps with rendering
    the messages under the code blocks without implementing
    complex logic in Jinja. 
    """
    
    def __init__(self, code_section: view_pb2.CodeSection):
        """
        Initialize enhanced code section.
        
        @param code_section Protocol buffer code section to process
        """
        self.metadata = code_section.metadata
        self.lines = code_section.lines
        self.hl_messages = code_section.hl_messages
        
        # Map line indices to sets of highlight tags
        self.line_tags: Dict[int, Set[int]] = defaultdict(set)
        for i, line in enumerate(self.lines):
            self._collect_tags_from_component(line.content, i)
            
        # Map tags to message information
        self.messages: Dict[int, MessageInfo] = {}
        for msg in self.hl_messages:
            last_line = max(
                (i for i, tags in self.line_tags.items() if msg.tag in tags), 
                default=-1
            )
            if last_line >= 0:
                self.messages[msg.tag] = MessageInfo(
                    tag=msg.tag,
                    priority=msg.priority,
                    type=msg.type,
                    content=self._extract_message_content(msg.message),
                    last_line=last_line
                )
    
    def _collect_tags_from_component(self, component: view_pb2.HlComponent, line_idx: int) -> None:
        """
        Recursively collect highlight tags from component hierarchy.
        
        @param component Component to process
        @param line_idx Line index for tag mapping
        """
        comp_type = component.WhichOneof('component')
        if comp_type == 'code_component':
            self.line_tags[line_idx].update(component.code_component.hl_tags)
        elif comp_type == 'concat_component':
            for child in component.concat_component.components:
                self._collect_tags_from_component(child, line_idx)
        elif comp_type == 'interactive_component':
            self._collect_tags_from_component(
                component.interactive_component.primary_component, line_idx
            )
    
    def _extract_message_content(self, component: view_pb2.NoHlComponent) -> str:
        """
        Extract text content from message component.
        
        @param component Message component to extract from
        @return Extracted text content
        """
        comp_type = component.WhichOneof('component')
        if comp_type == 'text_component':
            return component.text_component.content
        elif comp_type == 'code_component':
            return component.code_component.content
        elif comp_type == 'concat_component':
            return ''.join(
                self._extract_message_content(child) 
                for child in component.concat_component.components
            )
        elif comp_type == 'interactive_component':
            return self._extract_message_content(
                component.interactive_component.primary_component
            )
        return ''

    def get_messages_for_line(self, line_idx: int) -> List[MessageInfo]:
        """
        Get messages to display after specified line.
        
        @param line_idx Line index to get messages for
        @return List of messages sorted by priority
        """
        messages = [
            msg for msg in self.messages.values() 
            if msg.last_line == line_idx
        ]
        return sorted(messages, key=lambda m: m.priority)


def create_app() -> Flask:
    """
    Create and configure Flask application.
    
    @return Configured Flask application instance
    """
    app = Flask(__name__)
    return app


def get_grpc_stub() -> view_pb2_grpc.ViewServiceStub:
    """
    Create gRPC stub for service communication.
    
    @return Configured gRPC service stub
    """
    channel = grpc.insecure_channel('localhost:50051')
    return view_pb2_grpc.ViewServiceStub(channel)


def process_section(section) -> dict:
    """
    Transform section by wrapping code sections with EnhancedCodeSection.
    
    @param section Raw protobuf section from gRPC response
    @return Dictionary with enhanced section ready for template rendering
    """
    section_type = section.WhichOneof('section')
    if section_type == 'code_section':
        enhanced_section = EnhancedCodeSection(section.code_section)
        return {
            'type': 'code_section',
            'section': enhanced_section
        }
    else:
        return {
            'type': 'text_section',
            'section': section.text_section
        }


def process_info_sections(info) -> dict:
    """
    Transform info object by enhancing all contained code sections.
    
    Recursively processes all sections within an info object, replacing
    raw code sections with EnhancedCodeSection.
    
    @param info Info object containing metadata, header and sections
    @return Dictionary with processed sections and preserved metadata
    """
    processed_info = {
        'metadata': info.metadata,
        'header': info.header,
        'sections': []
    }
    
    # Add side info specific fields if present
    if hasattr(info, 'edges'):
        processed_info['edges'] = info.edges
    if hasattr(info, 'side_info_id'):
        processed_info['side_info_id'] = info.side_info_id
    
    for section in info.sections:
        processed_info['sections'].append(process_section(section))
    
    return processed_info


def process_diagnostic_or_path(item) -> dict:
    """
    Transform entire diagnostic or side path by enhancing all code sections.
    
    Processes the complete gRPC response tree for easier integration with
    Jinja2.
    
    @param item Diagnostic or side path item from gRPC response
    @return Processed tree with enhanced code sections integrated
    """
    processed_item = {'infos': []}
    for info in item.infos:
        processed_item['infos'].append(process_info_sections(info))
    return processed_item


# Initialize Flask app and gRPC stub
app = create_app()
stub = get_grpc_stub()


@app.route('/')
def index():
    """
    Main route displaying diagnostics and side paths.
    
    @return Rendered HTML template with processed diagnostic data
    """
    try:
        response = stub.GetView(view_pb2.ViewRequest())
        
        diagnostics = [
            process_diagnostic_or_path(diag) 
            for diag in response.diagnostics
        ]
        
        side_paths = [
            process_diagnostic_or_path(path) 
            for path in response.side_paths
        ]

        return render_template('index.html', 
                             diagnostics=diagnostics,
                             side_paths=side_paths)
    except Exception as e:
        print("Error in main route:")
        traceback.print_exc()
        return str(e), 500


@app.route('/click', methods=['POST'])
def click():
    """
    Handle component click interactions.
    
    @return JSON response with updated view state
    """
    try:
        data = request.json
        component_id = int(data.get('component_id', 0))
        click_type = data.get('click_type', 'CLICK')
        
        click_type_enum = view_pb2.ClickType.Value(click_type)
        
        click_request = view_pb2.ClickRequest(
            component_id=component_id,
            click_type=click_type_enum
        )
        click_response = stub.Click(click_request)
        
        view_response = stub.GetView(view_pb2.ViewRequest())
        
        return jsonify({
            'status': click_response.status,
            'diagnostics': [MessageToDict(d) for d in view_response.diagnostics],
            'side_paths': [MessageToDict(p) for p in view_response.side_paths]
        })
    except Exception as e:
        print("Error in click route:")
        traceback.print_exc()
        return str(e), 500


@app.route('/close_side_info', methods=['POST'])
def close_side_info():
    """
    Handle close side info requests.

    @return JSON response with updated view state
    """
    try:
        data = request.json
        side_info_id = int(data.get('side_info_id', 0))
        
        close_request = view_pb2.CloseSideInfoRequest(side_info_id=side_info_id)
        close_response = stub.CloseSideInfo(close_request)
        
        view_response = stub.GetView(view_pb2.ViewRequest())
        
        return jsonify({
            'status': close_response.status,
            'diagnostics': [MessageToDict(d) for d in view_response.diagnostics],
            'side_paths': [MessageToDict(p) for p in view_response.side_paths]
        })
    except Exception as e:
        print("Error in close_side_info route:")
        traceback.print_exc()
        return str(e), 500


@app.route('/get_edge', methods=['POST'])
def get_edge():
    """
    Handle edge interactions.
    
    @return JSON response with edge data and updated view state
    """
    try:
        data = request.json
        side_info_id = int(data.get('side_info_id', 0))
        edge_id = int(data.get('edge_id', 0))
        
        edge_request = view_pb2.EdgeRequest(
            side_info_id=side_info_id,
            edge_id=edge_id
        )
        edge_response = stub.GetEdge(edge_request)
        
        view_response = stub.GetView(view_pb2.ViewRequest())
        
        return jsonify({
            'status': edge_response.status,
            'diagnostics': [MessageToDict(d) for d in view_response.diagnostics],
            'side_paths': [MessageToDict(p) for p in view_response.side_paths]
        })
    except Exception as e:
        print("Error in get_edge route:")
        traceback.print_exc()
        return str(e), 500


if __name__ == '__main__':
    app.run(debug=True, port=5000)