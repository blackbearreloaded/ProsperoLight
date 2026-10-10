# ps5-native-app-boilerplate - LAN pacing report receiver.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import uuid

MAX_BYTES = 2 * 1024 * 1024


def handler(directory, reports):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(directory), **kwargs)

        def do_POST(self):
            if self.path != '/api/reports':
                self.send_error(404)
                return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= MAX_BYTES:
                    self.send_error(413)
                    return
                data = json.loads(self.rfile.read(size))
                if not isinstance(data, dict) or data.get('schema') != 1 or not isinstance(data.get('settings'), dict):
                    raise ValueError('Invalid report')
                report_id = uuid.uuid4().hex
                reports.mkdir(parents=True, exist_ok=True)
                with (reports / (report_id + '.json')).open('x') as output:
                    json.dump(data, output, ensure_ascii=False)
                body = json.dumps({'id': report_id}).encode()
                self.send_response(201)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except (ValueError, UnicodeError):
                self.send_error(400)

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=9075)
    parser.add_argument('--bind', default='0.0.0.0')
    parser.add_argument('--reports', type=Path, required=True)
    args = parser.parse_args()
    ThreadingHTTPServer((args.bind, args.port), handler(Path(__file__).parent, args.reports)).serve_forever()


if __name__ == '__main__':
    main()
