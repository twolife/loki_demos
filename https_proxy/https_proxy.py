#!/usr/bin/env python3

"""
The loki_update binary from 2001 doesn't support HTTPS, only plain HTTP
and doesn't follow HTTP code 301 "Moved Permanently".

So I needed a tool to proxy the HTTP requests to the real HTTPS server.
Here is a simple one, than can be used with the loki_update_https.so shim.

$ ./https_proxy.py &
$ LD_PRELOAD=$(pwd)/loki_update_https.so loki_update
"""

import requests
import signal
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

class LokiUpdatesHTTPSProxy:
    def start_server(self):
        class ProxyHTTPRequestHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                hostname = self.headers.get('Host')
                url = 'https://' + hostname + self.path
                print(url)
                if (not hostname):
                    print(f"Unable to resolve the URL {url}")
                    self.send_response(404)
                    self.send_header("Content-Length", 0)
                    self.end_headers()
                    return

                resp = requests.get(url, stream=True)

                self.send_response(resp.status_code)
                self.send_header("Content-Length", resp.headers["Content-Length"])
                self.end_headers()
                for chunk in resp.iter_content(chunk_size=1024):
                    self.wfile.write(chunk)

        server_address = ('127.0.0.1', 8888)
        self.httpd = HTTPServer(server_address, ProxyHTTPRequestHandler)
        print('LokiUpdate HTTPS Proxy server is running on', server_address)
        self.httpd.serve_forever()

def exit_now(signum, frame):
    sys.exit(0)

if __name__ == '__main__':
    proxy = LokiUpdatesHTTPSProxy()
    signal.signal(signal.SIGTERM, exit_now)
    proxy.start_server()
