"""
Headless map bootstrap for Calling scenes.
Run via UnrealEditor-Cmd -ExecutePythonScript=...
"""

import unreal

MAPS = [
    ("/Game/Maps/CL_Boot", "/Script/Calling.CLBootGameMode"),
    ("/Game/Maps/CL_Social", "/Script/Calling.CLSocialGameMode"),
    ("/Game/Maps/CL_PvpArena", "/Script/Calling.CLPvpGameMode"),
    ("/Game/Maps/CL_Raid_01", "/Script/Calling.CLRaidGameMode"),
    ("/Game/Maps/CL_Raid_02", "/Script/Calling.CLRaidGameMode"),
    ("/Game/Maps/CL_Raid_03", "/Script/Calling.CLRaidGameMode"),
    ("/Game/Maps/CL_Raid_04", "/Script/Calling.CLRaidGameMode"),
    ("/Game/Maps/CL_Practice", "/Script/Calling.CLPracticeGameMode"),
]


def ensure_directory(package_path: str) -> None:
    # package_path like /Game/Maps/CL_Boot -> /Game/Maps
    slash = package_path.rfind("/")
    folder = package_path[:slash] if slash > 0 else "/Game"
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)


def create_or_update_level(asset_path: str, game_mode_path: str) -> None:
    ensure_directory(asset_path)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log("existing: {}".format(asset_path))
        # Still open to apply GameMode override if missing.
        if not unreal.EditorLevelLibrary.load_level(asset_path):
            unreal.log_warning("failed to load existing level {}".format(asset_path))
            return
    else:
        # new_level creates + saves a blank level at the given path
        if not unreal.EditorLevelLibrary.new_level(asset_path):
            unreal.log_error("failed to create level {}".format(asset_path))
            return
        unreal.log("created: {}".format(asset_path))

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        unreal.log_error("no editor world after creating {}".format(asset_path))
        return

    game_mode_class = unreal.load_class(None, game_mode_path)
    if not game_mode_class:
        # Soft path fallback used by some UE versions
        game_mode_class = unreal.load_object(None, game_mode_path)

    if game_mode_class:
        world_settings = world.get_world_settings()
        if world_settings:
            world_settings.set_editor_property("default_game_mode", game_mode_class)
            unreal.log("set GameModeOverride {} -> {}".format(asset_path, game_mode_path))
        else:
            unreal.log_warning("no world settings for {}".format(asset_path))
    else:
        unreal.log_warning("could not load GameMode class {}".format(game_mode_path))

    unreal.EditorLevelLibrary.save_current_level()


def main() -> None:
    for asset_path, game_mode in MAPS:
        create_or_update_level(asset_path, game_mode)
    unreal.log("Calling map bootstrap complete.")


if __name__ == "__main__":
    main()
