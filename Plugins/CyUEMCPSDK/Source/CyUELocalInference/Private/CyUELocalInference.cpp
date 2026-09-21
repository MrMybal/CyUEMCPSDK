#include "CyUELocalInference.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
IMPLEMENT_MODULE(FCyUELocalInferenceModule, CyUELocalInference)
bool FCyUELocalInferenceModule::IsRunning()
{
    return Process.IsValid() && FPlatformProcess::IsProcRunning(Process);
}
bool FCyUELocalInferenceModule::Start(const FString &Server, const FString &Model, int32 Port, int32 GPU,
                                      FString &Error)
{
    if (IsRunning())
    {
        Error = TEXT("Local inference is already running");
        return false;
    }
    if (!FPaths::FileExists(Server) || !FPaths::FileExists(Model) || Server.Contains(TEXT("\"")) ||
        Model.Contains(TEXT("\"")) || Port < 1024 || Port > 65535 || GPU < 0 || GPU > 999)
    {
        Error =
            TEXT("Configure an existing llama-server executable and GGUF model, a port and 0-999 GPU layers");
        return false;
    }
    const FString Token =
        FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString Args = FString::Printf(TEXT("--model \"%s\" --host 127.0.0.1 --port %d --n-gpu-layers %d "
                                        "--ctx-size 4096 --parallel 1 --no-webui --api-key %s"),
                                   *Model, Port, GPU, *Token);
    Secret = Token;
    FPlatformProcess::CreatePipe(LogRead, LogWrite);
    Process = FPlatformProcess::CreateProc(*Server, *Args, false, true, true, nullptr, 0,
                                           *FPaths::GetPath(Server), LogWrite, nullptr, LogWrite);
    if (!Process.IsValid())
    {
        Error = TEXT("Failed to start local model server");
        Stop();
        return false;
    }
    ConnectionFile =
        FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("CyUEMCPSDK"),
                        FString::Printf(TEXT("local-%u.json"), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ConnectionFile), true);
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("url"), FString::Printf(TEXT("http://127.0.0.1:%d"), Port));
    O->SetStringField(TEXT("token"), Token);
    O->SetStringField(TEXT("model"), Model);
    FString Text;
    auto W = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(O, W);
    if (!FFileHelper::SaveStringToFile(Text, *ConnectionFile,
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        Error = TEXT("Cannot save local connection");
        Stop();
        return false;
    }
    return true;
}
void FCyUELocalInferenceModule::StartupModule()
{
    LogTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float) {
            FString Text = LogRead ? FPlatformProcess::ReadPipe(LogRead) : FString();
            if (!Secret.IsEmpty())
                Text.ReplaceInline(*Secret, TEXT("[redacted]"));
            PendingLog = (PendingLog + Text).Right(100000);
            return true;
        }),
        .2f);
}
FString FCyUELocalInferenceModule::ReadLog()
{
    FString Text = MoveTemp(PendingLog);
    PendingLog.Empty();
    return Text;
}
void FCyUELocalInferenceModule::Stop()
{
    if (Process.IsValid())
    {
        if (FPlatformProcess::IsProcRunning(Process))
        {
            FPlatformProcess::TerminateProc(Process, true);
            FPlatformProcess::WaitForProc(Process);
        }
        FPlatformProcess::CloseProc(Process);
        Process.Reset();
    }
    FPlatformProcess::ClosePipe(LogRead, LogWrite);
    LogRead = nullptr;
    LogWrite = nullptr;
    Secret.Empty();
    if (!ConnectionFile.IsEmpty())
        IFileManager::Get().Delete(*ConnectionFile);
}
