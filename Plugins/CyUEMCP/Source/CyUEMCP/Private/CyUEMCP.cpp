#include "CyUERegistry.h"
#include "CyUEBridge.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
class FCyUEMCPModule : public IModuleInterface {
public:
 void StartupModule() override {
  auto& Registry=FCyUEMCPCoreModule::Get().Registry;
  auto Add=[&](const FString& Id,const FString& Name,const FString& Description,const FString& Schema,bool Writes,FCyUEHandler Handler){
   auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("id"),Id);D->SetStringField(TEXT("name"),Name);D->SetStringField(TEXT("description"),Description);D->SetObjectField(TEXT("inputSchema"),CyUEJson::Parse(Schema));D->SetObjectField(TEXT("outputSchema"),CyUEJson::Parse(TEXT("{\"type\":\"object\"}")));
   FString OutputSchema=TEXT("{\"type\":\"object\"}");
   if(Id==TEXT("ue_get_context"))OutputSchema=TEXT("{\"type\":\"object\",\"properties\":{\"engineVersion\":{\"type\":\"string\"},\"project\":{\"type\":\"string\"},\"map\":{\"type\":\"string\"}},\"required\":[\"engineVersion\",\"project\",\"map\"],\"additionalProperties\":false}");
   if(Id==TEXT("ue_list_selection"))OutputSchema=TEXT("{\"type\":\"object\",\"properties\":{\"actors\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"label\":{\"type\":\"string\"},\"class\":{\"type\":\"string\"}},\"required\":[\"path\",\"label\",\"class\"]}}},\"required\":[\"actors\"]}");
   if(Id==TEXT("ue_set_actor_label"))OutputSchema=TEXT("{\"type\":\"object\",\"properties\":{\"actorPath\":{\"type\":\"string\"},\"label\":{\"type\":\"string\"},\"saved\":{\"type\":\"boolean\"}},\"required\":[\"actorPath\",\"label\",\"saved\"]}");
   if(Id==TEXT("ue_export_selection"))OutputSchema=TEXT("{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"x-cy-kind\":\"File\"}},\"required\":[\"path\"]}");
   D->SetObjectField(TEXT("outputSchema"),CyUEJson::Parse(OutputSchema));
   auto Effects=MakeShared<FJsonObject>();Effects->SetBoolField(TEXT("modifiesProject"),Writes);Effects->SetBoolField(TEXT("undoSupported"),Writes);Effects->SetBoolField(TEXT("savesToDisk"),false);D->SetObjectField(TEXT("x-cy-unreal"),Effects);
   FString Error;if(!Registry.Register(TEXT("CyUEMCP"),D,Writes,MoveTemp(Handler),Error))UE_LOG(LogTemp,Error,TEXT("CyUEMCP: %s"),*Error);
  };
  Add(TEXT("ue_get_context"),TEXT("Editor context"),TEXT("Read engine version, project and current editor map."),TEXT("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"),false,[](const FCyUEContext&,const TSharedRef<FJsonObject>&){
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("engineVersion"),FEngineVersion::Current().ToString());R->SetStringField(TEXT("project"),FPaths::GetProjectFilePath());auto* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;R->SetStringField(TEXT("map"),W?W->GetPathName():TEXT(""));return FCyUEResult::Ok(R);
  });
  Add(TEXT("ue_list_selection"),TEXT("Selected actors"),TEXT("Read actor references, labels and classes in the editor selection."),TEXT("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"),false,[](const FCyUEContext&,const TSharedRef<FJsonObject>&){
   auto R=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Items;if(GEditor)for(FSelectionIterator It(*GEditor->GetSelectedActors());It;++It)if(auto* A=Cast<AActor>(*It)){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),A->GetPathName());O->SetStringField(TEXT("label"),A->GetActorLabel());O->SetStringField(TEXT("class"),A->GetClass()->GetPathName());Items.Add(MakeShared<FJsonValueObject>(O));}R->SetArrayField(TEXT("actors"),Items);return FCyUEResult::Ok(R);
  });
  Add(TEXT("ue_set_actor_label"),TEXT("Rename selected actor"),TEXT("Set one selected actor label. Undo transaction; does not save the map."),TEXT("{\"type\":\"object\",\"properties\":{\"actorPath\":{\"type\":\"string\",\"minLength\":1},\"label\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":256}},\"required\":[\"actorPath\",\"label\"],\"additionalProperties\":false}"),true,[](const FCyUEContext&,const TSharedRef<FJsonObject>& P){
   FString Path,Label;if(!P->TryGetStringField(TEXT("actorPath"),Path)||!P->TryGetStringField(TEXT("label"),Label)||Label.IsEmpty()||Label.Len()>256)return FCyUEResult::Error(TEXT("InvalidParameters"),TEXT("Actor path and label required"));
   if(GEditor)for(FSelectionIterator It(*GEditor->GetSelectedActors());It;++It)if(auto* A=Cast<AActor>(*It))if(A->GetPathName()==Path){FScopedTransaction Tx(NSLOCTEXT("CyUEMCP","Rename","Cy MCP: rename actor"));A->Modify();A->SetActorLabel(Label);auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actorPath"),Path);R->SetStringField(TEXT("label"),A->GetActorLabel());R->SetBoolField(TEXT("saved"),false);return FCyUEResult::Ok(R);}
   return FCyUEResult::Error(TEXT("AccessDenied"),TEXT("Actor must be selected in the editor"));
  });
  Add(TEXT("ue_export_selection"),TEXT("Export selection metadata"),TEXT("Write selection JSON into the task workspace; export it before FinishTask to keep it."),TEXT("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"),false,[](const FCyUEContext& C,const TSharedRef<FJsonObject>& P){
   auto Selection=FCyUEMCPCoreModule::Get().Registry.Execute(TEXT("ue_list_selection"),C,P,false);if(!Selection.Value)return Selection;
   FString Path;if(!C.OutputPath(TEXT("selection.json"),Path)||!FFileHelper::SaveStringToFile(CyUEJson::Stringify(Selection.Value.ToSharedRef()),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return FCyUEResult::Error(TEXT("OutputError"),TEXT("Cannot write task-owned selection export"));
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("path"),Path);return FCyUEResult::Ok(R);
  });
  FCyUEBridgeModule::Get().Start();
  if(!IsRunningCommandlet())FModuleManager::LoadModuleChecked<IModuleInterface>("CyUEAssistant");
 }
 void ShutdownModule() override {if(FModuleManager::Get().IsModuleLoaded("CyUEMCPBridge"))FCyUEBridgeModule::Get().Stop();if(FModuleManager::Get().IsModuleLoaded("CyUEMCPCore"))FCyUEMCPCoreModule::Get().Registry.UnregisterOwner(TEXT("CyUEMCP"));}
};
IMPLEMENT_MODULE(FCyUEMCPModule,CyUEMCP)
