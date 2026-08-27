// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Calling : ModuleRules
{
	public Calling(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"NetCore",
			"AIModule",
			"NavigationSystem",
			"Json",
			"JsonUtilities",
			"Sockets",
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"HTTPServer"
		});
	}
}