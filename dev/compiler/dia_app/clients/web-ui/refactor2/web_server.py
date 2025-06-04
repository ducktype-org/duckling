from flask import Flask, render_template, jsonify, request
import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc
import json
from google.protobuf.json_format import MessageToDict
import traceback
from jinja2 import StrictUndefined

app = Flask(__name__)
app.jinja_env.undefined = StrictUndefined

# Create gRPC channel and stub
channel = grpc.insecure_channel('localhost:50051')
stub = view_pb2_grpc.ViewServiceStub(channel)

@app.route('/')
def index():
    try:
        # Get initial view
        response = stub.GetView(view_pb2.ViewRequest())
        
        # Debug: Print the structure of components
        response_dict = MessageToDict(response)
        print("DEBUG: Response structure:")
        print(json.dumps(response_dict, indent=2))

        # Debug: Print first diagnostic header structure
        if response.diagnostics:
            first_diag = response.diagnostics[0]
            if first_diag.infos:
                first_info = first_diag.infos[0]
                print("\nDEBUG: First diagnostic header structure:")
                print(f"Header type: {type(first_info.header)}")
                print(f"Header dir: {dir(first_info.header)}")
                if hasattr(first_info.header, 'ListFields'):
                    print("Header fields:")
                    for field, value in first_info.header.ListFields():
                        print(f"  {field.name}: {type(value)}")
                        if field.name == 'concat_component':
                            print("    Concat component fields:")
                            for comp in value.components:
                                print(f"      Component type: {type(comp)}")
                                print(f"      Component fields:")
                                for f, v in comp.ListFields():
                                    print(f"        {f.name}: {type(v)}")
                                    if hasattr(v, 'content'):
                                        print(f"          Content: {v.content}")
        
        return render_template('index.html', 
                             diagnostics=response.diagnostics,
                             side_paths=response.side_paths)
    except Exception as e:
        print(f"Error in index route:")
        traceback.print_exc()
        return str(e), 500

@app.route('/click', methods=['POST'])
def click():
    try:
        data = request.json
        component_id = int(data.get('component_id', 0))
        click_type = data.get('click_type', 'CLICK')
        
        # Convert string click type to enum
        click_type_enum = view_pb2.ClickType.Value(click_type)
        
        # Make click request
        click_request = view_pb2.ClickRequest(
            component_id=component_id,
            click_type=click_type_enum
        )
        click_response = stub.Click(click_request)
        
        # Get updated view
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
        
        # Make close side info request
        close_request = view_pb2.CloseSideInfoRequest(side_info_id=side_info_id)
        close_response = stub.CloseSideInfo(close_request)
        
        # Get updated view
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
        
        # Make edge request
        edge_request = view_pb2.EdgeRequest(
            side_info_id=side_info_id,
            edge_id=edge_id
        )
        edge_response = stub.GetEdge(edge_request)
        
        # Get updated view
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