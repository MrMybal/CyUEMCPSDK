using UnrealBuildTool;
public class CyUEMCPBridge : ModuleRules {
 public CyUEMCPBridge(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","Json","CyUEMCPCore","HTTPServer"});
  PrivateDependencyModuleNames.AddRange(new string[]{"HTTPServer","Sockets","Networking","Projects","CoreUObject","Engine","UnrealEd"});
 }
}
