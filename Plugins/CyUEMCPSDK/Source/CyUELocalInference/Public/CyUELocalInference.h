#pragma once
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
/** Optional owned llama.cpp server. Existing external models are never copied or deleted. */
class CYUELOCALINFERENCE_API FCyUELocalInferenceModule : public IModuleInterface
{
    FProcHandle Process;
    FString ConnectionFile, Secret, PendingLog;
    FTSTicker::FDelegateHandle LogTicker;
    void *LogRead = nullptr;
    void *LogWrite = nullptr;

  public:
    bool Start(const FString &Server, const FString &Model, int32 Port, int32 GPULayers, FString &Error);
    void Stop();
    bool IsRunning();
    FString ReadLog();
    virtual void StartupModule() override;
    virtual void ShutdownModule() override
    {
        FTSTicker::GetCoreTicker().RemoveTicker(LogTicker);
        Stop();
    }
};
