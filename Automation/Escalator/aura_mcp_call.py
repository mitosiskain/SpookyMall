"""Use a fresh, configured Aura MCP process when the host connection has closed.

This uses the MCP protocol and the installed Aura server unchanged, including
its authentication and approval handling. It does not directly call UE endpoints.
Pass a JSON file with {"tool": ..., "arguments": ...}.
"""
import argparse
import asyncio
import json
import os
from pathlib import Path
import sys
import tomllib

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


async def invoke(server_name, request_path, output_path):
    config_path = Path(os.environ.get("CODEX_HOME", str(Path.home() / ".codex"))) / "config.toml"
    config = tomllib.loads(config_path.read_text(encoding="utf-8"))
    server = config["mcp_servers"][server_name]
    request = json.loads(Path(request_path).read_text(encoding="utf-8-sig"))
    parameters = StdioServerParameters(
        command=server["command"], args=server.get("args", []),
        env=server.get("env"), cwd=server.get("cwd"),
    )
    with open(os.devnull, "w") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                payloads = []
                for call in request.get("calls", [request]):
                    result = await session.call_tool(call["tool"], call.get("arguments", {}))
                    payload = result.model_dump(mode="json", exclude_none=True)
                    payloads.append({"tool": call["tool"], "result": payload})
                    if output_path:
                        Path(output_path).write_text(json.dumps(payloads, indent=2), encoding="utf-8")
                    print("MCP_TOOL " + call["tool"], flush=True)
                    for content in payload.get("content", []):
                        if content.get("type") == "text":
                            if call["tool"] == "run_verification_sequence":
                                try:
                                    sequence = json.loads(content["text"])
                                    summary = []
                                    for step in sequence.get("results", []):
                                        data = step.get("result", {}).get("data", {})
                                        summary.append({"index":step.get("index"), "type":step.get("type"),
                                            "ok":step.get("ok"), "action_success":step.get("result", {}).get("success"), "error":step.get("error") or step.get("result", {}).get("error"),
                                            "time":step.get("result", {}).get("pie_time_seconds"),
                                            "data":{key:value for key,value in data.items()
                                                if key not in ("rows", "camera_location", "camera_rotation", "forward", "scale")}})
                                    print(json.dumps({"success":sequence.get("success"), "steps":summary,
                                        "screenshot_paths":sequence.get("screenshot_paths")}), flush=True)
                                except (ValueError, TypeError, AttributeError):
                                    print(content["text"], flush=True)
                            else:
                                print(content["text"], flush=True)
                        else:
                            print(json.dumps({"content_type": content.get("type")}), flush=True)
                    if result.isError:
                        raise RuntimeError("Aura MCP returned a tool error")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("server", choices=["unreal_inspector", "unreal_editor"])
    parser.add_argument("request")
    parser.add_argument("--output")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    asyncio.run(asyncio.wait_for(invoke(args.server, args.request, args.output), timeout=args.timeout))
