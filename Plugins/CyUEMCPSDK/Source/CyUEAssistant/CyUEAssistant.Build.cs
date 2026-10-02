using UnrealBuildTool;
using System.IO;
public class CyUEAssistant : ModuleRules {
 public CyUEAssistant(ReadOnlyTargetRules Target) : base(Target) {
  string RuntimeResources = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../Resources/AssistantRuntime"));
  if (Directory.Exists(RuntimeResources)) {
   foreach (string File in Directory.GetFiles(RuntimeResources)) RuntimeDependencies.Add(File, StagedFileType.NonUFS);
  }
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core"});
  PrivateDependencyModuleNames.AddRange(new string[]{"CoreUObject","Engine","UnrealEd","Slate","SlateCore","ToolMenus","HTTP","Json","WebBrowser","Projects","DesktopPlatform","ApplicationCore","InputCore","AssetRegistry","CyUEMCPCore","CyUEMCPBridge","CyUELocalInference"});
 }
}
