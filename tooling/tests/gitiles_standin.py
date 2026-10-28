#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""A stand-in for Gitiles and chromiumdash on 127.0.0.1, for the tests of
the CI checks (tooling/tests/test_ci.sh). Files and directory listings come
from the tags of a local git repository, as Gitiles serves them; releases
from STATE/releases.json ({"Stable": "1.0.0.0", ...}). STATE/faults.json
({"/request/path": [503, ...]}) gives codes a path answers first, one per
request. Every request path is appended to STATE/requests.log. The state
files are read per request. Prints the port, then serves until killed.

Usage: gitiles_standin.py REPO STATE
"""

import base64
import http.server
import json
import os
import subprocess
import sys
import urllib.parse

REPO, STATE = sys.argv[1], sys.argv[2]
TAGS = '/+/refs/tags/'


def git(*args):
    r = subprocess.run(['git', '-C', REPO, *args], capture_output=True)
    return r.stdout if r.returncode == 0 else None


def state(name, default):
    try:
        with open(os.path.join(STATE, name)) as f:
            return json.load(f)
    except FileNotFoundError:
        return default


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query)
        with open(os.path.join(STATE, 'requests.log'), 'a') as f:
            f.write(url.path + '\n')
        faults = state('faults.json', {})
        if faults.get(url.path):
            code = faults[url.path].pop(0)
            with open(os.path.join(STATE, 'faults.json'), 'w') as f:
                json.dump(faults, f)
            return self.answer(code, b'')
        if url.path == '/fetch_releases':
            version = state('releases.json', {}).get(query.get('channel', [''])[0])
            return self.answer(200, json.dumps([{'version': version}] if version else []).encode())
        if not url.path.startswith(TAGS):
            return self.answer(404, b'')
        tag, _, path = url.path[len(TAGS):].partition('/')
        if query.get('format') == ['JSON'] and path.endswith('/'):
            names = git('ls-tree', '--name-only', f'{tag}:{path.rstrip("/")}')
            if names is None:
                return self.answer(404, b'')
            entries = [{'name': n} for n in names.decode().split('\n') if n]
            return self.answer(200, b")]}'\n" + json.dumps({'entries': entries}).encode())
        if query.get('format') == ['TEXT'] and git('cat-file', '-t', f'{tag}:{path}') == b'blob\n':
            return self.answer(200, base64.b64encode(git('show', f'{tag}:{path}')))
        return self.answer(404, b'')

    def answer(self, code, body):
        self.send_response(code)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
print(server.server_address[1], flush=True)
server.serve_forever()
