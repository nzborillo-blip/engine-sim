# Engine parameter editor - core model

This directory contains the first implementation layer for the desktop engine editor.

## Current scope

- A typed, versioned configuration model for engine-level values and connected components.
- Component types matching the original simulator script model: crankshaft, cylinder bank, piston, connecting rod, cylinder head, camshaft, fuel, intake, exhaust, ignition module, throttle and cylinder.
- Validation for engine-level values, basic geometry and mass values, unique component IDs, and component references.
- Deterministic text-based .esconfig save/load support.
- Loading is transactional: the destination configuration is replaced only after parsing and validation succeed.
- A default starter configuration with one crankshaft and one cylinder bank.

## Deliberate separation

The model is not yet connected to the running simulation or to a Windows editor window. It does not apply changes to an active engine. Next integration stages:

1. Map this config graph to the original simulator's Piranha script nodes.
2. Compile and initialize a replacement engine before swapping it into the application.
3. Add the separate Windows editor window and bind its controls to a draft EngineConfig.
4. Connect the MODYFIKUJ SILNIK action to validation and transactional replacement.

The active simulator must remain unchanged when validation, script compilation or replacement initialization fails.
