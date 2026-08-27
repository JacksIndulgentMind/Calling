# Start PIE after UnrealEditor has loaded the Calling project.
import unreal

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
        les.editor_request_play_session()
        unreal.log("Calling: requested PIE session")
    else:
        unreal.log_error("Calling: no LevelEditorSubsystem")
except Exception as err:
    unreal.log_error(f"Calling PIE request failed: {err}")
