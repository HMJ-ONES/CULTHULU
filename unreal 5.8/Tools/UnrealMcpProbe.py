"""Probe Epic's local editor MCP without external dependencies.

Run with Unreal's bundled Python. Defaults to read-only tool discovery.
Tool invocations are serialized and always use a fresh MCP session.
"""
import argparse
import json
import urllib.request
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool")
    parser.add_argument("--arguments", default="{}")
    parser.add_argument("--output", help="Save the result JSON instead of printing it (useful for captures).")
    args = parser.parse_args()
    url = "http://127.0.0.1:8000/mcp"
    headers = {"Content-Type": "application/json",
               "Accept": "application/json, text/event-stream"}

    def rpc(method, params, request_id=None):
        payload = {"jsonrpc": "2.0", "method": method, "params": params}
        if request_id is not None:
            payload["id"] = request_id
        request = urllib.request.Request(url, json.dumps(payload).encode(), headers)
        with urllib.request.urlopen(request, timeout=45) as response:
            session = response.headers.get("Mcp-Session-Id")
            if session:
                headers["Mcp-Session-Id"] = session
            raw = response.read().decode()
        if not raw:
            return None
        if raw.lstrip().startswith("data:") or "\ndata:" in raw:
            raw = next(line[5:].strip() for line in raw.splitlines()
                       if line.startswith("data:") and '"id"' in line)
        result = json.loads(raw)
        if "error" in result:
            raise RuntimeError(result["error"])
        return result.get("result")

    info = rpc("initialize", {"protocolVersion": "2024-11-05",
               "capabilities": {}, "clientInfo": {
                   "name": "cultulhu-setup-probe", "version": "1.0"}}, 1)
    print(json.dumps({"server": info}, ensure_ascii=False))
    headers["MCP-Protocol-Version"] = info["protocolVersion"]
    rpc("notifications/initialized", {})
    if args.tool:
        result = rpc("tools/call", {"name": args.tool,
                     "arguments": json.loads(args.arguments)}, 2)
    else:
        result = rpc("tools/list", {}, 2)
    if args.output:
        Path(args.output).write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
        print("Result saved to " + args.output)
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    if isinstance(result, dict) and result.get("isError"):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
