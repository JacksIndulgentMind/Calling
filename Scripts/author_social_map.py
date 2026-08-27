"""Author a walkable 100x100 m pad, lights, and PlayerStart into CL_Social."""

import unreal


MAP_PATH = "/Game/Maps/CL_Social"


def _spawn(actor_subsystem, cls, location, rotation=None):
    if rotation is None:
        rotation = unreal.Rotator(0.0, 0.0, 0.0)
    return actor_subsystem.spawn_actor_from_class(cls, location, rotation)


def _has_label(actors, label):
    for actor in actors:
        if actor and actor.get_actor_label() == label:
            return True
    return False


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_editor.load_level(MAP_PATH)

    existing = actor_subsystem.get_all_level_actors()
    if _has_label(existing, "SocialPad") and _has_label(existing, "PlayerStart"):
        unreal.log("CL_Social already has SocialPad and PlayerStart; skipping.")
        return

    if not _has_label(existing, "PlayerStart"):
        player_start = _spawn(actor_subsystem, unreal.PlayerStart, unreal.Vector(0.0, 0.0, 200.0))
        player_start.set_actor_label("PlayerStart")

    if not _has_label(existing, "Sun"):
        sun = _spawn(
            actor_subsystem,
            unreal.DirectionalLight,
            unreal.Vector(0.0, 0.0, 500.0),
            unreal.Rotator(-48.0, 40.0, 0.0),
        )
        sun.set_actor_label("Sun")
        light = sun.light_component
        light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        light.set_intensity(8.0)
        light.set_atmosphere_sun_light(True)

    if not _has_label(existing, "SkyLight"):
        sky = _spawn(actor_subsystem, unreal.SkyLight, unreal.Vector(0.0, 0.0, 400.0))
        sky.set_actor_label("SkyLight")
        sky_light = sky.light_component
        sky_light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        sky_light.set_intensity(1.0)
        sky_light.set_editor_property("real_time_capture", True)

    if not _has_label(existing, "SkyAtmosphere"):
        atmosphere = _spawn(actor_subsystem, unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0))
        atmosphere.set_actor_label("SkyAtmosphere")

    if not _has_label(existing, "SocialPad"):
        cube = _spawn(actor_subsystem, unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, -20.0))
        cube.set_actor_label("SocialPad")
        mesh_comp = cube.static_mesh_component
        mesh_comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
        if mesh:
            mesh_comp.set_static_mesh(mesh)
        grid = unreal.EditorAssetLibrary.load_asset(
            "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
        )
        if grid:
            mesh_comp.set_material(0, grid)
        # Engine cube is 100 cm; 100 x 100 x 0.4 -> 100 m x 100 m x 40 cm, top at Z=0.
        cube.set_actor_scale3d(unreal.Vector(100.0, 100.0, 0.4))

    if not level_editor.save_current_level():
        unreal.log_error("Failed to save CL_Social.")
        raise SystemExit(1)
    unreal.log("Saved CL_Social with pad, lights, and PlayerStart.")


if __name__ == "__main__":
    main()
