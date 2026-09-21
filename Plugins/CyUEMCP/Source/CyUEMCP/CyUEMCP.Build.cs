using UnrealBuildTool;
public class CyUEMCP : ModuleRules {
 public CyUEMCP(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core"});
  PrivateDependencyModuleNames.AddRange(new string[]{"CoreUObject","Engine","UnrealEd","Json","CyUEMCPCore","CyUEMCPBridge","CyUEAssistant"});
 }
}
