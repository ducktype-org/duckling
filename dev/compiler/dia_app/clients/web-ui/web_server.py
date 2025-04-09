import grpc
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from google.protobuf.empty_pb2 import Empty
import view_pb2
import view_pb2_grpc

channel = grpc.insecure_channel('localhost:50051')
view_stub = view_pb2_grpc.ViewServiceStub(channel)

def generate_html(component):
    html = """
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8">
      <title>View from Web Server</title>
      <style>
         body { font-family: Arial, sans-serif; margin: 20px; background: #f7f7f7; }
         .component { margin-bottom: 20px; padding: 10px; background: #fff; border: 1px solid #ddd; border-radius: 4px; }
         .clickable { cursor: pointer; }
         .text-entry { margin-bottom: 10px; transition: background-color 0.2s ease; }
         .code-block {
            background-color: #f0f0f0;
            color: #333;
            padding: 2px 4px;
            border-radius: 3px;
         }
      </style>
      <script>
      document.addEventListener("DOMContentLoaded", function() {
          let elements = document.querySelectorAll("[data-group]");
          elements.forEach(el => {
              el.addEventListener("mouseover", function() {
                  let group = this.getAttribute("data-group");
                  document.querySelectorAll("[data-group='" + group + "']").forEach(e => {
                      e.style.backgroundColor = "#d3eaff";
                  });
              });
              el.addEventListener("mouseout", function() {
                  let group = this.getAttribute("data-group");
                  document.querySelectorAll("[data-group='" + group + "']").forEach(e => {
                      if(e.tagName === "CODE") {
                          e.style.backgroundColor = "#f0f0f0";
                      } else {
                          e.style.backgroundColor = "";
                      }
                  });
              });
          });
      });
      function handleClick(id) {
         fetch('/click?object_id=' + id)
            .then(response => response.text())
            .then(text => {
               console.log("Clicked:", text);
               window.location.reload();
            });
      }
      </script>
    </head>
    <body>
    """
    html += render_component(component)
    html += "</body></html>"
    return html

def render_component(component):
    clickable_attr = ""
    if component.id:
        clickable_attr = f' onclick="handleClick({component.id})" class="clickable"'
    if component.HasField("text_component"):
        content = render_text_component(component.text_component)
    elif component.HasField("list_component"):
        content = render_list_component(component.list_component)
    else:
        content = "Unknown component"
    return f"<div class='component'{clickable_attr}>{content}</div>"

def render_text_component(text_component):
    parts = []
    for entry in text_component.entries:
        group_attr = f" data-group='{entry.group_id}'" if entry.group_id != 0 else ""
        if entry.type == view_pb2.PLAIN:
            parts.append(f"<span{group_attr}>{entry.text}</span>")
        elif entry.type == view_pb2.CODE:
            parts.append(f"<code{group_attr} class='code-block'>{entry.text}</code>")
    return f"<p class='text-entry'>{' '.join(parts)}</p>"

def render_list_component(list_component):
    html = ""
    for comp in list_component.components:
        html += render_component(comp)
    return html

class WebRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed_url = urlparse(self.path)
        if parsed_url.path == "/click":
            params = parse_qs(parsed_url.query)
            if "object_id" in params:
                try:
                    object_id = int(params["object_id"][0])
                except ValueError:
                    self.send_error(400, "Invalid id")
                    return
                click_request = view_pb2.ClickRequest(object_id=object_id)
                response = view_stub.Click(click_request)
                self.send_response(200)
                self.send_header("Content-type", "text/plain")
                self.end_headers()
                self.wfile.write(response.status.encode())
            else:
                self.send_error(400, "Missing parameter: object_id")
        else:
            view_response = view_stub.GetView(Empty())
            html = generate_html(view_response.component)
            self.send_response(200)
            self.send_header("Content-type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))

def run_server(port=8080):
    server_address = ('', port)
    httpd = HTTPServer(server_address, WebRequestHandler)
    print(f"Web Server is running on port {port}")
    httpd.serve_forever()

if __name__ == "__main__":
    run_server(port=8080)
