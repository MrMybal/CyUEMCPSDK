using UnrealBuildTool;
public class CyUEMCPCore : ModuleRules {
 public CyUEMCPCore(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","Json"});
  PrivateDependencyModuleNames.AddRange(new string[]{});
 }
}
