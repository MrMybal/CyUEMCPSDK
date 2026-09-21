#include "CyUEBridge.h"
#include "CyUERegistry.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Runtime/Launch/Resources/Version.h"
IMPLEMENT_MODULE(FCyUEBridgeModule,CyUEMCPBridge)
namespace {
TSharedRef<FJsonObject> Failure(const FString& Code,const FString& Message){auto O=MakeShared<FJsonObject>();auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("code"),Code);E->SetStringField(TEXT("message"),Message);O->SetObjectField(TEXT("error"),E);return O;}
FString Header(const FHttpServerRequest& R,const FString& Name){for(auto& P:R.Headers)if(P.Key.Equals(Name,ESearchCase::IgnoreCase)&&P.Value.Num())return P.Value[0];return TEXT("");}
}
bool FCyUEBridgeModule::Start(){
 check(IsInGameThread());if(Route.IsValid())return true;
 Token=FGuid::NewGuid().ToString(EGuidFormats::Digits)+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 uint32 Port=0;
 for(uint32 Candidate=39000+FPlatformProcess::GetCurrentProcessId()%1000;Candidate<41000;++Candidate){
  TArray<FString> Overrides;GConfig->GetArray(TEXT("HTTPServer.Listeners"),TEXT("ListenerOverrides"),Overrides,GEngineIni);
  const FString Entry=FString::Printf(TEXT("(Port=%u,BindAddress=127.0.0.1)"),Candidate);Overrides.Insert(Entry,0);
  GConfig->SetArray(TEXT("HTTPServer.Listeners"),TEXT("ListenerOverrides"),Overrides,GEngineIni);
  Router=FHttpServerModule::Get().GetHttpRouter(Candidate,true);
  Overrides.RemoveAt(0);GConfig->SetArray(TEXT("HTTPServer.Listeners"),TEXT("ListenerOverrides"),Overrides,GEngineIni);
  if(Router){Port=Candidate;break;}
 }
 if(!Router)return false;
 auto Handler=[this](const FHttpServerRequest& R,const FHttpResultCallback& Reply){
  auto Send=[&](const TSharedRef<FJsonObject>& O,int Code=200){auto Response=FHttpServerResponse::Create(CyUEJson::Stringify(O),TEXT("application/json"));Response->Code=static_cast<EHttpServerResponseCodes>(Code);Reply(MoveTemp(Response));};
  if(!Header(R,TEXT("Origin")).IsEmpty()||Header(R,TEXT("Authorization"))!=TEXT("Bearer ")+Token){Send(Failure(TEXT("AccessDenied"),TEXT("Authenticated local companion required")),403);return true;}
  if(R.Body.Num()>1024*1024||R.Body.IsEmpty()){Send(Failure(TEXT("InvalidParameters"),TEXT("Request must be 1 byte to 1 MiB")),400);return true;}
  FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(R.Body.GetData()),R.Body.Num());auto Request=CyUEJson::Parse(FString(Converted.Length(),Converted.Get()));
  FString Method;if(!Request||!Request->TryGetStringField(TEXT("method"),Method)){Send(Failure(TEXT("InvalidParameters"),TEXT("method required")),400);return true;}
  auto Out=MakeShared<FJsonObject>();
  if(Method==TEXT("describe")){
   Out->SetNumberField(TEXT("schemaVersion"),1);Out->SetStringField(TEXT("protocolVersion"),TEXT("1.0"));Out->SetStringField(TEXT("id"),TEXT("CyUEMCP"));Out->SetStringField(TEXT("name"),TEXT("Cy Unreal MCP"));Out->SetStringField(TEXT("version"),TEXT("0.1.0"));Out->SetStringField(TEXT("description"),TEXT("Unreal Editor operations registered through CyUEMCPSDK"));
   Out->SetArrayField(TEXT("operations"),FCyUEMCPCoreModule::Get().Registry.Describe());
   auto Platform=MakeShared<FJsonObject>();Platform->SetStringField(TEXT("os"),TEXT("Windows"));Platform->SetStringField(TEXT("architecture"),TEXT("x64"));Platform->SetStringField(TEXT("status"),TEXT("Experimental"));Out->SetArrayField(TEXT("platforms"),{MakeShared<FJsonValueObject>(Platform)});
   auto Concurrency=MakeShared<FJsonObject>();Concurrency->SetStringField(TEXT("policy"),TEXT("Concurrent"));Concurrency->SetNumberField(TEXT("maxWorkers"),4);Out->SetObjectField(TEXT("concurrency"),Concurrency);
   Send(Out);return true;
  }
  FString CallId;if(!Request->TryGetStringField(TEXT("callId"),CallId)||CallId.IsEmpty()){Send(Failure(TEXT("InvalidParameters"),TEXT("callId required")),400);return true;}
  if(Method==TEXT("ack")){Receipts.Remove(CallId);Out->SetBoolField(TEXT("acknowledged"),true);Send(Out);return true;}
  if(auto* Previous=Receipts.Find(CallId)){Send(Previous->ToSharedRef());return true;}
  if(Method==TEXT("status")){Out->SetBoolField(TEXT("pending"),true);Send(Out);return true;}
  FString Operation,Workspace;const TSharedPtr<FJsonObject>* Parameters=nullptr;
  if(Method!=TEXT("invoke")||!Request->TryGetStringField(TEXT("operation"),Operation)||!Request->TryGetStringField(TEXT("workspace"),Workspace)||!Request->TryGetObjectField(TEXT("parameters"),Parameters)){Send(Failure(TEXT("InvalidParameters"),TEXT("invoke requires operation, workspace and parameters")),400);return true;}
  if(Receipts.Num()>=1024){Send(Failure(TEXT("Busy"),TEXT("Companion must acknowledge completed calls")),503);return true;}
  FCyUEContext Context{CallId,Workspace};auto Result=FCyUEMCPCoreModule::Get().Registry.Execute(Operation,Context,Parameters->ToSharedRef(),bAllowWrites);
  if(Result.Value)Out->SetObjectField(TEXT("result"),Result.Value);else Out=Failure(Result.Code,Result.Message);
  Receipts.Add(CallId,Out);Send(Out);return true;
 };
#if ENGINE_MINOR_VERSION >= 4
 Route=Router->BindRoute(FHttpPath(TEXT("/cyue/v1")),EHttpServerRequestVerbs::VERB_POST,FHttpRequestHandler::CreateLambda(MoveTemp(Handler)));
#else
 Route=Router->BindRoute(FHttpPath(TEXT("/cyue/v1")),EHttpServerRequestVerbs::VERB_POST,MoveTemp(Handler));
#endif
 if(!Route)return false;
 FHttpServerModule::Get().StartAllListeners();
 // Per-user runtime directory, never the project's repository. Do not log the token.
 ConnectionFile=FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("CyUEMCPSDK"),FString::Printf(TEXT("editor-%u.connection.json"),FPlatformProcess::GetCurrentProcessId()));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(ConnectionFile),true);
 auto Info=MakeShared<FJsonObject>();Info->SetStringField(TEXT("url"),FString::Printf(TEXT("http://127.0.0.1:%u/cyue/v1"),Port));Info->SetStringField(TEXT("token"),Token);Info->SetNumberField(TEXT("pid"),FPlatformProcess::GetCurrentProcessId());Info->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
 if(!FFileHelper::SaveStringToFile(CyUEJson::Stringify(Info),*ConnectionFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){Stop();return false;}
 return true;
}
void FCyUEBridgeModule::Stop(){if(Router&&Route){Router->UnbindRoute(Route);Route.Reset();}Router.Reset();if(!ConnectionFile.IsEmpty())IFileManager::Get().Delete(*ConnectionFile);Token.Empty();Receipts.Empty();}
