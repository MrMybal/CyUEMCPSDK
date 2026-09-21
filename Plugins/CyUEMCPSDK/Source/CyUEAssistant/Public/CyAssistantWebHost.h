#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CyAssistantWebHost.generated.h"
/** Private bridge for the packaged assistant page; never bound to remote content. */
UCLASS()
class UCyAssistantWebHost : public UObject
{
    GENERATED_BODY()
  public:
    TFunction<void(const FString &)> Receive;
    UFUNCTION() void PostMessage(const FString &Message)
    {
        if (Receive)
            Receive(Message);
    }
};
