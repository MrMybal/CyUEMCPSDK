using UnrealBuildTool;
public class CyUEAssistant : ModuleRules {
 public CyUEAssistant(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core"});
  PrivateDependencyModuleNames.AddRange(new string[]{"CoreUObject","Engine","UnrealEd","Slate","SlateCore","ToolMenus","HTTP","Json","WebBrowser","Projects","DesktopPlatform","ApplicationCore","InputCore","CyUEMCPCore","CyUEMCPBridge","CyUELocalInference"});
 }
}
