"""The `ghidra` MCP server for Claude in YamiPan: the GhidraMCP 2.0.0 wheel's own server (the
`ghidra-mcp` command, LaurieWired/GhidraMCP), aimed at the Ghidra window that has YamiPan's program
active. Not run by hand: .mcp.json starts it with the python of the venv the wheel is installed in.

The wheel alone talks to one URL (--ghidra-server, 127.0.0.1:8080) and to whatever program is active in
that window: with two Ghidras, or another program's tab in front, it silently reads and writes the
wrong one. This wrapper builds the wheel's server unchanged around its HTTP client, re-aimed before
every request by ghidra_mcp.py: the window whose active program is the followed one (data/ghidra.json's
"program"), checked by one /health, else a scan of the port range; no such window -> the tool fails
with NO_BACKEND and says what runs. Two tools are added:

- list_instances: every backend on the port range and the program active in each;
- connect_instance(target): follow another program (a name, or the port whose active program to
  follow), e.g. FF2 in MioPan's Ghidra, until the next call.

Ghidra may start after the session: every tool is registered anyway and finds it on its first call.
"""
import importlib.util
import json
import logging
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
# ghidra_mcp.py here would shadow the wheel's ghidra_mcp package: drop this directory from the path and
# load the file under another name.
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
_spec = importlib.util.spec_from_file_location('yamipan_ghidra', TOOLS / 'ghidra_mcp.py')
gm = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = gm
_spec.loader.exec_module(gm)

from ghidra_mcp.backend import BackendError, GhidraBackend  # noqa: E402  (the wheel)
from ghidra_mcp.server import create_server  # noqa: E402

NOTE = """
In YamiPan these tools act on one program, data/ghidra.json's (connect_instance follows another), in
whichever Ghidra window has it as the active tab; a call fails with NO_BACKEND when no window does.
Changes are not saved: they stay in Ghidra until it saves (Ctrl+S there), and a crash loses them.
"""


class YamiPanBackend(GhidraBackend):
    """The wheel's HTTP client, re-aimed before every request at the window serving the followed
    program."""

    def __init__(self, program):
        super().__init__(f'http://{gm.HOST}:{gm.PORTS[0]}/')
        self.client = gm.Ghidra(program)

    def request(self, method, endpoint, params=None):
        try:
            inst = self.client.instance(self.timeout[1])
        except gm.GhidraError as e:
            raise BackendError('NO_BACKEND', str(e)) from e
        self.base_url = inst.url
        return super().request(method, endpoint, params)


backend = YamiPanBackend(gm.PROGRAM)


def _record(inst):
    return {'url': inst.url, 'port': inst.port, 'program': inst.program, 'program_path': inst.program_path,
            'language': inst.language, 'ghidra': inst.ghidra, 'plugin': inst.version,
            'followed': inst.serves(backend.client.program), 'yamipan': inst.serves(gm.PROGRAM)}


def list_instances() -> str:
    """List the GhidraMCP backends on 127.0.0.1 (one per Ghidra CodeBrowser window, ports from
    tools/reverse/data/ghidra.json): the program active in each. `followed` marks the one these tools
    act on, `yamipan` the one with YamiPan's program."""
    found = gm.scan()
    out = {'followed_program': backend.client.program, 'instances': [_record(i) for i in found]}
    if not found:
        out['note'] = f'No GhidraMCP backend answers on ports {gm.PORTS[0]}-{gm.PORTS[-1]}.'
    return json.dumps(out, indent=2)


def connect_instance(target: str) -> str:
    """Make the Ghidra tools follow another program until the next call: a program name
    ("SLES_523.84") or a port ("8081") whose active program to follow. It must be the active program of
    exactly one window (list_instances shows them). connect_instance("SCES_538.51") returns to
    YamiPan's program."""
    key = target.strip()
    found = gm.scan()
    program = key
    if key.isdigit():
        at = [i for i in found if i.port == int(key)]
        program = at[0].program if at else None
        if not program:
            return json.dumps({'error': f'no window with a program open answers on port {key}',
                               'available': [i.describe() for i in found]})
    hits = gm.match(found, program)
    if len(hits) != 1:
        why = 'is active in no' if not hits else f'is active in {len(hits)}'
        return json.dumps({'error': f"'{program}' {why} Ghidra window", 'available': [i.describe() for i in found]})
    backend.client = gm.Ghidra(program)
    return json.dumps({'connected': True, **_record(hits[0])})


def main():
    # stdout carries the MCP stream: logs go to stderr.
    logging.basicConfig(level=logging.INFO, stream=sys.stderr, format='%(levelname)s:%(name)s:%(message)s')
    log = logging.getLogger('ghidra_bridge')
    health = backend.health()
    if health is None:
        log.info(f"No window serves {gm.PROGRAM} yet; every tool is registered and finds it when called.")
    else:
        log.info(f"Following {gm.PROGRAM} at {backend.base_url} (Ghidra {health.get('ghidra_version')}, "
                 f"GhidraMCP {health.get('version')})")
    mcp = create_server(backend, capabilities=set(health.get('capabilities', [])) if health else None)
    mcp._mcp_server.instructions = (mcp.instructions or '') + NOTE
    for fn in (list_instances, connect_instance):
        mcp.tool()(fn)
    mcp.run(transport='stdio')


if __name__ == '__main__':
    main()
