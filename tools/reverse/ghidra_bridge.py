"""The `ghidra` MCP server for Claude in YamiPan: bethington's GhidraMCP bridge (`bridge_mcp_ghidra`, the
plugin's own MCP<->HTTP bridge, from the checkout in data/ghidra.json "bridge"), connected on startup
to the Ghidra process that has YamiPan's project open. Not run by hand: .mcp.json starts it with the
python of the venv that has its dependencies (mcp < 2, pydantic 2).

What this wrapper adds to the bridge as it ships: the checkout's python/ on sys.path (it is not
pip-installed), `--project <name>` from data/ghidra.json "project" (its last path component, the
name Ghidra shows), and a note in the server's instructions that every call should name YamiPan's
program. The bridge itself finds the plugin's servers (TCP 8089.., or its Unix sockets), retries the
connection while Ghidra is down, and offers list_instances / connect_instance to switch projects.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import ghidra_mcp as gm  # noqa: E402

BRIDGE = Path(gm.CONFIG.get('bridge') or TOOLS.parents[3] / 'ghidra-mcp') / 'python'
sys.path.insert(0, str(BRIDGE))

from bridge_mcp_ghidra import cli  # noqa: E402  (the plugin's bridge)
from bridge_mcp_ghidra.server import mcp  # noqa: E402

NOTE = f"""
In YamiPan the game is the program {gm.PROGRAM} in the Ghidra project {Path(gm.PROJECT or '').name!r}: pass
program="{gm.PROGRAM}" on every call (another program may be open beside it). Names follow the game's
style (rx*, Class::method), not the plugin's PascalCase gate: rename with strict_mode="off". Writes are
not saved until save_program.
"""


def main():
    mcp._mcp_server.instructions = (mcp._mcp_server.instructions or '') + NOTE
    if '--project' not in sys.argv and gm.PROJECT:
        sys.argv[1:1] = ['--project', Path(gm.PROJECT).name]
    cli.main()


if __name__ == '__main__':
    main()
