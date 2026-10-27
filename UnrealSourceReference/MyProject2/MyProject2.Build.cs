using UnrealBuildTool;
using System.IO;

public class MyProject2 : ModuleRules
{
	public MyProject2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OnlineSubsystem", "ProceduralMeshComponent"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Voice", "SignalProcessing", "Json", "DeveloperSettings", "AIModule",
			"NavigationSystem", "GameplayTasks", "StateTreeModule", "GameplayStateTreeModule", "PropertyBindingUtils",
			"UMG", "Slate", "SlateCore", "OnlineSubsystemUtils", "EngineSettings"
		});
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd", "BlueprintGraph", "KismetCompiler", "StateTreeEditorModule", "PropertyBindingUtilsEditor"
			});
		}

		// Speech runtimes load dynamically; stage DLLs and models as loose files.
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../.."));
		PrivateIncludePaths.Add(Path.Combine(ProjectRoot, "ThirdParty", "Whisper", "include"));
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			foreach (string SpeechRuntime in new string[] { "Vosk", "Whisper" })
			{
				string RuntimeRoot = Path.Combine(ProjectRoot, "ThirdParty", SpeechRuntime, "Win64");
				if (Directory.Exists(RuntimeRoot))
				{
					foreach (string File in Directory.GetFiles(RuntimeRoot, "*.dll"))
					{
						RuntimeDependencies.Add(File, StagedFileType.NonUFS);
					}
				}
				string NoticesRoot = Path.Combine(ProjectRoot, "ThirdParty", SpeechRuntime);
				if (Directory.Exists(NoticesRoot))
				{
					foreach (string File in Directory.GetFiles(NoticesRoot, "*.txt"))
					{
						RuntimeDependencies.Add(File, StagedFileType.NonUFS);
					}
				}
			}
		}
		// Keep .bin models outside Content, where Unreal's asset importers scan.
		string WhisperModelsRoot = Path.Combine(ProjectRoot, "ThirdParty", "Whisper", "Models");
		if (Directory.Exists(WhisperModelsRoot))
		{
			foreach (string File in Directory.GetFiles(WhisperModelsRoot, "*.bin"))
			{
				RuntimeDependencies.Add(File, StagedFileType.NonUFS);
			}
		}
		string ModelsRoot = Path.Combine(ProjectRoot, "Content", "VoiceModels");
		if (Directory.Exists(ModelsRoot))
		{
			foreach (string File in Directory.GetFiles(ModelsRoot, "*", SearchOption.AllDirectories))
			{
				RuntimeDependencies.Add(File, StagedFileType.NonUFS);
			}
		}
	}
}
