using UnrealBuildTool;
public class CyUELocalInference : ModuleRules {
 public CyUELocalInference(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","Json"});
  PrivateDependencyModuleNames.AddRange(new string[]{});
 }
}
