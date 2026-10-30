"""Find and talk to the right Ghidra when several run at once. Import it (the tools' GhidraMCP client)
or run it:

    python ghidra_mcp.py list [--json]   every running Ghidra instance; * marks the one the tools use
    python ghidra_mcp.py which           the instance the tools resolve to, or why none does
    python ghidra_mcp.py launch          start Ghidra on YamiPan's project unless it already runs

How it works (GhidraMCP 5.14.2, bethington/ghidra-mcp, checked in the plugin's bytecode 2026-10-30):
each Ghidra *process* runs one HTTP server, shared by all its tool windows, and serves any program
open in any of them (`program=` picks one). The server binds the configured port (Tool Options ->
GhidraMCP HTTP Server -> Server Port, 8089) or, when another Ghidra holds it, the next free one of the
16 from there (8090..8104), and answers GET /mcp/instance_info with its pid, project and the
project's files. A Ghidra project can be open in one process only, so the project names the
instance. The plugin's Unix-socket transport would make discovery exact, but Python on Windows has no
AF_UNIX: this module scans the TCP range instead.

Configuration: data/ghidra.json; an environment variable overrides each key:

    project    GHIDRA_MCP_PROJECT  the Ghidra project: its path without .gpr (C:\\Users\\willi\\YamiPan)
                                   or just its name; the instance is found by it
    program    GHIDRA_MCP_PROGRAM  the game's program as Ghidra names it (null until imported); used
                                   when no instance has the project, and in every request
    ports      GHIDRA_MCP_PORTS    "8089:16": first port and how many to scan (the plugin's range)
    ghidra_dir GHIDRA_DIR          the Ghidra install (ghidraRun.bat), for launch
    bridge, bridge_python          bridge_mcp_ghidra.py and the venv python that runs it (ghidra_bridge.py)
               GHIDRA_MCP_URL      pins one instance: no discovery, as the bridge and older tools read it

The resolved instance is cached in work/ghidra_instance.json (port + pid) and re-checked with one
request; a connection that fails re-resolves once, so a Ghidra restarted on another port is followed.
"""
import argparse
import concurrent.futures
import json
import os
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.parse
from dataclasses import dataclass, field
from http import client as http_client   # module-level http() below is the client call
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
CONFIG_JSON = TOOLS / 'data' / 'ghidra.json'
CACHE_JSON = TOOLS / 'work' / 'ghidra_instance.json'
HOST = '127.0.0.1'
# A closed port on Windows does not refuse at once (the SYN is retried for ~1 s): connect with a short
# timeout, then give a live but busy Ghidra the request timeout to answer.
CONNECT_TIMEOUT = 0.4


def _config():
    try:
        cfg = json.loads(CONFIG_JSON.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        cfg = {}
    for key, var in (('project', 'GHIDRA_MCP_PROJECT'), ('program', 'GHIDRA_MCP_PROGRAM'),
                     ('ports', 'GHIDRA_MCP_PORTS'), ('ghidra_dir', 'GHIDRA_DIR'),
                     ('bridge', 'GHIDRA_MCP_BRIDGE'), ('bridge_python', 'GHIDRA_MCP_BRIDGE_PYTHON')):
        if os.environ.get(var):
            cfg[key] = os.environ[var]
    return cfg


CONFIG = _config()
PROJECT = CONFIG.get('project') or None
PROGRAM = CONFIG.get('program') or None
PINNED = os.environ.get('GHIDRA_MCP_URL', '').rstrip('/') or None
_first, _, _count = str(CONFIG.get('ports') or '8089:16').partition(':')
PORTS = range(int(_first), int(_first) + int(_count or 16))


class GhidraError(ConnectionError):
    """No instance, or more than one, fits what was asked. An OSError, so callers that treat a
    refused connection as "Ghidra is down" treat this the same way."""


class _Connection(http_client.HTTPConnection):
    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), CONNECT_TIMEOUT)
        self.sock.settimeout(self.timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)


def _get(port, endpoint, timeout=5.0):
    """GET endpoint on 127.0.0.1:port -> parsed JSON (a Response.ok wrapper unwrapped), or None."""
    conn = _Connection(HOST, port, timeout=timeout)
    try:
        conn.request('GET', '/' + endpoint.lstrip('/'))
        resp = conn.getresponse()
        body = resp.read().decode('utf-8', 'replace')
        if resp.status != 200:
            return None
        data = json.loads(body)
    except (OSError, ValueError, http_client.HTTPException):
        return None
    finally:
        conn.close()
    if isinstance(data, dict) and set(data) <= {'data', 'success', 'status'} and 'data' in data:
        return data['data']
    return data


def _project_path(path):
    """A project path as given, without .gpr: Ghidra names the project after it, case included."""
    p = str(path).strip().rstrip('/\\')
    if p.lower().endswith('.gpr'):
        p = p[:-4]
    return os.path.normpath(p)


def _norm(path):
    """A project path for comparison (case-folded on Windows)."""
    return os.path.normcase(_project_path(path))


def _is_path(project):
    return any(c in project for c in '/\\:')


def _names(programs):
    out = set()
    for p in programs:
        for v in (p.get('name') or '', p.get('path') or ''):
            if v:
                out.update((v.casefold(), v.lstrip('/').casefold()))
    return out


@dataclass
class Instance:
    port: int
    pid: int | None = None
    project: str = ''
    project_path: str = ''
    files: list = field(default_factory=list)     # the project's programs: [{'name', 'path', 'open'}]
    _open: list | None = field(default=None, repr=False)

    @property
    def url(self):
        return f'http://{HOST}:{self.port}/'

    def open_programs(self, fresh=False):
        """[{'name', 'path'}] of the programs open in this Ghidra's tools. From list_open_programs:
        instance_info's own 'open' flag compares program names with file names, so a program whose
        name is not its file's ('SLUS_214.48' in '/Rule of Rose - US') always shows closed there."""
        if self._open is None or fresh:
            d = _get(self.port, 'list_open_programs', timeout=10) or {}
            self._open = [{'name': p.get('name', ''), 'path': p.get('path', '')}
                          for p in d.get('programs', []) if isinstance(p, dict)]
        return self._open

    def has_program(self, program, open_only=False):
        """program: a program name ('SLUS_214.48') or a project file's name or path."""
        key = program.casefold()
        if key in _names(self.open_programs()):
            return True
        return not open_only and key in _names(self.files)

    def is_project(self, project):
        """project: a path (with or without .gpr) or a bare name."""
        if _is_path(project):
            return bool(self.project_path) and _norm(project) == _norm(self.project_path)
        return project.casefold() == self.project.casefold()

    def describe(self):
        opened = ', '.join(p['name'] for p in self.open_programs()) or '-'
        where = f' ({self.project_path})' if self.project_path else ''
        return f'{self.port}  pid {self.pid}  project {self.project or "?"}{where}  open: {opened}'


def probe(port):
    """The Ghidra instance serving 127.0.0.1:port, or None."""
    info = _get(port, 'mcp/instance_info')
    if not isinstance(info, dict) or 'project' not in info:
        return None
    return Instance(port=port, pid=info.get('pid'), project=info.get('project') or '',
                    project_path=info.get('project_path') or '', files=info.get('programs') or [])


def scan(ports=PORTS):
    """Every instance answering in the port range, in port order."""
    ports = list(ports)
    with concurrent.futures.ThreadPoolExecutor(max(1, len(ports))) as ex:
        return [i for i in ex.map(probe, ports) if i]


def match(instances, project=None, program=None):
    """The instances that fit. The project decides (it is open in one process at most); when no
    instance has it, or none is configured, the program does, instances with it open first."""
    if project:
        hits = [i for i in instances if i.is_project(project)]
        if hits or not program:
            return hits
    if program:
        hits = [i for i in instances if i.has_program(program)]
        return [i for i in hits if i.has_program(program, open_only=True)] or hits
    return list(instances)


def _wanted(project, program):
    parts = [f'project {project}' if project else '', f'program {program}' if program else '']
    return ' / '.join(p for p in parts if p) or 'no project or program configured'


def _load_cache():
    try:
        return json.loads(CACHE_JSON.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def _remember(project, program, inst):
    cache = _load_cache()
    cache[f'{project}|{program}'] = {'port': inst.port, 'pid': inst.pid}
    try:
        CACHE_JSON.parent.mkdir(parents=True, exist_ok=True)
        CACHE_JSON.write_text(json.dumps(cache, indent=1) + '\n', encoding='utf-8')
    except OSError:
        pass


def forget(project=PROJECT, program=PROGRAM):
    cache = _load_cache()
    if cache.pop(f'{project}|{program}', None) is not None:
        try:
            CACHE_JSON.write_text(json.dumps(cache, indent=1) + '\n', encoding='utf-8')
        except OSError:
            pass


def find(project=PROJECT, program=PROGRAM, cache=True):
    """The one instance for (project, program). Raises GhidraError naming what runs instead."""
    if PINNED:
        port = urllib.parse.urlparse(PINNED).port or 80
        inst = probe(port)
        if inst is None:
            if _get(port, 'check_connection') is None:
                raise GhidraError(f'GHIDRA_MCP_URL={PINNED} does not answer')
            inst = Instance(port=port)   # an older plugin without instance_info
        return inst
    if cache:
        # Trusted only for a match by the deciding key: an instance found through the program while
        # the project is configured is re-scanned every time, so the project's own Ghidra wins once
        # it starts.
        hit = _load_cache().get(f'{project}|{program}')
        if hit:
            inst = probe(hit['port'])
            if inst and inst.pid == hit['pid'] and (
                    inst.is_project(project) if project else bool(program) and inst.has_program(program)):
                return inst
    found = scan()
    hits = match(found, project, program)
    if len(hits) == 1:
        _remember(project, program, hits[0])
        return hits[0]
    first, last = PORTS[0], PORTS[-1]
    if not found:
        raise GhidraError(f'no Ghidra is running (nothing answers on {HOST}:{first}-{last}); '
                          f'start it: python ghidra_mcp.py launch')
    running = '\n  '.join(i.describe() for i in found)
    if not hits:
        raise GhidraError(f'no Ghidra instance has {_wanted(project, program)}. Running:\n  {running}\n'
                          f'Open it there, or start its own: python ghidra_mcp.py launch')
    listed = '\n  '.join(i.describe() for i in hits)
    raise GhidraError(f'{len(hits)} Ghidra instances fit ({_wanted(project, program)}):\n  {listed}\n'
                      f'Set "project" in data/ghidra.json (or GHIDRA_MCP_URL) to pick one')


class Ghidra:
    """GhidraMCP client for one program, in whichever instance holds it. `program` always goes in the
    query string: in a POST body GhidraMCP ignores it and writes to the active program."""

    def __init__(self, program=PROGRAM, project=PROJECT):
        self.program, self.project = program, project
        self._inst = None

    @property
    def instance(self):
        if self._inst is None:
            self._inst = find(self.project, self.program)
        return self._inst

    @property
    def url(self):
        return self.instance.url

    def http(self, method, endpoint, body=None, program=None, timeout=300, **query):
        program = program or self.program
        q = {'program': program, **query} if program else query
        path = '/' + endpoint.lstrip('/') + ('?' + urllib.parse.urlencode(q) if q else '')
        data = json.dumps(body).encode() if body is not None else None
        headers = {'Content-Type': 'application/json'} if data is not None else {}
        for attempt in (0, 1):
            inst = self.instance
            conn = _Connection(HOST, inst.port, timeout=timeout)
            try:
                try:
                    conn.connect()
                except OSError:
                    # Nothing on that port any more: Ghidra restarted, maybe on another port. Only a
                    # failed connect is retried; a request that reached Ghidra may have written.
                    if attempt:
                        raise
                    self._inst = None
                    forget(self.project, self.program)
                    continue
                conn.request(method, path, body=data, headers=headers)
                resp = conn.getresponse()
                text = resp.read().decode('utf-8', 'replace')
            finally:
                conn.close()
            if resp.status >= 400:
                raise urllib.error.HTTPError(inst.url + path.lstrip('/'), resp.status, text[:200], resp.headers, None)
            return text

    def up(self):
        """True when the instance answers and, if a program is set, has it open."""
        try:
            inst = self.instance
        except OSError:
            return False
        if probe(inst.port) is None and _get(inst.port, 'check_connection') is None:
            self._inst = None
            return False
        if not self.program:
            return True
        inst.open_programs(fresh=True)
        return inst.has_program(self.program, open_only=True)


_default = Ghidra()


def http(method, endpoint, body=None, program=None, timeout=300, **query):
    """A request to the configured program's instance (see Ghidra.http)."""
    return _default.http(method, endpoint, body=body, program=program, timeout=timeout, **query)


def up():
    return _default.up()


# ---- CLI ------------------------------------------------------------------------------------------

def _target():
    try:
        return find(cache=False)
    except GhidraError:
        return None


def cmd_list(args):
    found = scan()
    target = _target() if (PROJECT or PROGRAM or PINNED) else None
    if args.json:
        print(json.dumps([{'url': i.url, 'port': i.port, 'pid': i.pid, 'project': i.project,
                           'project_path': i.project_path, 'open': i.open_programs(),
                           'target': bool(target and target.port == i.port)} for i in found], indent=1))
        return 0
    if not found:
        print(f'no Ghidra is running (nothing answers on {HOST}:{PORTS[0]}-{PORTS[-1]})')
        return 1
    for i in found:
        print(('* ' if target and target.port == i.port else '  ') + i.describe())
    if PROJECT or PROGRAM:
        print(f'\n* = YamiPan\'s ({_wanted(PROJECT, PROGRAM)})' if target else
              f'\nnone is YamiPan\'s ({_wanted(PROJECT, PROGRAM)}): python ghidra_mcp.py launch')
    return 0


def cmd_which(args):
    try:
        inst = find(cache=False)
    except GhidraError as e:
        print(e)
        return 1
    print(inst.url)
    print(inst.describe())
    if PROGRAM:
        state = 'open' if inst.has_program(PROGRAM, open_only=True) else 'NOT open -- open it in Ghidra'
        print(f'program {PROGRAM}: {state}')
    if PINNED:
        print('(pinned by GHIDRA_MCP_URL)')
    return 0


def cmd_launch(args):
    inst = _target()
    if inst:
        print(f'already running: {inst.describe()}')
        return 0
    if not PROJECT or not _is_path(PROJECT):
        print('data/ghidra.json "project" must be the project\'s path (C:\\...\\Name, no .gpr) to launch it')
        return 1
    gpr = Path(_project_path(PROJECT) + '.gpr')
    if not gpr.exists():
        print(f'{gpr} does not exist yet: create it in Ghidra (File > New Project) or import the game '
              f'headless (bootstrap.md "Ghidra project")')
        return 1
    ghidra_dir = Path(CONFIG.get('ghidra_dir') or '')
    run = ghidra_dir / ('ghidraRun.bat' if os.name == 'nt' else 'ghidraRun')
    if not run.exists():
        print(f'no {run.name} in "{ghidra_dir}": set ghidra_dir in data/ghidra.json or GHIDRA_DIR')
        return 1
    flags = (subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP) if os.name == 'nt' else 0
    subprocess.Popen([str(run), str(gpr)], cwd=str(ghidra_dir), creationflags=flags, close_fds=True,
                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'started Ghidra on {gpr}; waiting up to {args.wait:.0f} s for its GhidraMCP server')
    deadline = time.monotonic() + args.wait
    while time.monotonic() < deadline:
        time.sleep(2)
        hits = [i for i in scan() if i.is_project(PROJECT)]
        if hits:
            print(hits[0].describe())
            return 0
    # GhidraMCP lives in the CodeBrowser tool, not the project window: a project closed without a
    # CodeBrowser open reopens without a server.
    print('no instance with the project answered in time. GhidraMCP runs inside a CodeBrowser window: '
          'open the program from the project window (Ghidra reopens it with the project next time). '
          'Else a dialog may be waiting, or the project is open in another Ghidra (python ghidra_mcp.py list).')
    return 1


def main():
    ap = argparse.ArgumentParser(description='Find the Ghidra instance that holds YamiPan\'s project.')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list', help='every running instance; * marks the one the tools use')
    p.add_argument('--json', action='store_true')
    p.set_defaults(fn=cmd_list)
    sub.add_parser('which', help='the instance the tools resolve to').set_defaults(fn=cmd_which)
    p = sub.add_parser('launch', help='start Ghidra on the project unless it already runs')
    p.add_argument('--wait', type=float, default=180, help='seconds to wait for its server (180)')
    p.set_defaults(fn=cmd_launch)
    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == '__main__':
    main()
