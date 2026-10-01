#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
class SWidget;

// Informational editor context, never an authorization or filesystem root.
struct FCyAssistantContext
{
    FString Id, Label, Plugin, Asset, File;
};
class CYUEASSISTANT_API ICyUEAssistantModule : public IModuleInterface
{
public:
    static ICyUEAssistantModule& Get()
    { return FModuleManager::LoadModuleChecked<ICyUEAssistantModule>(TEXT("CyUEAssistant")); }
    // Each returned widget owns independent chats. Keep it in the host's dock tab/toolkit.
    virtual TSharedRef<SWidget> CreateAssistant(const FCyAssistantContext& Context,
        TFunction<FCyAssistantContext()> ResolveContext = {}) = 0;
    virtual void OpenAssistant(const FCyAssistantContext& Context,
        TFunction<FCyAssistantContext()> ResolveContext = {}) = 0;
};
