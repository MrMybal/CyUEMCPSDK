# Integrated connector conventions

CyAIConnectorLab was used as a local implementation example. The provider code is
now adapted and maintained inside this SDK, in cyue_mcp.providers. No package,
source directory, service or application from ConnectorLab is required.

Keep detect/connect/start_session/resume_session/send/cancel/disconnect/list_models
and get_capabilities/get_status as the common contract. Events are versioned;
TextDelta, snapshots and terminal results are distinct. One active turn per instance.
Use exact native session IDs and never silently retry a submitted turn. Keep process
arguments shell-free, credentials redacted, stdout structured and owned processes
closed. Unknown capabilities and unimplemented transports must stay explicit.

CyToolsCore remains the tool/job/MCP runtime. Provider CLIs (Codex, Claude, etc.)
and a local inference executable are needed only for the selected backend.
