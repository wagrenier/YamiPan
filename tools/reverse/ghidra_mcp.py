"""Find and talk to YamiPan's Ghidra when several run at once. Import it (the tools' GhidraMCP client)
or run it:

    python ghidra_mcp.py list [--json]   every GhidraMCP backend running; * marks the one the tools use
    python ghidra_mcp.py which           the backend the tools resolve to, or why none does
    python ghidra_mcp.py launch          start Ghidra on YamiPan's project unless it already serves

How it works (GhidraMCP 2.0.0, LaurieWired/GhidraMCP, read from its source 2026-10-31): the plugin is
loaded per CodeBrowser *window*, and each window runs its own loopback HTTP backend on the port set in
Tool Options -> GhidraMCP HTTP Server -> Server Port (default 8080). There is no fallback: a second
window or Ghidra on a busy port logs an error and serves nothing, so each Ghidra that should serve gets
its own port by hand. A backend serves only the program active in its window (requests have no
`program` parameter) and GET /health names it. So the program names the backend: this module scans the
port range for it, and checks it again before every request, so a request never lands on another game
because a window's active tab changed. Every answer is {success, data | error: {code, message}}.

Configuration: data/ghidra.json; an environment variable overrides each key:

    project    GHIDRA_MCP_PROJECT  the Ghidra project's path without .gpr, for launch and its lock file
    program    GHIDRA_MCP_PROGRAM  the game's program as Ghidra names it; the backend is found by it
    ports      GHIDRA_MCP_PORTS    "8080:16": first port and how many to scan
    ghidra_dir GHIDRA_DIR          the Ghidra install (ghidraRun.bat), for launch
               GHIDRA_MCP_URL      pins one backend's port: no scan (the program is still checked)

The resolved port is cached in work/ghidra_instance.json and re-checked by the per-request /health; a
connection that fails before the request is sent re-resolves once, so a Ghidra restarted on another
port is followed.

The wheel ghidra_bridge.py runs on is also a package named ghidra_mcp: tools here import this file (a
script's directory comes first on sys.path); the bridge loads it by path under another name.
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
from dataclasses import asdict, dataclass
from http import client as http_client   # module-level http() below is the client call
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
CONFIG_JSON = TOOLS / 'data' / 'ghidra.json'
CACHE_JSON = TOOLS / 'work' / 'ghidra_instance.json'
HOST = '127.0.0.1'
# A closed port on Windows does not refuse at once (the SYN is retried for ~1 s): connect with a short
# timeout. A backend answers one request at a time (a decompile may take 60 s), so the answer itself
# gets longer: SCAN_TIMEOUT while scanning, the request's own timeout when checking before a request.
CONNECT_TIMEOUT = 0.4
SCAN_TIMEOUT = 10.0


def _config():
    try:
        cfg = json.loads(CONFIG_JSON.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        cfg = {}
    for key, var in (('project', 'GHIDRA_MCP_PROJECT'), ('program', 'GHIDRA_MCP_PROGRAM'),
                     ('ports', 'GHIDRA_MCP_PORTS'), ('ghidra_dir', 'GHIDRA_DIR')):
        if os.environ.get(var):
            cfg[key] = os.environ[var]
    return cfg


CONFIG = _config()
PROJECT = CONFIG.get('project') or None
PROGRAM = CONFIG.get('program') or None
PINNED = os.environ.get('GHIDRA_MCP_URL', '').rstrip('/') or None
_first, _, _count = str(CONFIG.get('ports') or '8080:16').partition(':')
PORTS = range(int(_first), int(_first) + int(_count or 16))


class GhidraError(ConnectionError):
    """No backend, or more than one, serves the program, or the connection broke. An OSError, so
    callers that treat a refused connection as "Ghidra is down" treat this the same way."""


class GhidraRequestError(Exception):
    """The backend answered with an error: .code ('FUNCTION_NOT_FOUND', 'INVALID_PARAMETER', ...),
    .message, .status (HTTP)."""

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


def _call(port, method, endpoint, params=None, timeout=SCAN_TIMEOUT):
    """One request to the backend on 127.0.0.1:port -> the answer's `data`. params go in the query
    string for GET, in a form body otherwise. Raises _Unreachable when nothing was sent, GhidraError
    when the connection broke after sending, GhidraRequestError when the backend reports an error."""
    data = urllib.parse.urlencode({k: v for k, v in (params or {}).items() if v is not None})
    path = '/' + endpoint.strip('/')
    conn = _Connection(HOST, port, timeout=timeout)
    try:
        try:
            conn.connect()
        except OSError as e:
            raise _Unreachable(f'{HOST}:{port}: {e}') from e
        try:
            if method == 'GET':
                conn.request('GET', path + ('?' + data if data else ''))
            else:
                conn.request(method, path, body=data.encode(),
                             headers={'Content-Type': 'application/x-www-form-urlencoded'})
            resp = conn.getresponse()
            status, raw = resp.status, resp.read().decode('utf-8', 'replace')
        except (OSError, http_client.HTTPException) as e:
            raise GhidraError(f'{HOST}:{port}{path}: {e.__class__.__name__} {e} (after sending: not retried)') from e
    finally:
        conn.close()
    try:
        body = json.loads(raw)
    except ValueError:
        raise GhidraRequestError('BAD_RESPONSE', f'HTTP {status}, not JSON (not GhidraMCP 2.x?): {raw[:120]!r}',
                                 status) from None
    if not isinstance(body, dict) or 'success' not in body:
        raise GhidraRequestError('BAD_RESPONSE', f'HTTP {status}, no "success" (not GhidraMCP 2.x?)', status)
    if not body['success']:
        err = body.get('error') or {}
        raise GhidraRequestError(err.get('code', 'UNKNOWN_ERROR'), err.get('message', 'unknown error'), status)
    return body.get('data')


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
    """One GhidraMCP backend: a CodeBrowser window and the program active in it."""
    port: int
    program: str | None = None        # the active program's name; None when the window has none
    program_path: str | None = None   # its file in the project ('/SCES_538.51')
    language: str | None = None
    ghidra: str | None = None         # Ghidra's version
    version: str | None = None        # the plugin's

    @property
    def url(self):
        return f'http://{HOST}:{self.port}/'

    def serves(self, program):
        """Whether program (a program name or its project file's path) is active in this window."""
        if not program or not self.program:
            return False
        key = program.strip().lstrip('/').casefold()
        return key in (self.program.casefold(), (self.program_path or '').lstrip('/').casefold())

    def describe(self):
        what = f'{self.program} ({self.program_path}, {self.language})' if self.program else 'no program open'
        return f'{self.port}  {what}  Ghidra {self.ghidra}, GhidraMCP {self.version}'


def probe(port, timeout=SCAN_TIMEOUT):
    """The backend on 127.0.0.1:port, or None when nothing (or nothing GhidraMCP 2.x) answers."""
    try:
        h = _call(port, 'GET', 'health', timeout=timeout)
    except (OSError, GhidraRequestError):
        return None
    if not isinstance(h, dict) or h.get('status') != 'ok':
        return None
    p = h.get('program') or {}
    return Instance(port, p.get('name'), p.get('project_path'), p.get('language_id'),
                    h.get('ghidra_version'), h.get('version'))


def scan(ports=PORTS):
    """Every backend answering in the port range, in port order."""
    ports = list(ports)
    with concurrent.futures.ThreadPoolExecutor(max(1, len(ports))) as ex:
        return [i for i in ex.map(probe, ports) if i]


def match(instances, program):
    return [i for i in instances if i.serves(program)]


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
        return (f'{len(hits)} Ghidra windows have {program} active:\n  {listed}\n'
                f'Close one, or pin one with GHIDRA_MCP_URL')
    lock = (f' The project is open ({_project_path(PROJECT)}.lock exists): open {program} in its CodeBrowser '
            f'and keep it the active tab; that window serves it.' if program == PROGRAM and _project_open() else '')
    if not found:
        return (f'no GhidraMCP backend answers on {HOST}:{first}-{last} (a busy one answers after its current '
                f'request).{lock or " Start it: python ghidra_mcp.py launch"}')
    running = '\n  '.join(i.describe() for i in found)
    return f'no Ghidra window has {program} active.{lock} Running:\n  {running}'


def find(program=PROGRAM, cache=True, timeout=SCAN_TIMEOUT):
    """The one backend whose window has program active. Raises GhidraError naming what runs instead."""
    if not program:
        raise GhidraError('no program configured: data/ghidra.json "program" (or GHIDRA_MCP_PROGRAM)')
    if PINNED:
        port = urllib.parse.urlparse(PINNED).port or 80
        inst = probe(port, timeout)
        if inst is None:
            raise GhidraError(f'GHIDRA_MCP_URL={PINNED} does not answer')
        if not inst.serves(program):
            raise GhidraError(f'GHIDRA_MCP_URL={PINNED} has {inst.program or "no program"} active, not {program}')
        return inst
    if cache:
        port = _load_cache().get(program)
        if port in PORTS:
            inst = probe(port, timeout)
            if inst and inst.serves(program):
                return inst
    found = scan()
    hits = match(found, program)
    if len(hits) == 1:
        _remember(program, hits[0])
        return hits[0]
    forget(program)
    raise GhidraError(_none_serves(program, found, hits))


class Ghidra:
    """GhidraMCP client for one program, in whichever window has it active:
    Ghidra('SCES_538.51').http('GET', 'functions', limit=2000) -> {'items': [...], 'has_more': ...}."""

    def __init__(self, program=PROGRAM):
        self.program = program
        self._inst = None

    def instance(self, timeout=SCAN_TIMEOUT):
        """The backend, after a /health that shows the program still active in its window."""
        if self._inst is None:
            self._inst = find(self.program, timeout=timeout)   # the cache's probe or a scan: checked
            return self._inst
        inst = probe(self._inst.port, timeout)
        if not (inst and inst.serves(self.program)):
            # Ghidra restarted (maybe on another port), or that window's active tab changed.
            self._inst = None
            forget(self.program)
            inst = find(self.program, cache=False)
        self._inst = inst
        return inst

    @property
    def url(self):
        return self.instance().url

    def http(self, method, endpoint, timeout=300, **params):
        """method endpoint with params (query string for GET, form body for POST) -> the answer's
        `data`, as GhidraMCP 2.0.0 returns it. Checks the program is still active first (one /health).
        Raises GhidraError when no backend serves it, GhidraRequestError when the backend reports an
        error. Only a failed connect is retried; a request that reached Ghidra may have written."""
        for attempt in (0, 1):
            inst = self.instance(timeout)
            try:
                return _call(inst.port, method, endpoint, params, timeout)
            except _Unreachable as e:
                self._inst = None
                forget(self.program)
                if attempt:
                    raise GhidraError(f'{inst.describe()} stopped answering: {e}') from e

    def up(self):
        """True when a backend has the program active."""
        try:
            self.instance()
        except GhidraError:
            return False
        return True


_default = Ghidra()


def http(method, endpoint, timeout=300, **params):
    """A request to the configured program's backend (see Ghidra.http)."""
    return _default.http(method, endpoint, timeout=timeout, **params)


def up():
    return _default.up()


# ---- CLI ------------------------------------------------------------------------------------------

def cmd_list(args):
    found = scan()
    if args.json:
        print(json.dumps([{'url': i.url, **asdict(i), 'yamipan': i.serves(PROGRAM)} for i in found], indent=1))
        return 0
    if not found:
        print(f'no GhidraMCP backend answers on {HOST}:{PORTS[0]}-{PORTS[-1]}')
        return 1
    for i in found:
        print(('* ' if i.serves(PROGRAM) else '  ') + i.describe())
    hits = match(found, PROGRAM)
    if PROGRAM:
        print(f'\n* = YamiPan\'s (program {PROGRAM})' if len(hits) == 1 else
              f'\n{len(hits)} windows have YamiPan\'s program ({PROGRAM}) active: pin one with GHIDRA_MCP_URL'
              if hits else f'\nnone has YamiPan\'s program ({PROGRAM}) active: python ghidra_mcp.py which')
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
              f'window serves {PROGRAM}. Open {PROGRAM} in its CodeBrowser and keep it the active tab.')
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
    print(f'started Ghidra on {gpr}; waiting up to {args.wait:.0f} s for a window serving {PROGRAM}')
    deadline = time.monotonic() + args.wait
    while time.monotonic() < deadline:
        time.sleep(2)
        hits = match(scan(), PROGRAM)
        if hits:
            print(hits[0].describe())
            return 0
    # GhidraMCP lives in the CodeBrowser, not the project window: a project closed without a CodeBrowser
    # open reopens without a backend.
    print(f'no window serving {PROGRAM} answered in time. GhidraMCP runs inside a CodeBrowser window: open '
          f'the program from the project window (Ghidra reopens it with the project next time). Else a dialog '
          f'may be waiting, or the port is taken (python ghidra_mcp.py list; the Ghidra log says).')
    return 1


def main():
    ap = argparse.ArgumentParser(description='Find the GhidraMCP backend that serves YamiPan\'s program.')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list', help='every running backend; * marks the one the tools use')
    p.add_argument('--json', action='store_true')
    p.set_defaults(fn=cmd_list)
    sub.add_parser('which', help='the backend the tools resolve to').set_defaults(fn=cmd_which)
    p = sub.add_parser('launch', help='start Ghidra on the project unless a window already serves it')
    p.add_argument('--wait', type=float, default=180, help='seconds to wait for its backend (180)')
    p.set_defaults(fn=cmd_launch)
    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == '__main__':
    main()
