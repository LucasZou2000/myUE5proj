# Agent Collaboration Notes

## Division Of Work

- Agent owns C++ gameplay code, data structures, authority/replication contracts, automation tests, and Unreal Data Assets that can be created and verified reliably through MCP.
- Lucas owns short editor interactions that are faster and more reliable manually: Blueprint node wiring, UMG layout, viewport aiming, PIE console commands, and visual confirmation.
- UI is intentionally deferred when the milestone only requires runtime functionality. Agent should keep the runtime API UI-ready and provide the exact Blueprint/manual steps later.

## Verification Rules

- Do not spend several minutes attempting fragile Slate/MCP input automation for a task Lucas can perform in seconds.
- When a PIE or editor action is not reliable through available tools, report it as pending and provide a short copy-paste checklist.
- Distinguish clearly between `build passed`, `automation passed`, `asset saved/read back`, and `manual PIE accepted`; these are separate states.
- Never claim visual, Blueprint, network-topology, or interaction acceptance without observing the result in the editor.

## Handoff Format

- Record completed code/assets, exact automated results, and remaining manual checks in `Docs/chat.md`.
- Keep manual checklists short and command-oriented.
- Preserve user changes and do not reset unrelated worktree modifications.
