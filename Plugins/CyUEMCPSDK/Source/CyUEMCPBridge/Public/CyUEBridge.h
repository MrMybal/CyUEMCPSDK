#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HttpRouteHandle.h"
class IHttpRouter;
class CYUEMCPBRIDGE_API FCyUEBridgeModule : public IModuleInterface {
 TSharedPtr<IHttpRouter> Router;
 FHttpRouteHandle Route;
 FString Token,ConnectionFile;
 TMap<FString,TSharedPtr<FJsonObject>> Receipts;
public:
 bool bAllowWrites=false;
 bool Start();
 void Stop();
 virtual void ShutdownModule() override { Stop(); }
 const FString& GetConnectionFile() const {return ConnectionFile;}
 static FCyUEBridgeModule& Get(){return FModuleManager::LoadModuleChecked<FCyUEBridgeModule>("CyUEMCPBridge");}
};
