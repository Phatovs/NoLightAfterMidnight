// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NoLightAfterMidnight : ModuleRules
{
	public NoLightAfterMidnight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"NoLightAfterMidnight",
			"NoLightAfterMidnight/Variant_Horror",
			"NoLightAfterMidnight/Variant_Horror/UI",
			"NoLightAfterMidnight/Variant_Shooter",
			"NoLightAfterMidnight/Variant_Shooter/AI",
			"NoLightAfterMidnight/Variant_Shooter/UI",
			"NoLightAfterMidnight/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
