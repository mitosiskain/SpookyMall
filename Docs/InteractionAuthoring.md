# Blueprint interaction

## Validation status: runtime smoke test failed

Blueprint conversion compiled and saved without errors or warnings, and the project build succeeded after retiring the native interaction classes. However, the single PIE smoke test failed: pressing E while aiming at the existing cube reported `No interactable in range`. The cube's `CanInteract` returned true and an equivalent camera collision trace hit the cube, but the component's `FindInteractable` returned failure. The precise cause remains unresolved. The runtime interaction is therefore not validated or ready to use.

PIE was stopped after that one session, with no further correction or test performed. Diagnostic evidence is in `Saved/Logs/InteractionSmoke.log`. The authoring instructions below describe the converted assets; successful runtime use depends on resolving this failure.

The interaction interface and tracing component are ordinary Blueprint assets. No custom C++ interaction classes are required.

## Assets

- `/Game/Game/Blueprints/Interaction/BPI_Interactable`: `Interact(Interactor)` and `CanInteract(Interactor)`.
- `/Game/Game/Blueprints/Interaction/BP_InteractionComponent`: commented `FindInteractable` and `TryInteract` graphs. Parent: Unreal `ActorComponent`.
- `/Game/Game/Blueprints/Interaction/BP_TestEvidence`: existing test cube. Open Event Graph and find **INTERACTION RESPONSE - Edit what this cube does here**. Its existing `evidence found cold spot` message is preserved. The commented `CanInteract` function returns true.
- `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`: existing `Interaction` component now uses the Blueprint component. Existing `IA_Interact` Started input remains connected to `TryInteract`.

## Create another interactable

1. Create a Blueprint Actor and add the desired mesh/collision.
2. In Class Settings, add **BPI_Interactable** under Implemented Interfaces.
3. Implement **CanInteract** and return true, or a Blueprint condition.
4. Add **Event Interact** and connect the actor's response in its Event Graph. `Interactor` is the player pawn.
5. Ensure collision blocks **Visibility**, place the actor, aim the center of the camera at it, and press **E**.

The target actor does not need an Interaction Component. The initiating player owns that component.

## Settings and presentation

The player's component preserves its existing 1,000 cm camera trace distance and Visibility channel. These are editable in the component's Details panel. The first blocking hit must implement the interface and allow interaction. This uses the owning player's camera and does not require Tick.

`FindInteractable` returns an actor and success flag; `TryInteract` returns whether it dispatched interaction. Future Blueprint/UMG presentation can consume these. Disable **Show Debug Messages** to hide the component's temporary Print Strings; the cube's own response is edited separately in its graph. No final UI or dependent gameplay systems were added.

Original assets and native files are retained under `Saved/InteractionAuthoring/Before` and `Saved/InteractionAuthoring/RetiredNativeInteraction`. The one-off editor authoring helper is archived under `Saved/InteractionAuthoring/AuthoringPlugin`; it is not an active plugin or a gameplay dependency.
