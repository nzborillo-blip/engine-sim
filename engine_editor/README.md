# Engine parameter editor

This directory contains the configuration model and the separate Windows editor window.

## Implemented

- A typed, versioned configuration model for engine-level values and connected components.
- Validation for engine-level values, basic geometry and mass values, unique component IDs, and component references.
- Deterministic text-based `.esconfig` save/load support.
- Transactional loading: the destination configuration is replaced only after parsing and validation succeed.
- Strict parsing rejects malformed records and any data after the final `END` record.
- A separate native Windows editor window with ten engine-level controls, sliders and numeric fields.
- Draft editing: moving controls does not change the running engine until `MODYFIKUJ SILNIK` is pressed.
- The editor is initialized by `EngineSimApplication` and pumps its own Windows messages while the simulator runs.
- Applying supported scalar values updates the active engine, simulator frequency, starter motor, dynamometer limits and synthesizer noise parameters.
- The editor can reload the active values and save/load `.esconfig` files.

## Current limitations

- Live application currently supports engine-level scalar parameters only.
- Changes to component geometry, component count, cylinder-bank layout, crankshaft layout and component connections are rejected rather than silently reported as applied.
- The configuration model can represent component graphs, but a complete conversion from that graph into the simulator's Piranha script nodes and a safe full-engine rebuild are not implemented yet.

## Next integration stage

Implement a validated conversion from `EngineConfig` to the simulator's Piranha node graph, build and initialize a replacement engine without touching the active one, then swap it into the application only after every step succeeds. If parsing, compilation or initialization fails, the current engine must keep running.
