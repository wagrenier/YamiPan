"""The `ghidra` MCP server for Claude in YamiPan: GhidraMCP's own bridge (bridge_mcp_ghidra.py),
pointed at YamiPan's Ghidra when several run. Not run by hand: .mcp.json starts it with the bridge's
venv python (it needs the `mcp` package); extra arguments go to the bridge (--lazy ...).

On Windows the bridge alone auto-connects to whatever answers on 8089, lists only the instance it is
connected to, and its connect_instance falls back to 8089 when no project of that name answers: with
two Ghidras it silently drives the wrong one. This wrapper loads the bridge unchanged and replaces four
of its pieces with ghidra_mcp.py's discovery:

- start-up connects to the instance ghidra_mcp.find() resolves (data/ghidra.json's project, else its
  program), or to none -- never to a port by default;
- list_instances lists every instance on 8089..8104: project, open programs, which one is YamiPan's;
- connect_instance(target) takes a project name or path, a program name or a port, and refuses a
  target that fits several instances;
- a lost connection resolves the connected project again (Ghidra restarted, maybe on another port).

The bridge's Unix-socket discovery is switched off where Python has no AF_UNIX (Windows): the plugin's
socket files exist there (F:\\tmp\\ghidra-mcp-<user>) but cannot be connected to, and two of them made
the bridge refuse to auto-connect at all.
"""
import importlib.util
import json
import socket
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ghidra_mcp as gm  # noqa: E402

from mcp.server.fastmcp import Context  # noqa: E402  (the bridge's venv has it)


def _load_bridge():
    path = Path(gm.CONFIG.get('bridge') or '')
    if not path.is_file():
        sys.exit(f'ghidra_bridge: no bridge at "{path}" (data/ghidra.json "bridge" or GHIDRA_MCP_BRIDGE)')
    spec = importlib.util.spec_from_file_location('bridge_mcp_ghidra', path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)   # registers the static tools; its main() does not run
    return mod


b = _load_bridge()
log = b.logger
# What start-up and reconnects look for; connect_instance retargets it to the instance it picked.
target = {'project': gm.PROJECT, 'program': gm.PROGRAM}


def _use(inst):
    """Point the bridge at inst over TCP and register that Ghidra's tools; returns how many."""
    b._active_tcp = inst.url.rstrip('/')
    b._active_socket = None
    b._transport_mode = 'tcp'
    b._connected_project = inst.project or None
    try:
        return b._fetch_and_register_schema()
    except Exception:
        b._active_tcp, b._transport_mode = None, 'none'
        raise


def _auto_connect():
    try:
        inst = gm.find(target['project'], target['program'], cache=False)
    except gm.GhidraError as e:
        log.info(f"YamiPan's Ghidra is not connected: {e}. Tools register on connect_instance().")
        return
    try:
        log.info(f'Connected to {inst.describe()}: {_use(inst)} tools')
    except Exception as e:
        log.warning(f'Schema fetch from {inst.url} failed: {e}')


def _try_reconnect():
    try:
        inst = gm.find(target['project'], target['program'], cache=False)
        _use(inst)
    except Exception as e:
        log.info(f'Reconnect failed: {e}')
        return False
    log.info(f'Reconnected to {inst.describe()}')
    return True


def _yamipan_port(found):
    """The port of the instance YamiPan's tools use (data/ghidra.json), None when not exactly one."""
    hits = gm.match(found, gm.PROJECT, gm.PROGRAM)
    return hits[0].port if len(hits) == 1 else None


def _record(inst, connected, mine):
    return {'url': inst.url, 'port': inst.port, 'pid': inst.pid, 'project': inst.project,
            'project_path': inst.project_path, 'open_programs': [p['name'] for p in inst.open_programs()],
            'connected': connected, 'yamipan': inst.port == mine}


def list_instances() -> str:
    """List the running Ghidra instances (GhidraMCP on 127.0.0.1:8089..8104, one per Ghidra process):
    port, pid, project, the programs open in it. `connected` marks the one this bridge talks to,
    `yamipan` the one YamiPan's tools use (tools/reverse/data/ghidra.json's project / program)."""
    found = gm.scan()
    mine = _yamipan_port(found)
    active = b._active_tcp if b._transport_mode == 'tcp' else None
    out = [_record(i, active == i.url.rstrip('/'), mine) for i in found]
    note = '' if found else f'No Ghidra is running on ports {gm.PORTS[0]}-{gm.PORTS[-1]}.'
    return json.dumps({'instances': out, 'note': note} if note else {'instances': out}, indent=2)


async def connect_instance(project: str, ctx: Context | None = None) -> str:
    """Switch the bridge to another Ghidra instance and register its analysis tools.

    project: a project name or path ("YamiPan", "C:\\Users\\willi\\Ghidra"), a program name
    ("SLUS_214.48"), or a port ("8090"); it must fit exactly one running instance (list_instances()
    shows them). Until the next connect_instance, reconnects after a lost connection look for the
    same project again. Clients that cache tools/list must re-list after this call."""
    found = gm.scan()
    key = project.strip()
    if key.isdigit():
        hits = [i for i in found if i.port == int(key)]
    else:
        hits = [i for i in found if i.is_project(key)] or gm.match(found, None, key)
    if len(hits) != 1:
        why = 'fits no' if not hits else f'fits {len(hits)}'
        return json.dumps({'error': f"'{key}' {why} running Ghidra instance (project, program or port)",
                           'available': [i.describe() for i in found]})
    inst = hits[0]
    try:
        count = _use(inst)
    except Exception as e:
        return json.dumps({'error': f'schema fetch from {inst.url} failed: {e}'})
    target.update(project=inst.project_path or inst.project or None, program=None)
    await b._notify_tools_changed(ctx)
    return json.dumps({'connected': True, 'transport': 'tcp', **_record(inst, True, _yamipan_port(found)),
                       'tools_registered': count, 'tools_total': len(b._full_schema),
                       'loaded_groups': sorted(b._loaded_groups)})


for _fn in (list_instances, connect_instance):
    b.mcp.remove_tool(_fn.__name__)
    b.mcp.tool()(_fn)
b._auto_connect = _auto_connect
b._try_reconnect = _try_reconnect
if not hasattr(socket, 'AF_UNIX'):
    b.discover_instances = lambda: []

if __name__ == '__main__':
    b.main()
