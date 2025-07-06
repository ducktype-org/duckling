from flask import Flask, render_template, jsonify, request
import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc
import json
import traceback
from jinja2 import StrictUndefined
from google.protobuf.json_format import MessageToDict
from collections import defaultdict
from dataclasses import dataclass
from typing import List, Dict, Set

@dataclass
class MessageInfo:
    tag: int
    priority: int
    type: int
    content: str
    last_line: int

class EnhancedCodeSection:
    def __init__(self, code_section: view_pb2.CodeSection):
        self.metadata = code_section.metadata
        self.lines = code_section.lines
        self.hl_messages = code_section.hl_messages
        
        self.line_tags: Dict[int, Set[int]] = defaultdict(set)
        for i, line in enumerate(self.lines):
            self._collect_tags_from_component(line.content, i)
            
        self.messages: Dict[int, MessageInfo] = {}
        for msg in self.hl_messages:
            last_line = max((i for i, tags in self.line_tags.items() if msg.tag in tags), default=-1)
            if last_line >= 0:
                self.messages[msg.tag] = MessageInfo(
                    tag=msg.tag,
                    priority=msg.priority,
                    type=msg.type,
                    content=self._extract_message_content(msg.message),
                    last_line=last_line
                )
    
    def _collect_tags_from_component(self, component: view_pb2.HlComponent, line_idx: int) -> None:
        comp_type = component.WhichOneof('component')
        if comp_type == 'code_component':
            self.line_tags[line_idx].update(component.code_component.hl_tags)
        elif comp_type == 'concat_component':
            for child in component.concat_component.components:
                self._collect_tags_from_component(child, line_idx)
        elif comp_type == 'interactive_component':
            self._collect_tags_from_component(component.interactive_component.primary_component, line_idx)
    
    def _extract_message_content(self, component: view_pb2.NoHlComponent) -> str:
        comp_type = component.WhichOneof('component')
        if comp_type == 'text_component':
            return component.text_component.content
        elif comp_type == 'code_component':
            return component.code_component.content
        elif comp_type == 'concat_component':
            return ''.join(self._extract_message_content(child) for child in component.concat_component.components)
        elif comp_type == 'interactive_component':
            return self._extract_message_content(component.interactive_component.primary_component)
        return ''

    def get_messages_for_line(self, line_idx: int) -> List[MessageInfo]:
        """Get all messages that should be displayed after this line, sorted by priority"""
        messages = [msg for msg in self.messages.values() if msg.last_line == line_idx]
        return sorted(messages, key=lambda m: m.priority)

app = Flask(__name__)
app.jinja_env.undefined = StrictUndefined

channel = grpc.insecure_channel('localhost:50051')
stub = view_pb2_grpc.ViewServiceStub(channel)

@app.route('/')
def index():
    try:
        response = stub.GetView(view_pb2.ViewRequest())
        
        diagnostics = []
        for diag in response.diagnostics:
            processed_diag = {'infos': []}
            for info in diag.infos:
                processed_info = {
                    'metadata': info.metadata,
                    'header': info.header,
                    'sections': []
                }
                for section in info.sections:
                    section_type = section.WhichOneof('section')
                    if section_type == 'code_section':
                        enhanced_section = EnhancedCodeSection(section.code_section)
                        processed_info['sections'].append({
                            'type': 'code_section',
                            'section': enhanced_section
                        })
                    else:  
                        processed_info['sections'].append({
                            'type': 'text_section',
                            'section': section.text_section
                        })
                processed_diag['infos'].append(processed_info)
            diagnostics.append(processed_diag)

        side_paths = []
        for path in response.side_paths:
            processed_path = {'infos': []}
            for info in path.infos:
                processed_info = {
                    'metadata': info.metadata,
                    'header': info.header,
                    'sections': [],
                    'edges': info.edges,
                    'side_info_id': info.side_info_id
                }
                for section in info.sections:
                    section_type = section.WhichOneof('section')
                    if section_type == 'code_section':
                        enhanced_section = EnhancedCodeSection(section.code_section)
                        processed_info['sections'].append({
                            'type': 'code_section',
                            'section': enhanced_section
                        })
                    else:  
                        processed_info['sections'].append({
                            'type': 'text_section',
                            'section': section.text_section
                        })
                processed_path['infos'].append(processed_info)
            side_paths.append(processed_path)

        return render_template('index.html', 
                            diagnostics=diagnostics,
                            side_paths=side_paths)
    except Exception as e:
        print(f"Error in main route:")
        traceback.print_exc()
        return str(e), 500

@app.route('/click', methods=['POST'])
def click():
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
        print(f"Error in click route:")
        traceback.print_exc()
        return str(e), 500

@app.route('/close_side_info', methods=['POST'])
def close_side_info():
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
        print(f"Error in close_side_info route:")
        traceback.print_exc()
        return str(e), 500

@app.route('/get_edge', methods=['POST'])
def get_edge():
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
        print(f"Error in get_edge route:")
        traceback.print_exc()
        return str(e), 500

if __name__ == '__main__':
    app.run(debug=True, port=5000) 