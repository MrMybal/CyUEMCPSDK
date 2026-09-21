#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
struct CYUEMCPCORE_API FCyUEResult {
 TSharedPtr<FJsonObject> Value; FString Code, Message;
 static FCyUEResult Ok(TSharedPtr<FJsonObject> In) { return {In,TEXT(""),TEXT("")}; }
 static FCyUEResult Error(const FString& C,const FString& M) { return {nullptr,C,M}; }
};
struct CYUEMCPCORE_API FCyUEContext {
 FString JobId, Workspace;
 bool OutputPath(const FString& Relative,FString& Out) const;
};
using FCyUEHandler=TFunction<FCyUEResult(const FCyUEContext&,const TSharedRef<FJsonObject>&)>;
struct CYUEMCPCORE_API FCyUEOperation {
 FName Owner; TSharedPtr<FJsonObject> Descriptor; bool bMutates=false; FCyUEHandler Handler;
};
/** Game-thread registry. Registration is owned by a plugin; shutdown unregisters it. */
class CYUEMCPCORE_API FCyUERegistry {
 TMap<FString,FCyUEOperation> Operations;
public:
 bool Register(FName Owner,const TSharedRef<FJsonObject>& Descriptor,bool bMutates,FCyUEHandler Handler,FString& Error);
 void UnregisterOwner(FName Owner);
 TArray<TSharedPtr<FJsonValue>> Describe() const;
 FCyUEResult Execute(const FString& Id,const FCyUEContext& Context,const TSharedRef<FJsonObject>& Parameters,bool bAllowWrites);
};
class CYUEMCPCORE_API FCyUEMCPCoreModule : public IModuleInterface {
public:
 FCyUERegistry Registry;
 static FCyUEMCPCoreModule& Get() {return FModuleManager::LoadModuleChecked<FCyUEMCPCoreModule>("CyUEMCPCore");}
};
namespace CyUEJson {
 CYUEMCPCORE_API TSharedPtr<FJsonObject> Parse(const FString& Text);
 CYUEMCPCORE_API FString Stringify(const TSharedRef<FJsonObject>& Object);
}
