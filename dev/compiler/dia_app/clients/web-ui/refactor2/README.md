# Web UI for Compiler View

This is a Flask-based web interface for viewing compiler diagnostics and interacting with the compiler service.

## Setup

1. Create a virtual environment (recommended):
```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

2. Install dependencies:
```bash
pip install -r requirements.txt
```

3. Generate gRPC code from proto file:
```bash
python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. view.proto
```

4. Run the web server:
```bash
python web_server.py
```

The web interface will be available at http://localhost:5000

## Features

- Two-panel interface: main panel for diagnostics and side panel for additional information
- Interactive components with click handling
- Side information management with close functionality
- Edge navigation between related information
- Automatic view updates after any interaction

## Note

Make sure the gRPC server is running on port 50051 before starting the web interface. 