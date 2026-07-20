// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MYPROJ2 : ModuleRules
{
	public MYPROJ2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MYPROJ2",
			"MYPROJ2/Variant_Strategy",
			"MYPROJ2/Variant_Strategy/UI",
			"MYPROJ2/Variant_TwinStick",
			"MYPROJ2/Variant_TwinStick/AI",
			"MYPROJ2/Variant_TwinStick/Gameplay",
			"MYPROJ2/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
