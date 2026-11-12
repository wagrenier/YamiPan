"""Find and talk to YamiPan's Ghidra when several run at once. Import it (the tools' GhidraMCP client)
or run it:

    python ghidra_mcp.py list [--json]   every GhidraMCP server running; * marks the one the tools use
    python ghidra_mcp.py which           the server the tools resolve to, or why none does
    python ghidra_mcp.py launch          start Ghidra on YamiPan's project unless it already serves

How it works (bethington's GhidraMCP 7.0.0, ../ghidra-mcp, read from its source and its live
/mcp/schema 2026-11-02; it replaced LaurieWired's 2.0.0 that day, open-items.md "Decisions"): the plugin
runs one HTTP server per Ghidra *process* (not per window), on 8089 by default (Tool Options; another
Ghidra takes the next free port), and serves every program open in that process: each request names
its program (`program` in the query string, also for a POST, whose parameters go in a JSON body).
GET /mcp/instance_info names the process's project and the programs in it, `open` or not, so the
program names the server: this module scans the port range for the one process with the program
open. The schema of every endpoint is GET /mcp/schema. Answers are JSON; a failure is
{"error": "..."} with HTTP 200.

Configuration: data/ghidra.json; an environment variable overrides each key:

    project    GHIDRA_MCP_PROJECT  the Ghidra project's path without .gpr, for launch and to match a server
    program    GHIDRA_MCP_PROGRAM  the game's program as Ghidra names it; the server is found by it
    ports      GHIDRA_MCP_PORTS    "8089:16": first port and how many to scan
    ghidra_dir GHIDRA_DIR          the Ghidra install (ghidraRun.bat), for launch
    bridge     GHIDRA_MCP_BRIDGE   the plugin's checkout (its python/ holds the MCP bridge), for ghidra_bridge.py
               GHIDRA_MCP_URL      pins one server's port: no scan (the program is still checked)

The resolved port is cached in work/ghidra_instance.json and re-checked (one /mcp/instance_info) when
it is older than CHECK_TTL seconds; a connection that fails before the request is sent re-resolves
once, so a Ghidra restarted on another port is followed.
"""
import argparse
import concurrent.futures
import json
import os
import socket
import subprocess
import sys
import time
import urllib.parse
from dataclasses import asdict, dataclass, field
from http import client as http_client   # module-level http() below is the client call
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
CONFIG_JSON = TOOLS / 'data' / 'ghidra.json'
CACHE_JSON = TOOLS / 'work' / 'ghidra_instance.json'
HOST = '127.0.0.1'
# A closed port on Windows does not refuse at once (the SYN is retried for ~1 s): connect with a short
# timeout. The server may be busy with a long request (a decompile), so the answer gets longer.
CONNECT_TIMEOUT = 0.4
SCAN_TIMEOUT = 10.0
CHECK_TTL = 30.0
# Endpoints that take no `program`.
NO_PROGRAM = {'list_open_programs', 'mcp/instance_info', 'mcp/health', 'mcp/schema', 'check_connection'}


def _config():
    try:
        cfg = json.loads(CONFIG_JSON.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        cfg = {}
    for key, var in (('project', 'GHIDRA_MCP_PROJECT'), ('program', 'GHIDRA_MCP_PROGRAM'),
                     ('ports', 'GHIDRA_MCP_PORTS'), ('ghidra_dir', 'GHIDRA_DIR'), ('bridge', 'GHIDRA_MCP_BRIDGE')):
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
    """No server, or more than one, has the program open, or the connection broke. An OSError, so
    callers that treat a refused connection as "Ghidra is down" treat this the same way."""


class GhidraRequestError(Exception):
    """The server answered with an error: .code ('NOT_FOUND', 'LOCKED', 'HTTP_404', 'ERROR'), .message,
    .status (HTTP)."""

    def __init__(self, code, message, status=None):
        super().__init__(f'[{code}] {message}')
        self.code, self.message, self.status = code, message, status


class _Unreachable(OSError):
    """The connection failed before the request was sent: safe to re-resolve and retry."""


class _Connection(http_client.HTTPConnection):
    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), CONNECT_TIMEOUT)
        self.sock.settimeout(self.timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)


def _error_code(message):
    m = message.lower()
    if 'not found' in m or 'no function' in m:
        return 'NOT_FOUND'
    if 'unable to lock' in m:
        return 'LOCKED'
    return 'ERROR'


def _call(port, method, endpoint, query=None, body=None, timeout=SCAN_TIMEOUT):
    """One request to the server on 127.0.0.1:port -> the parsed answer. query goes in the query string,
    body (a dict) as JSON. Raises _Unreachable when nothing was sent, GhidraError when the connection
    broke after sending, GhidraRequestError when the server reports an error."""
    path = '/' + endpoint.strip('/')
    q = urllib.parse.urlencode({k: ('true' if v is True else 'false' if v is False else v)
                                for k, v in (query or {}).items() if v is not None})
    conn = _Connection(HOST, port, timeout=timeout)
    try:
        try:
            conn.connect()
        except OSError as e:
            raise _Unreachable(f'{HOST}:{port}: {e}') from e
        try:
            conn.request(method, path + ('?' + q if q else ''),
                         body=json.dumps(body).encode() if body is not None else None,
                         headers={'Content-Type': 'application/json'})
            resp = conn.getresponse()
            status, raw = resp.status, resp.read().decode('utf-8', 'replace')
        except (OSError, http_client.HTTPException) as e:
            raise GhidraError(f'{HOST}:{port}{path}: {e.__class__.__name__} {e} (after sending: not retried)') from e
    finally:
        conn.close()
    if status != 200:
        raise GhidraRequestError(f'HTTP_{status}', f'{path}: {raw[:200]!r}', status)
    try:
        data = json.loads(raw)
    except ValueError:
        raise GhidraRequestError('BAD_RESPONSE', f'{path}: not JSON: {raw[:120]!r}', status) from None
    if isinstance(data, dict):
        if isinstance(data.get('error'), str) and data.get('success') is not True:
            raise GhidraRequestError(_error_code(data['error']), data['error'], status)
        if set(data) <= {'data', 'success', 'status'} and 'data' in data:
            return data['data']
    return data


def _project_path(path):
    """A project path as given, without .gpr: Ghidra names the project after it, case included."""
    p = str(path).strip().rstrip('/\\')
    if p.lower().endswith('.gpr'):
        p = p[:-4]
    return os.path.normpath(p)


def _project_open(project=PROJECT):
    """Whether the project looks open in a Ghidra: Ghidra holds <name>.lock while it is open (a Ghidra
    that crashed leaves it behind)."""
    return bool(project) and Path(_project_path(project) + '.lock').exists()


@dataclass
class Instance:
    """One GhidraMCP server: a Ghidra process, its project and the programs open in it."""
    port: int
    pid: int | None = None
    project: str | None = None
    project_path: str | None = None
    programs: list = field(default_factory=list)   # names of the programs open in the process
    version: str | None = None                     # the plugin's

    @property
    def url(self):
        return f'http://{HOST}:{self.port}/'

    def serves(self, program):
        """Whether program (a name or its project file's path) is open in this process."""
        if not program:
            return False
        key = program.strip().lstrip('/').casefold()
        return key in {p.lstrip('/').casefold() for p in self.programs}

    def same_project(self, project=PROJECT):
        return not project or not self.project_path or \
            os.path.normcase(_project_path(project)) == os.path.normcase(_project_path(self.project_path))

    def describe(self):
        what = ', '.join(self.programs) or 'no program open'
        return f'{self.port}  {self.project} [{what}]  GhidraMCP {self.version}, pid {self.pid}'


def probe(port, timeout=SCAN_TIMEOUT):
    """The server on 127.0.0.1:port, or None when nothing (or nothing GhidraMCP 7.x) answers."""
    try:
        info = _call(port, 'GET', 'mcp/instance_info', timeout=timeout)
    except (OSError, GhidraRequestError):
        return None
    if not isinstance(info, dict) or 'programs' not in info:
        return None
    opened = [p.get('path') or p.get('name') for p in info.get('programs') or []
              if isinstance(p, dict) and p.get('open')]
    return Instance(port, info.get('pid'), info.get('project'), info.get('project_path'),
                    [p.lstrip('/') for p in opened if p], info.get('version'))


def scan(ports=PORTS):
    """Every server answering in the port range, in port order."""
    ports = list(ports)
    with concurrent.futures.ThreadPoolExecutor(max(1, len(ports))) as ex:
        return [i for i in ex.map(probe, ports) if i]


def match(instances, program):
    return [i for i in instances if i.serves(program) and (program != PROGRAM or i.same_project())]


def _load_cache():
    try:
        return json.loads(CACHE_JSON.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def _remember(program, inst):
    cache = _load_cache()
    cache[program] = inst.port
    try:
        CACHE_JSON.parent.mkdir(parents=True, exist_ok=True)
        CACHE_JSON.write_text(json.dumps(cache, indent=1) + '\n', encoding='utf-8')
    except OSError:
        pass


def forget(program=PROGRAM):
    cache = _load_cache()
    if cache.pop(program, None) is not None:
        try:
            CACHE_JSON.write_text(json.dumps(cache, indent=1) + '\n', encoding='utf-8')
        except OSError:
            pass


def _none_serves(program, found, hits):
    first, last = PORTS[0], PORTS[-1]
    if hits:
        listed = '\n  '.join(i.describe() for i in hits)
        return f'{len(hits)} Ghidra processes have {program} open:\n  {listed}\nClose one, or pin one with GHIDRA_MCP_URL'
    lock = (f' The project is open ({_project_path(PROJECT)}.lock exists): open {program} in its CodeBrowser.'
            if program == PROGRAM and _project_open() else '')
    if not found:
        return (f'no GhidraMCP server answers on {HOST}:{first}-{last} (a busy one answers after its current '
                f'request).{lock or " Start it: python ghidra_mcp.py launch"}')
    running = '\n  '.join(i.describe() for i in found)
    return f'no Ghidra has {program} open.{lock} Running:\n  {running}'


def find(program=PROGRAM, cache=True, timeout=SCAN_TIMEOUT):
    """The one server whose process has program open. Raises GhidraError naming what runs instead."""
    if not program:
        raise GhidraError('no program configured: data/ghidra.json "program" (or GHIDRA_MCP_PROGRAM)')
    if PINNED:
        port = urllib.parse.urlparse(PINNED).port or 80
        inst = probe(port, timeout)
        if inst is None:
            raise GhidraError(f'GHIDRA_MCP_URL={PINNED} does not answer')
        if not inst.serves(program):
            raise GhidraError(f'GHIDRA_MCP_URL={PINNED} does not have {program} open ({inst.describe()})')
        return inst
    if cache:
        port = _load_cache().get(program)
        if port in PORTS:
            inst = probe(port, timeout)
            if inst and match([inst], program):
                return inst
    found = scan()
    hits = match(found, program)
    if len(hits) == 1:
        _remember(program, hits[0])
        return hits[0]
    forget(program)
    raise GhidraError(_none_serves(program, found, hits))


class Ghidra:
    """GhidraMCP client for one program, in whichever Ghidra has it open:
    Ghidra('SCES_538.51').http('GET', 'find_functions', limit=500) -> {'functions': [...], 'total': ...}."""

    def __init__(self, program=PROGRAM):
        self.program = program
        self._inst, self._checked = None, 0.0

    def instance(self, timeout=SCAN_TIMEOUT):
        """The server, re-checked (one /mcp/instance_info) when the last check is older than CHECK_TTL."""
        now = time.monotonic()
        if self._inst is not None and now - self._checked < CHECK_TTL:
            return self._inst
        if self._inst is None:
            inst = find(self.program, timeout=timeout)   # the cache's probe or a scan: checked
        else:
            inst = probe(self._inst.port, timeout)
            if not (inst and inst.serves(self.program)):
                # Ghidra restarted (maybe on another port), or the program was closed.
                forget(self.program)
                inst = find(self.program, cache=False)
        self._inst, self._checked = inst, now
        return inst

    @property
    def url(self):
        return self.instance().url

    def http(self, method, endpoint, body=None, timeout=300, **query):
        """method endpoint -> the parsed answer. query goes in the query string with `program` added;
        body (a dict, for a POST) as JSON. Raises GhidraError when no server has the program open,
        GhidraRequestError when the server reports an error. Only a failed connect is retried; a
        request that reached Ghidra may have written."""
        if endpoint.strip('/') not in NO_PROGRAM:
            query = {'program': self.program, **query}
        for attempt in (0, 1):
            inst = self.instance(timeout)
            try:
                return _call(inst.port, method, endpoint, query, body, timeout)
            except _Unreachable as e:
                self._inst = None
                forget(self.program)
                if attempt:
                    raise GhidraError(f'{inst.describe()} stopped answering: {e}') from e

    def up(self):
        """True when a server has the program open."""
        try:
            self.instance()
        except GhidraError:
            return False
        return True


_default = Ghidra()


def http(method, endpoint, body=None, timeout=300, **query):
    """A request for the configured program (see Ghidra.http)."""
    return _default.http(method, endpoint, body, timeout=timeout, **query)


def up():
    return _default.up()


# ---- CLI ------------------------------------------------------------------------------------------

def cmd_list(args):
    found = scan()
    if args.json:
        print(json.dumps([{'url': i.url, **asdict(i), 'yamipan': bool(match([i], PROGRAM))} for i in found], indent=1))
        return 0
    if not found:
        print(f'no GhidraMCP server answers on {HOST}:{PORTS[0]}-{PORTS[-1]}')
        return 1
    hits = match(found, PROGRAM)
    for i in found:
        print(('* ' if i in hits else '  ') + i.describe())
    if PROGRAM:
        print(f'\n* = YamiPan\'s (program {PROGRAM})' if len(hits) == 1 else
              f'\n{len(hits)} Ghidras have YamiPan\'s program ({PROGRAM}) open: pin one with GHIDRA_MCP_URL'
              if hits else f'\nnone has YamiPan\'s program ({PROGRAM}) open: python ghidra_mcp.py which')
    return 0


def cmd_which(args):
    try:
        inst = find(cache=False)
    except GhidraError as e:
        print(e)
        return 1
    print(inst.url)
    print(inst.describe())
    if PINNED:
        print('(pinned by GHIDRA_MCP_URL)')
    return 0


def cmd_launch(args):
    try:
        print(f'already serving: {find(cache=False).describe()}')
        return 0
    except GhidraError:
        pass
    if not PROJECT:
        print('data/ghidra.json "project" must be the project\'s path (C:\\...\\Name, no .gpr) to launch it')
        return 1
    if _project_open():
        print(f'{_project_path(PROJECT)}.lock exists: the project is open in a Ghidra (or one crashed) but no '
              f'server has {PROGRAM} open. Open {PROGRAM} in its CodeBrowser.')
        return 1
    gpr = Path(_project_path(PROJECT) + '.gpr')
    if not gpr.exists():
        print(f'{gpr} does not exist: create it in Ghidra or import the game headless (bootstrap.md 2)')
        return 1
    ghidra_dir = Path(CONFIG.get('ghidra_dir') or '')
    run = ghidra_dir / ('ghidraRun.bat' if os.name == 'nt' else 'ghidraRun')
    if not run.exists():
        print(f'no {run.name} in "{ghidra_dir}": set ghidra_dir in data/ghidra.json or GHIDRA_DIR')
        return 1
    flags = (subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP) if os.name == 'nt' else 0
    subprocess.Popen([str(run), str(gpr)], cwd=str(ghidra_dir), creationflags=flags, close_fds=True,
                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f'started Ghidra on {gpr}; waiting up to {args.wait:.0f} s for a server with {PROGRAM} open')
    deadline = time.monotonic() + args.wait
    while time.monotonic() < deadline:
        time.sleep(2)
        hits = match(scan(), PROGRAM)
        if hits:
            print(hits[0].describe())
            return 0
    # GhidraMCP lives in the CodeBrowser, not the project window: a project closed without a CodeBrowser
    # open reopens without a server.
    print(f'no server with {PROGRAM} open answered in time. GhidraMCP runs inside a CodeBrowser window: open '
          f'the program from the project window (Ghidra reopens it with the project next time). Else a dialog '
          f'may be waiting, or the port is taken (python ghidra_mcp.py list; the Ghidra log says).')
    return 1


def main():
    ap = argparse.ArgumentParser(description='Find the GhidraMCP server that serves YamiPan\'s program.')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list', help='every running server; * marks the one the tools use')
    p.add_argument('--json', action='store_true')
    p.set_defaults(fn=cmd_list)
    sub.add_parser('which', help='the server the tools resolve to').set_defaults(fn=cmd_which)
    p = sub.add_parser('launch', help='start Ghidra on the project unless a server already has the program open')
    p.add_argument('--wait', type=float, default=180, help='seconds to wait for its server (180)')
    p.set_defaults(fn=cmd_launch)
    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == '__main__':
    main()
