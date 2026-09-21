#include "CyUERegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION != 5 || ENGINE_MINOR_VERSION < 3 || ENGINE_MINOR_VERSION > 8
#error CyUEMCPSDK supports UE 5.3 through 5.8; validate newer engines before changing this guard.
#endif
IMPLEMENT_MODULE(FCyUEMCPCoreModule,CyUEMCPCore)
TSharedPtr<FJsonObject> CyUEJson::Parse(const FString& Text){TSharedPtr<FJsonObject> O;auto R=TJsonReaderFactory<>::Create(Text);FJsonSerializer::Deserialize(R,O);return O;}
FString CyUEJson::Stringify(const TSharedRef<FJsonObject>& O){FString S;auto W=TJsonWriterFactory<>::Create(&S);FJsonSerializer::Serialize(O,W);return S;}
bool FCyUEContext::OutputPath(const FString& Relative,FString& Out) const {
 if(Workspace.IsEmpty()||!FPaths::IsRelative(Relative)||Relative.Contains(TEXT(".."))||Relative.Contains(TEXT(":")))return false;
 FString Root=FPaths::ConvertRelativePathToFull(Workspace);FPaths::NormalizeDirectoryName(Root);
 Out=FPaths::ConvertRelativePathToFull(Root/Relative);FPaths::NormalizeFilename(Out);
 if(!Out.StartsWith(Root+TEXT("/")))return false;
 return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out),true);
}
bool FCyUERegistry::Register(FName Owner,const TSharedRef<FJsonObject>& D,bool Mutates,FCyUEHandler Handler,FString& Error){
 check(IsInGameThread());FString Id;
 if(Owner.IsNone()||!D->TryGetStringField(TEXT("id"),Id)||Id.IsEmpty()||Operations.Contains(Id)||!D->HasTypedField<EJson::Object>(TEXT("inputSchema"))||!D->HasTypedField<EJson::Object>(TEXT("outputSchema"))||!Handler){Error=TEXT("Invalid or duplicate CyTools operation");return false;}
 // A native delegate is synchronous on the game thread. Never advertise mid-handler cancellation.
 D->SetBoolField(TEXT("canCancel"),false);D->SetBoolField(TEXT("canRunAsync"),true);D->SetBoolField(TEXT("supportsProgress"),false);
 D->SetArrayField(TEXT("supportedBackends"),{});
 TArray<TSharedPtr<FJsonValue>> Permissions;const TArray<TSharedPtr<FJsonValue>>* Declared=nullptr;if(D->TryGetArrayField(TEXT("permissions"),Declared))Permissions=*Declared;Permissions.Add(MakeShared<FJsonValueString>(Mutates?TEXT("ue.write"):TEXT("ue.read")));D->SetArrayField(TEXT("permissions"),Permissions);
 Operations.Add(Id,{Owner,D,Mutates,MoveTemp(Handler)});return true;
}
void FCyUERegistry::UnregisterOwner(FName Owner){check(IsInGameThread());for(auto It=Operations.CreateIterator();It;++It)if(It.Value().Owner==Owner)It.RemoveCurrent();}
TArray<TSharedPtr<FJsonValue>> FCyUERegistry::Describe() const {check(IsInGameThread());TArray<TSharedPtr<FJsonValue>> A;TArray<FString> Keys;Operations.GetKeys(Keys);Keys.Sort();for(auto& K:Keys)A.Add(MakeShared<FJsonValueObject>(Operations[K].Descriptor));return A;}
FCyUEResult FCyUERegistry::Execute(const FString& Id,const FCyUEContext& C,const TSharedRef<FJsonObject>& P,bool Allow){
 check(IsInGameThread());auto* O=Operations.Find(Id);if(!O)return FCyUEResult::Error(TEXT("OperationUnavailable"),TEXT("Plugin operation is no longer registered; reconnect the bridge."));
 if(O->bMutates&&!Allow)return FCyUEResult::Error(TEXT("AccessDenied"),TEXT("Enable editor modifications in the Cy MCP panel."));
 return O->Handler(C,P);
}
