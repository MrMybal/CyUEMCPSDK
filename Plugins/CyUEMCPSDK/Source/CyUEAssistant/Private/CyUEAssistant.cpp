#include "Containers/Ticker.h"
#include "CyAssistantWebHost.h"
#include "CyUEBridge.h"
#include "CyUELocalInference.h"
#include "CyUERegistry.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "IWebBrowserWindow.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SWebBrowser.h"
#include "ToolMenus.h"
#include "UObject/StrongObjectPtr.h"
#include "WebBrowserModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "InputCoreTypes.h"
#include "CyChromeTabs.inl"
#define LOCTEXT_NAMESPACE "CyUEAssistant"
class SCyPanel : public SCompoundWidget
{
    TSharedPtr<SWebBrowser> Browser;
    TStrongObjectPtr<UCyAssistantWebHost> WebHost;
    TSet<FString> Uploads;
    int32 CompletedUploads = 0;
    FString UploadRoot;
    FString PageURL, ActiveOperation;
    void *CompanionRead = nullptr;
    void *CompanionWrite = nullptr;
    bool bSubmitting = false;
    bool bCreatingSession = false;
    bool bConnecting = false;
    double ConnectDeadline = 0;
    FString ServiceURL, Token, Session, Conversation, Job, Log;
    TSharedPtr<SEditableTextBox> Python, Script, Connector, Model, Operation, LocalServer, LocalModel;
    TSharedPtr<SMultiLineEditableTextBox> Prompt, Parameters, Output;
    FProcHandle Companion;
    int64 EventSequence = 0;
    bool bReadingEvents = false;
    FTSTicker::FDelegateHandle Ticker;
    bool bPolling = false;

  public:
    bool bPageReady = false;
    TFunction<void(const FString &)> ForwardLog;
#include "CyAssistantNative.inl"
#if WITH_DEV_AUTOMATION_TESTS
    FString SmokeConversationId() const
    {
        return Conversation;
    }
    bool bSmokeJobCompleted = false;
    bool bSmokeChatRendered = false;
    void SmokeSend()
    {
        Browser->ExecuteJavascript(TEXT("window.cyNativeTest=true;document.getElementById('prompt').value='"
                                        "Reply exactly CYUE_NATIVE_CHAT_OK and nothing "
                                        "else.';document.getElementById('send').click();"));
    }
    bool SmokeConnected() const
    {
        return !Session.IsEmpty();
    }
    bool SmokeUploaded() const
    {
        return CompletedUploads > 0;
    }
    FString SmokeDiagnostics() const
    {
        return FString::Printf(TEXT("ready=%d connected=%d uploaded=%d jobCompleted=%d\n%s"), bPageReady,
                               !Session.IsEmpty(), CompletedUploads, bSmokeJobCompleted, *Log);
    }
    void SmokeConnect()
    {
        Browser->ExecuteJavascript(
            TEXT("window.ue.host.postmessage(JSON.stringify({"
                 "action:'upload',id:'smoke',name:'native-test.txt',data:'dGVzdA=='}));"));
    }
#endif
    SLATE_BEGIN_ARGS(SCyPanel) : _OwnBridge(true)
    {
    }
    SLATE_ARGUMENT(bool, OwnBridge)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args)
    {
        bOwnBridge = Args._OwnBridge;
        FString PythonPath, ScriptPath;
        GConfig->GetString(TEXT("CyUEMCP"), TEXT("Python"), PythonPath, GEditorPerProjectIni);
        GConfig->GetString(TEXT("CyUEMCP"), TEXT("Script"), ScriptPath, GEditorPerProjectIni);
        // Resolve defaults before any dock tab (including Connections) is opened.
        for (auto *Value : {&PythonPath, &ScriptPath})
        {
            if (!Value->IsEmpty())
                continue;
            FString Root = IPluginManager::Get().FindPlugin(TEXT("CyUEMCPSDK"))->GetBaseDir();
            for (int I = 0; I < 7; ++I)
            {
                FString Candidate = Root / (Value == &PythonPath ? TEXT("Runtime/python/Scripts/python.exe")
                                                                 : TEXT("Bridge/run.py"));
                if (FPaths::FileExists(Candidate))
                {
                    *Value = FPaths::ConvertRelativePathToFull(Candidate);
                    break;
                }
                Root = FPaths::GetPath(Root);
            }
        }
        Python = SNew(SEditableTextBox).Text(FText::FromString(PythonPath));
        Script = SNew(SEditableTextBox).Text(FText::FromString(ScriptPath));
        Connector = SNew(SEditableTextBox);
        Model = SNew(SEditableTextBox);
        LocalServer = SNew(SEditableTextBox);
        LocalModel = SNew(SEditableTextBox);
        auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CyUEMCPSDK"));
        PageURL = TEXT("file:///") + FPaths::ConvertRelativePathToFull(
                                         Plugin->GetBaseDir() / TEXT("Resources/Assistant/index.html"));
        UploadRoot =
            FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CyAssistant/Uploads") /
                                              FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IWebBrowserModule::Get(); // UE 5.8 Slate browser expects its module to be loaded explicitly.
        LoadProfiles();
        SAssignNew(Browser, SWebBrowser)
            .InitialURL(PageURL)
            .ShowControls(false)
            .ShowErrorMessage(true)
            .OnBeforeNavigation_Lambda(
                [this](const FString &URL, const FWebNavigationRequest &) { return URL != PageURL; })
            .OnBeforePopup_Lambda([](FString, FString) { return true; })
            .OnConsoleMessage_Lambda(
                [](const FString &Message, const FString &, int32, EWebBrowserConsoleLogSeverity Severity) {
                    if (Severity == EWebBrowserConsoleLogSeverity::Error)
                        UE_LOG(LogTemp, Warning, TEXT("Assistant JavaScript: %s"), *Message.Left(2000));
                });
        ChildSlot[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(4)[ChatToolbar()] +
                  SVerticalBox::Slot().AutoHeight().Padding(
                      6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
                      return FText::FromString(NativeStatus);
                  })] +
                  SVerticalBox::Slot().FillHeight(1)[Browser.ToSharedRef()]];
        WebHost.Reset(NewObject<UCyAssistantWebHost>());
        TWeakPtr<SCyPanel> Weak = SharedThis(this);
        WebHost->Receive = [Weak](const FString &Message) {
            if (auto Panel = Weak.Pin())
                Panel->Action(Message);
        };
        Browser->BindUObject(TEXT("host"), WebHost.Get(), true);
        Ticker =
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SCyPanel::TickJob), .3f);
        Start();
    }
    ~SCyPanel()
    {
        if (WebHost.IsValid())
            WebHost->Receive = nullptr;
        for (const auto &File : Uploads)
            IFileManager::Get().Delete(*File);
        IFileManager::Get().DeleteDirectory(*UploadRoot, false, false);
        FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
        if (!Session.IsEmpty())
        {
            auto P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("sessionId"), Session);
            P->SetBoolField(TEXT("cancelRunning"), true);
            auto B = MakeShared<FJsonObject>();
            B->SetStringField(TEXT("protocolVersion"), TEXT("1.0"));
            B->SetNumberField(TEXT("id"), 1);
            B->SetStringField(TEXT("method"), TEXT("FinishTask"));
            B->SetObjectField(TEXT("params"), P);
            auto R = FHttpModule::Get().CreateRequest();
            R->SetURL(ServiceURL);
            R->SetVerb(TEXT("POST"));
            R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
            R->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            R->SetContentAsString(CyUEJson::Stringify(B));
            R->ProcessRequest();
        }
        if (Companion.IsValid())
            FPlatformProcess::CloseProc(Companion);
        FPlatformProcess::ClosePipe(CompanionRead, CompanionWrite);
    }
    void Append(const FString &Text)
    {
        if (ForwardLog)
            ForwardLog(Text);
        FString Safe = Text;
        if (!Token.IsEmpty())
            Safe.ReplaceInline(*Token, TEXT("[redacted]"));
        Log += FDateTime::Now().ToString(TEXT("[%H:%M:%S] ")) + Safe + TEXT("\n");
        if (Log.Len() > 200000)
            Log = Log.Right(200000);
        if (Safe == TEXT("assistant_send: Completed"))
            NativeStatus = TEXT("Ready");
        else if (Safe == TEXT("assistant_open: Completed"))
            NativeStatus = TEXT("Conversation ready");
        else if (Safe == TEXT("assistant_close: Completed"))
            NativeStatus = TEXT("New conversation");
        else if (Safe == TEXT("assistant_probe: Completed"))
            NativeStatus = TEXT("Connection check complete");
        else if (Safe.Contains(TEXT(": Failed")))
            NativeStatus = TEXT("Request failed. See Connection logs for details.");
        else if (Safe != TEXT("Job submitted") && !Safe.StartsWith(TEXT("[")) && !Safe.StartsWith(TEXT("{")))
            NativeStatus = Safe.Left(240);
        if (Output)
            Output->SetText(FText::FromString(Log));
        Send(TEXT("log"), Log);
    }
    void Send(const FString &Type, const FString &Text)
    {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), Type);
        O->SetStringField(TEXT("text"), Text);
        Browser->ExecuteJavascript(TEXT("window.cyReceive&&window.cyReceive(") + CyUEJson::Stringify(O) +
                                   TEXT(")"));
    }
    void Action(const FString &Data)
    {
        auto O = CyUEJson::Parse(Data);
        if (!O)
            return;
        FString A;
        O->TryGetStringField(TEXT("action"), A);
        auto Get = [&](const TCHAR *K) {
            FString V;
            O->TryGetStringField(K, V);
            return V;
        };
#if WITH_DEV_AUTOMATION_TESTS
        if (A == TEXT("testRendered"))
        {
            bSmokeChatRendered = Get(TEXT("text")).Contains(TEXT("CYUE_NATIVE_CHAT_OK"));
            return;
        }
#endif
        if (A == TEXT("ready"))
        {
            bPageReady = true;
            auto Config = MakeShared<FJsonObject>();
            for (auto Key : {TEXT("Python"), TEXT("Script"), TEXT("server"), TEXT("gguf"), TEXT("connector"),
                             TEXT("model"), TEXT("port"), TEXT("gpu"), TEXT("executable"), TEXT("transport")})
            {
                FString V;
                GConfig->GetString(TEXT("CyUEMCP"), Key, V, GEditorPerProjectIni);
                if (V.IsEmpty() && (FString(Key) == TEXT("Python") || FString(Key) == TEXT("Script")))
                {
                    FString Root = IPluginManager::Get().FindPlugin(TEXT("CyUEMCPSDK"))->GetBaseDir();
                    for (int32 I = 0; I < 7; I++)
                    {
                        FString Candidate =
                            Root / (FString(Key) == TEXT("Python") ? TEXT("Runtime/python/Scripts/python.exe")
                                                                   : TEXT("Bridge/run.py"));
                        if (FPaths::FileExists(Candidate))
                        {
                            V = FPaths::ConvertRelativePathToFull(Candidate);
                            break;
                        }
                        Root = FPaths::GetPath(Root);
                    }
                }
                Config->SetStringField(Key, V);
                if (FString(Key) == TEXT("Python"))
                    Python->SetText(FText::FromString(V));
                if (FString(Key) == TEXT("Script"))
                    Script->SetText(FText::FromString(V));
            }
            Send(TEXT("config"), CyUEJson::Stringify(Config));
            NotifyProfile();
            Start();
        }
        else if (A == TEXT("save"))
        {
            const TSharedPtr<FJsonObject> *Values;
            if (O->TryGetObjectField(TEXT("values"), Values))
            {
                for (auto Key :
                     {TEXT("Python"), TEXT("Script"), TEXT("server"), TEXT("gguf"), TEXT("connector"),
                      TEXT("model"), TEXT("port"), TEXT("gpu"), TEXT("executable"), TEXT("transport")})
                {
                    FString V;
                    if ((*Values)->TryGetStringField(Key, V))
                        GConfig->SetString(TEXT("CyUEMCP"), Key, *V, GEditorPerProjectIni);
                }
                GConfig->Flush(false, GEditorPerProjectIni);
                Append(TEXT("Settings saved for this project"));
            }
        }
        else if (A == TEXT("connect"))
        {
            Python->SetText(FText::FromString(Get(TEXT("Python"))));
            Script->SetText(FText::FromString(Get(TEXT("Script"))));
            Start();
        }
        else if (A == TEXT("open"))
        {
            auto P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("connector"), Get(TEXT("connector")));
            P->SetStringField(TEXT("model"), Get(TEXT("model")));
            P->SetStringField(TEXT("executable"), Get(TEXT("executable")));
            P->SetStringField(TEXT("transport"), Get(TEXT("transport")));
            Submit(TEXT("assistant_open"), P);
        }
        else if (A == TEXT("send"))
        {
            if (PendingSend || (!Job.IsEmpty() && ActiveOperation != TEXT("assistant_probe")))
            {
                Send(TEXT("error"), TEXT("Wait for the active job."));
                return;
            }
            auto P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("conversationId"), Conversation);
            P->SetStringField(TEXT("text"), Get(TEXT("text")));
            const TArray<TSharedPtr<FJsonValue>> *Files;
            if (O->TryGetArrayField(TEXT("files"), Files))
                P->SetArrayField(TEXT("files"), *Files);
            PendingSend = P;
            NativeStatus = TEXT("Connecting assistant...");
            if (Session.IsEmpty())
                Start();
            PumpAssistant();
        }
        else if (A == TEXT("close"))
        {
            auto P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("conversationId"), Conversation);
            Submit(TEXT("assistant_close"), P);
        }
        else if (A == TEXT("catalog"))
            Submit(TEXT("assistant_connectors"), MakeShared<FJsonObject>());
        else if (A == TEXT("operations"))
        {
            auto P = MakeShared<FJsonObject>();
            P->SetArrayField(TEXT("operations"), FCyUEMCPCoreModule::Get().Registry.Describe());
            Send(TEXT("operations"), CyUEJson::Stringify(P));
        }
        else if (A == TEXT("run"))
        {
            auto P = CyUEJson::Parse(Get(TEXT("parameters")));
            if (P)
                Submit(Get(TEXT("operation")), P.ToSharedRef());
            else
                Append(TEXT("Invalid parameters JSON"));
        }
        else if (A == TEXT("writes"))
            FCyUEBridgeModule::Get().bAllowWrites = O->GetBoolField(TEXT("enabled"));
        else if (A == TEXT("cancel"))
        {
            PendingSend.Reset();
            Send(TEXT("error"), TEXT("Request cancelled"));
            if (Job.IsEmpty())
            {
                Send(TEXT("error"), TEXT("Request cancelled"));
                return;
            }
            auto P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("jobId"), Job);
            Call(TEXT("CancelJob"), P, [this](auto R) { Append(CyUEJson::Stringify(R)); });
        }
        else if (A == TEXT("localStart"))
        {
            FString Error;
            int32 Port = FCString::Atoi(*Get(TEXT("port"))), GPU = FCString::Atoi(*Get(TEXT("gpu")));
            auto &Local = FModuleManager::LoadModuleChecked<FCyUELocalInferenceModule>("CyUELocalInference");
            if (Local.Start(Get(TEXT("server")), Get(TEXT("gguf")), Port, GPU, Error))
                Append(TEXT(
                    "Local server started. Select local and open a conversation; loading may take time."));
            else
                Append(Error);
        }
        else if (A == TEXT("localStop"))
        {
            FModuleManager::LoadModuleChecked<FCyUELocalInferenceModule>("CyUELocalInference").Stop();
            Append(TEXT("Local model stopped"));
        }
        else if (A == TEXT("openLink"))
        {
            FString URL = Get(TEXT("url"));
            if (URL.StartsWith(TEXT("https://")) || URL.StartsWith(TEXT("http://")))
                FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
        }
        else if (A == TEXT("exportLog"))
        {
            TArray<FString> Files;
            auto *Desktop = FDesktopPlatformModule::Get();
            if (Desktop &&
                Desktop->SaveFileDialog(nullptr, TEXT("Save connection log"), FPaths::ProjectSavedDir(),
                                        TEXT("cyue-connection.log"), TEXT("Log|*.log"), 0, Files) &&
                Files.Num())
                FFileHelper::SaveStringToFile(Log, *Files[0],
                                              FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }
        else if (A == TEXT("copy"))
            FPlatformApplicationMisc::ClipboardCopy(*Get(TEXT("text")));
        else if (A == TEXT("upload"))
        {
            FString Encoded = Get(TEXT("data"));
            TArray<uint8> Bytes;
            if (Encoded.Len() > 45 * 1024 * 1024 || !FBase64::Decode(Encoded, Bytes) ||
                Bytes.Num() > 32 * 1024 * 1024)
            {
                Send(TEXT("error"), TEXT("Attachment limit: 32 MiB per file"));
                return;
            }
            FString Name = FPaths::MakeValidFileName(FPaths::GetCleanFilename(Get(TEXT("name"))));
            FString Path = UploadRoot / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("-") + Name);
            IFileManager::Get().MakeDirectory(*UploadRoot, true);
            if (!FFileHelper::SaveArrayToFile(Bytes, *Path))
            {
                Send(TEXT("error"), TEXT("Cannot stage attachment"));
                return;
            }
            Uploads.Add(Path);
            ++CompletedUploads;
            auto R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("id"), Get(TEXT("id")));
            R->SetStringField(TEXT("path"), Path);
            R->SetStringField(TEXT("name"), Name);
            Send(TEXT("uploaded"), CyUEJson::Stringify(R));
        }
        else if (A == TEXT("discard"))
        {
            FString Path = Get(TEXT("path"));
            if (Uploads.Remove(Path))
                IFileManager::Get().Delete(*Path);
        }
        else if (A == TEXT("browse"))
        {
            TArray<FString> Files;
            auto *Desktop = FDesktopPlatformModule::Get();
            if (Desktop &&
                Desktop->OpenFileDialog(nullptr, TEXT("Select a local file"), TEXT(""), TEXT(""),
                                        TEXT("All files|*.*"), 0, Files) &&
                Files.Num())
            {
                auto R = MakeShared<FJsonObject>();
                R->SetStringField(TEXT("field"), Get(TEXT("field")));
                R->SetStringField(TEXT("path"), Files[0]);
                Send(TEXT("file"), CyUEJson::Stringify(R));
            }
        }
    }
    void Start()
    {
        if (bCreatingSession)
            return;
        if (!Session.IsEmpty())
        {
            Append(TEXT("Already connected"));
            return;
        }
        auto File = FCyUEBridgeModule::Get().GetConnectionFile();
        auto Service = FPaths::ChangeExtension(File, TEXT("service.json"));
        FString Data;
        if (FFileHelper::LoadFileToString(Data, *Service))
        {
            auto O = CyUEJson::Parse(Data);
            if (O && O->TryGetStringField(TEXT("url"), ServiceURL) &&
                O->TryGetStringField(TEXT("token"), Token))
            {
                auto P = MakeShared<FJsonObject>();
                P->SetBoolField(TEXT("temporary"), false);
                bCreatingSession = true;
                Call(TEXT("CreateSession"), P, [this](auto R) {
                    bCreatingSession = false;
                    if (R->TryGetStringField(TEXT("sessionId"), Session))
                    {
                        bConnecting = false;
                        Send(TEXT("connected"), Session);
                        Append(TEXT("Connected to CyToolsCore"));
                        if (!bStartupChecksQueued && bOwnBridge)
                        {
                            bStartupChecksQueued = true;
                            for (auto Profile : Profiles)
                                if (Profile->bStartup)
                                    QueueCheck(Profile);
                        }
                        PumpAssistant();
                    }
                });
                return;
            }
        }
        if (!bOwnBridge)
        {
            if (!bConnecting)
            {
                bConnecting = true;
                ConnectDeadline = FPlatformTime::Seconds() + 30;
            }
            return;
        }
        FString Exe = Python->GetText().ToString(), Path = Script->GetText().ToString();
        if (!FPaths::FileExists(Exe) || !FPaths::FileExists(Path) || Path.Contains(TEXT("\"")))
        {
            AssistantFailure(TEXT("Configure Python and Bridge/run.py in the Connections tab."));
            return;
        }
        if (Companion.IsValid() && FPlatformProcess::IsProcRunning(Companion))
        {
            return;
        }
        GConfig->SetString(TEXT("CyUEMCP"), TEXT("Python"), *Exe, GEditorPerProjectIni);
        GConfig->SetString(TEXT("CyUEMCP"), TEXT("Script"), *Path, GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
        if (Companion.IsValid())
            FPlatformProcess::CloseProc(Companion);
        FPlatformProcess::ClosePipe(CompanionRead, CompanionWrite);
        CompanionRead = nullptr;
        CompanionWrite = nullptr;
        FPlatformProcess::CreatePipe(CompanionRead, CompanionWrite);
        Companion = FPlatformProcess::CreateProc(
            *Exe, *FString::Printf(TEXT("\"%s\" --editor \"%s\""), *Path, *File), false, true, true, nullptr,
            0, *FPaths::GetPath(Path), CompanionWrite, nullptr, CompanionWrite);
        bConnecting = Companion.IsValid();
        ConnectDeadline = FPlatformTime::Seconds() + 30;
        Append(bConnecting ? TEXT("Companion launched; waiting for connection...")
                           : TEXT("Cannot launch companion"));
    }
    void Call(const FString &Method, TSharedRef<FJsonObject> P, TFunction<void(TSharedRef<FJsonObject>)> Done)
    {
        if (ServiceURL.IsEmpty() || Token.IsEmpty())
        {
            Append(TEXT("Connect the companion first"));
            return;
        }
        auto Body = MakeShared<FJsonObject>();
        Body->SetStringField(TEXT("protocolVersion"), TEXT("1.0"));
        Body->SetNumberField(TEXT("id"), 1);
        Body->SetStringField(TEXT("method"), Method);
        Body->SetObjectField(TEXT("params"), P);
        auto Request = FHttpModule::Get().CreateRequest();
        Request->SetTimeout(30);
        Request->SetURL(ServiceURL);
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(CyUEJson::Stringify(Body));
        TWeakPtr<SCyPanel> Weak = SharedThis(this);
        Request->OnProcessRequestComplete().BindLambda(
            [Weak, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr R, bool Ok) {
                auto Self = Weak.Pin();
                if (!Self)
                    return;
                if (!Ok || !R)
                {
                    Self->bPolling = false;
                    Self->bReadingEvents = false;
                    Self->bSubmitting = false;
                    Self->bCreatingSession = false;
                    Self->bConnecting = false;
                    Self->Session.Empty();
                    Self->Conversation.Empty();
                    for (auto P : Self->Profiles)
                        P->ConversationId.Empty();
                    Self->bStartupChecksQueued = false;
                    Self->Job.Empty();
                    Self->Send(TEXT("disconnected"), TEXT("Connection lost"));
                    Self->Send(TEXT("error"), TEXT("Companion connection failed. Check Connection logs."));
                    Self->AssistantFailure(TEXT("Companion connection failed. Open Connections and retry."));
                    return;
                }
                auto O = CyUEJson::Parse(R->GetContentAsString());
                if (!O)
                {
                    Self->bPolling = false;
                    Self->bReadingEvents = false;
                    Self->bSubmitting = false;
                    Self->bCreatingSession = false;
                    Self->Send(TEXT("error"), TEXT("Invalid companion response"));
                    Self->AssistantFailure(TEXT("Invalid companion response"));
                    return;
                }
                const TSharedPtr<FJsonObject> *Result;
                if (O->TryGetObjectField(TEXT("result"), Result))
                    Done(Result->ToSharedRef());
                else
                {
                    Self->bPolling = false;
                    Self->bReadingEvents = false;
                    Self->bSubmitting = false;
                    Self->bCreatingSession = false;
                    Self->Send(TEXT("error"), CyUEJson::Stringify(O.ToSharedRef()));
                    Self->AssistantFailure(CyUEJson::Stringify(O.ToSharedRef()));
                }
            });
        Request->ProcessRequest();
    }
    void Submit(const FString &Id, TSharedRef<FJsonObject> P)
    {
        if (!Job.IsEmpty() || bSubmitting)
        {
            Send(TEXT("error"), TEXT("Wait for the active job or cancel it"));
            return;
        }
        if (Session.IsEmpty())
        {
            Send(TEXT("error"), TEXT("Connect the bridge first"));
            return;
        }
        bSubmitting = true;
        ActiveOperation = Id;
        auto R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("sessionId"), Session);
        R->SetStringField(TEXT("operationId"), Id);
        R->SetObjectField(TEXT("parameters"), P);
        Call(TEXT("SubmitJob"), R, [this](auto J) {
            bSubmitting = false;
            J->TryGetStringField(TEXT("jobId"), Job);
            Append(TEXT("Job submitted"));
        });
    }
    bool TickJob(float)
    {
        if (CompanionRead)
        {
            FString Text = FPlatformProcess::ReadPipe(CompanionRead);
            if (!Text.IsEmpty())
                Append(TEXT("[bridge] ") + Text);
        }
        if (FModuleManager::Get().IsModuleLoaded("CyUELocalInference"))
        {
            FString Text =
                FModuleManager::GetModuleChecked<FCyUELocalInferenceModule>("CyUELocalInference").ReadLog();
            if (!Text.IsEmpty())
                Append(TEXT("[local] ") + Text);
        }

        if (bConnecting)
        {
            if (FPlatformTime::Seconds() > ConnectDeadline)
            {
                bConnecting = false;
                AssistantFailure(TEXT("Companion startup timed out; check executable and logs"));
            }
            else if (FPaths::FileExists(FPaths::ChangeExtension(FCyUEBridgeModule::Get().GetConnectionFile(),
                                                                TEXT("service.json"))))
                Start();
        }
        if (!Session.IsEmpty() && !bReadingEvents)
        {
            bReadingEvents = true;
            auto E = MakeShared<FJsonObject>();
            E->SetNumberField(TEXT("after"), EventSequence);
            Call(TEXT("GetEvents"), E, [this](auto R) {
                bReadingEvents = false;
                const TArray<TSharedPtr<FJsonValue>> *Items = nullptr;
                if (R->TryGetArrayField(TEXT("events"), Items))
                    for (auto &V : *Items)
                    {
                        auto O = V->AsObject();
                        double Seq = 0;
                        if (O->TryGetNumberField(TEXT("sequence"), Seq))
                            EventSequence = FMath::Max(EventSequence, static_cast<int64>(Seq));
                        FString EventSession;
                        if (O->TryGetStringField(TEXT("sessionId"), EventSession) && EventSession != Session)
                            continue;
                        FString EventJob;
                        if (!O->TryGetStringField(TEXT("jobId"), EventJob) || EventJob != Job)
                            continue;
                        FString Type;
                        O->TryGetStringField(TEXT("type"), Type);
                        if (Type == TEXT("ConnectorEvent"))
                        {
                            const TSharedPtr<FJsonObject> *Data = nullptr;
                            if (O->TryGetObjectField(TEXT("data"), Data))
                            {
                                FString Kind, Text;
                                (*Data)->TryGetStringField(TEXT("type"), Kind);
                                (*Data)->TryGetStringField(TEXT("text"), Text);
                                if (Kind == TEXT("ModelResolved"))
                                {
                                    ActualModel = Text;
                                    NotifyProfile();
                                }
                                else if (Kind == TEXT("TextDelta"))
                                {
                                    Send(TEXT("delta"), Text);
                                }
                                else if (Kind == TEXT("TextSnapshot"))
                                {
                                    Send(TEXT("snapshot"), Text);
                                }
                                else if (Kind != TEXT("TurnCompleted"))
                                {
                                    Append(TEXT("[provider] ") + Kind + TEXT(" ") + Text);
                                }
                            }
                        }
                    }
            });
        }
        PumpAssistant();
        if (Job.IsEmpty() || bPolling)
            return true;
        bPolling = true;
        auto P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("jobId"), Job);
        Call(TEXT("GetJob"), P, [this](auto J) {
            bPolling = false;
            FString State;
            J->TryGetStringField(TEXT("state"), State);
            if (State == TEXT("Completed") || State == TEXT("Failed") || State == TEXT("Cancelled"))
            {
                const TSharedPtr<FJsonObject> *Result;
                if (J->TryGetObjectField(TEXT("outputs"), Result))
                    (*Result)->TryGetStringField(TEXT("conversationId"), Conversation);
#if WITH_DEV_AUTOMATION_TESTS
                if (ActiveOperation == TEXT("ue_get_context") && State == TEXT("Completed"))
                    bSmokeJobCompleted = true;
#endif
                if (ActiveOperation == TEXT("assistant_probe"))
                {
                    if (Checking)
                    {
                        Checking->Status = State;
                        if (J->TryGetObjectField(TEXT("outputs"), Result))
                        {
                            (*Result)->TryGetStringField(TEXT("status"), Checking->Status);
                            (*Result)->TryGetStringField(TEXT("message"), Checking->Detail);
                            Checking->Detail += TEXT("\nChecked ") + FDateTime::Now().ToString();
                            const TArray<TSharedPtr<FJsonValue>> *Models = nullptr;
                            Checking->Models.Empty();
                            if ((*Result)->TryGetArrayField(TEXT("models"), Models))
                                for (auto V : *Models)
                                {
                                    auto M = V->AsObject();
                                    FString Id;
                                    if (M && (M->TryGetStringField(TEXT("model"), Id) ||
                                              M->TryGetStringField(TEXT("id"), Id)))
                                        Checking->Models.Add(MakeShared<FString>(Id));
                                }
                            SaveProfiles();
                            RefreshModels();
                        }
                        else
                            Checking->Detail = CyUEJson::Stringify(J);
                        Checking.Reset();
                    }
                }
                else
                {
                    if (State != TEXT("Completed"))
                        PendingSend.Reset();
                    if (J->TryGetObjectField(TEXT("outputs"), Result))
                    {
                        const TSharedPtr<FJsonObject> *NativeSession = nullptr;
                        if ((*Result)->TryGetObjectField(TEXT("session"), NativeSession))
                            (*NativeSession)->TryGetStringField(TEXT("model"), ActualModel);
                        NotifyProfile();
                    }
                    Send(TEXT("job"), CyUEJson::Stringify(J));
                    if (Selected)
                        Selected->ConversationId = Conversation;
                }
                Append(ActiveOperation + TEXT(": ") + State);
                if (State == TEXT("Failed"))
                    Append(CyUEJson::Stringify(J));
                if (ActiveOperation == TEXT("assistant_close") && State == TEXT("Completed"))
                {
                    Conversation.Empty();
                    if (Selected)
                        Selected->ConversationId.Empty();
                    Send(TEXT("reset"), TEXT(""));
                }
                Job.Empty();
                PumpAssistant();
            }
        });
        return true;
    }
};
#include "CyChatWorkspace.inl"
// One public Assistant tab. Settings pages are children, not editor-wide dock tabs.
class FCyUEAssistantModule : public IModuleInterface
{
    TSharedPtr<SCyPanel> Panel;
    TSharedPtr<SCyChatWorkspace> Workspace;
    TSharedPtr<SWidgetSwitcher> MainPages, SettingsPages;
    TSharedPtr<SWidget> Root;
    TSharedRef<SWidget> GetAssistant()
    {
        if (!Root)
        {
            Panel = SNew(SCyPanel);
            Workspace = SNew(SCyChatWorkspace);
            auto OpenSettings = [this](FName Name) {
                MainPages->SetActiveWidgetIndex(1);
                SettingsPages->SetActiveWidgetIndex(Name == TEXT("CyUELogs") ? 1 : 0);
            };
            Panel->OpenNativeTab = OpenSettings;
            Workspace->Initialize(OpenSettings);
            TWeakPtr<SCyPanel> Weak = Panel;
            Workspace->LogSink = [Weak](const FString &Message) {
                if (auto P = Weak.Pin())
                    P->Append(Message);
            };
            auto Settings = SNew(SVerticalBox);
            auto Tabs = SNew(SHorizontalBox);
            int Index = 0;
            for (auto Label : {TEXT("Providers"), TEXT("Connection logs")})
            {
                int Page = Index++;
                Tabs->AddSlot().AutoWidth()[SNew(SCyChromeTab)
                                                .Label(FText::FromString(Label))
                                                .Width(165)
                                                .Active_Lambda([this, Page] {
                                                    return SettingsPages &&
                                                           SettingsPages->GetActiveWidgetIndex() == Page;
                                                })
                                                .OnSelected(FSimpleDelegate::CreateLambda([this, Page] {
                                                    SettingsPages->SetActiveWidgetIndex(Page);
                                                }))];
            }
            Settings->AddSlot().AutoHeight()[Tabs];
            Settings->AddSlot().FillHeight(1)[SAssignNew(SettingsPages, SWidgetSwitcher) +
                                              SWidgetSwitcher::Slot()[Panel->ConnectionsWidget()] +
                                              SWidgetSwitcher::Slot()[Panel->LogsWidget()]];
            Root =
                SNew(SVerticalBox) +
                SVerticalBox::Slot().AutoHeight()
                    [SNew(SHorizontalBox) +
                     SHorizontalBox::Slot().AutoWidth()[SNew(SCyChromeTab)
                                                            .Label(LOCTEXT("ChatPage", "Chat"))
                                                            .Width(125)
                                                            .Active_Lambda([this] {
                                                                return MainPages &&
                                                                       MainPages->GetActiveWidgetIndex() == 0;
                                                            })
                                                            .OnSelected(FSimpleDelegate::CreateLambda([this] {
                                                                MainPages->SetActiveWidgetIndex(0);
                                                            }))] +
                     SHorizontalBox::Slot().AutoWidth()[SNew(SCyChromeTab)
                                                            .Label(LOCTEXT("SettingsPage", "Settings"))
                                                            .Width(125)
                                                            .Active_Lambda([this] {
                                                                return MainPages &&
                                                                       MainPages->GetActiveWidgetIndex() == 1;
                                                            })
                                                            .OnSelected(FSimpleDelegate::CreateLambda([this] {
                                                                MainPages->SetActiveWidgetIndex(1);
                                                            }))]] +
                SVerticalBox::Slot().FillHeight(
                    1)[SAssignNew(MainPages, SWidgetSwitcher) +
                       SWidgetSwitcher::Slot()[Workspace.ToSharedRef()] + SWidgetSwitcher::Slot()[Settings]];
        }
        return Root.ToSharedRef();
    }

  public:
#if WITH_DEV_AUTOMATION_TESTS
    TSharedRef<SWidget> SmokeRoot()
    {
        return GetAssistant();
    }
    TSharedPtr<SCyPanel> SmokeChat()
    {
        return Workspace->Chat(0);
    }
    int32 SmokeProfiles() const
    {
        return Panel->Profiles.Num();
    }
    void SmokePage(int32 Page)
    {
        if (Page < 0)
            MainPages->SetActiveWidgetIndex(0);
        else
        {
            Panel->OpenNativeTab(Page == 2 ? TEXT("CyUELogs") : TEXT("CyUEConnections"));
            if (Page == 1)
                for (auto P : Panel->Profiles)
                    if (P->Provider == TEXT("local"))
                    {
                        Panel->EditProfile(P);
                        break;
                    }
        }
    }

#endif
    void StartupModule() override
    {
        FGlobalTabmanager::Get()
            ->RegisterNomadTabSpawner(TEXT("CyUEMCP"),
                                      FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs &) {
                                          return SNew(SDockTab).TabRole(ETabRole::NomadTab)[GetAssistant()];
                                      }))
            .SetDisplayName(LOCTEXT("Tab", "Assistant"));
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCyUEAssistantModule::Menu));
    }
    void Menu()
    {
        FToolMenuOwnerScoped Owner(this);
        UToolMenus::Get()
            ->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"))
            ->FindOrAddSection(TEXT("CyTools"))
            .AddMenuEntry(TEXT("CyUEMCP"), LOCTEXT("Entry", "Assistant"),
                          LOCTEXT("Tip", "Assistant conversations and settings"), FSlateIcon(),
                          FUIAction(FExecuteAction::CreateLambda(
                              [] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("CyUEMCP"))); })));
    }
    void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        if (auto Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(FName(TEXT("CyUEMCP"))))
            Tab->RequestCloseTab();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("CyUEMCP"));
        Root.Reset();
        MainPages.Reset();
        SettingsPages.Reset();
        Workspace.Reset();
        Panel.Reset();
    }
};
IMPLEMENT_MODULE(FCyUEAssistantModule, CyUEAssistant)
#undef LOCTEXT_NAMESPACE

#if WITH_DEV_AUTOMATION_TESTS
static TSharedPtr<SCyPanel> CyTestPanel;
static TSharedPtr<SWindow> CyTestWindow, CySettingsTestWindow;
static void CyCaptureWindow(TSharedPtr<SWindow> Window, const TCHAR *Name)
{
    TArray<FColor> Pixels;
    FIntVector Size = FIntVector::ZeroValue;
    if (Window && FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size))
    {
        TArray64<uint8> Bytes;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Bytes);
        FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectSavedDir() / Name));
    }
}
static double CyTestDeadline = 0;
static int32 CySmokePhase = 0;
static bool CySmokeLiveChat = false;
static double CyRenderedAt = 0;
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyWaitForBrowser, FAutomationTestBase *, Test);
bool FCyWaitForBrowser::Update()
{
    if (CyTestPanel->bPageReady && CySmokePhase == 0)
    {
        CySmokePhase = 1;
        CyTestPanel->SmokeConnect();
    }
    if (CyTestPanel->SmokeConnected() && !CyTestPanel->IsBusy() && CySmokePhase == 1)
    {
        CySmokePhase = 2;
        if (CySmokeLiveChat)
        {
            CySettingsTestWindow = SNew(SWindow)
                                       .Title(FText::FromString(TEXT("Connection profile test")))
                                       .ClientSize(FVector2D(660, 760))[CyTestPanel->ConnectionsWidget()];
            FSlateApplication::Get().AddWindow(CySettingsTestWindow.ToSharedRef(), true);
            CyTestPanel->NativeTestActions[TEXT("+ New profile")]();
            CyTestPanel->ProfileName->SetText(FText::FromString(TEXT("Retest fixture")));
            CyTestPanel->ExeField->SetText(FText::FromString(TEXT("Z:/missing-cyue-test-provider.exe")));
            CyTestPanel->NativeTestActions[TEXT("Test connection")]();
        }
        else
            CyTestPanel->Submit(TEXT("ue_get_context"), MakeShared<FJsonObject>());
    }
    if (CySmokeLiveChat && CySmokePhase == 2 && !CyTestPanel->IsBusy() && CyTestPanel->CheckQueue.IsEmpty())
    {
        Test->TestTrue(TEXT("Missing executable diagnosed in its own profile"),
                       CyTestPanel->Editing->Status != TEXT("Ready") &&
                           !CyTestPanel->Editing->Detail.IsEmpty());
        CyTestPanel->ExeField->SetText(FText::GetEmpty());
        CyTestPanel->NativeTestActions[TEXT("Test connection")]();
        CySmokePhase = 3;
    }
    else if (CySmokeLiveChat && CySmokePhase == 3 && !CyTestPanel->IsBusy() &&
             CyTestPanel->CheckQueue.IsEmpty())
    {
        Test->TestEqual(TEXT("Native Save and test successfully retests installed Codex"),
                        CyTestPanel->Editing->Status, FString(TEXT("Ready")));
        CyCaptureWindow(CySettingsTestWindow, TEXT("cy-assistant-connections.png"));
        CyTestPanel->Profiles.Remove(CyTestPanel->Editing);
        CyTestPanel->SaveProfiles();
        CySettingsTestWindow->RequestDestroyWindow();
        CySettingsTestWindow.Reset();
        CyTestPanel->SmokeSend();
        CySmokePhase = 4;
    }
    if ((CySmokeLiveChat ? (CyTestPanel->bSmokeChatRendered && !CyTestPanel->IsBusy())
                         : CyTestPanel->bSmokeJobCompleted) &&
        CyTestPanel->SmokeUploaded())
    {
        if (CySmokeLiveChat)
        {
            if (CyRenderedAt == 0)
                CyRenderedAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - CyRenderedAt < .5)
                return false;
        }
        Test->TestTrue(
            TEXT("Unreal CEF initialized; upload and requested real bridge/chat operation completed"), true);
        if (CySmokeLiveChat)
            CyCaptureWindow(CyTestWindow, TEXT("cy-assistant-live-chat.png"));
        if (CySettingsTestWindow)
        {
            CySettingsTestWindow->RequestDestroyWindow();
            CySettingsTestWindow.Reset();
        }
        CyTestWindow->RequestDestroyWindow();
        CyTestPanel.Reset();
        CyTestWindow.Reset();
        return true;
    }
    if (FPlatformTime::Seconds() > CyTestDeadline)
    {
        Test->AddError(TEXT("Assistant UI/bridge test did not complete before its deadline: ") +
                       CyTestPanel->SmokeDiagnostics());
        if (CySmokeLiveChat)
            CyCaptureWindow(CyTestWindow, TEXT("cy-assistant-live-chat.png"));
        if (CySettingsTestWindow)
        {
            CySettingsTestWindow->RequestDestroyWindow();
            CySettingsTestWindow.Reset();
        }
        CyTestWindow->RequestDestroyWindow();
        CyTestPanel.Reset();
        CyTestWindow.Reset();
        return true;
    }
    return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyAssistantBrowserTest, "CyUEMCP.Assistant.BrowserReady",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyAssistantBrowserTest::RunTest(const FString &Parameter)
{
    CySmokeLiveChat = Parameter == TEXT("live");
    CyTestPanel = SNew(SCyPanel);
    CyTestWindow = SNew(SWindow)
                       .Title(FText::FromString(TEXT("Assistant UI test")))
                       .ClientSize(FVector2D(520, 550))[CyTestPanel.ToSharedRef()];
    FSlateApplication::Get().AddWindow(CyTestWindow.ToSharedRef(), true);
    CySmokePhase = 0;
    CyRenderedAt = 0;
    CyTestDeadline = FPlatformTime::Seconds() + (CySmokeLiveChat ? 150 : 60);
    ADD_LATENT_AUTOMATION_COMMAND(FCyWaitForBrowser(this));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyAssistantLiveTest, "CyUEMCP.Assistant.LiveConversation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyAssistantLiveTest::RunTest(const FString &)
{
    CySmokeLiveChat = true;
    CyTestPanel = SNew(SCyPanel);
    CyTestWindow = SNew(SWindow)
                       .Title(FText::FromString(TEXT("Live assistant test")))
                       .ClientSize(FVector2D(520, 550))[CyTestPanel.ToSharedRef()];
    FSlateApplication::Get().AddWindow(CyTestWindow.ToSharedRef(), true);
    CySmokePhase = 0;
    CyRenderedAt = 0;
    CyTestDeadline = FPlatformTime::Seconds() + 150;
    ADD_LATENT_AUTOMATION_COMMAND(FCyWaitForBrowser(this));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
static TSharedPtr<SCyChatWorkspace> CyMultiWorkspace;
static TSharedPtr<SCyPanel> CyMultiHost;
static TSharedPtr<SWindow> CyMultiWindow;
static int32 CyMultiPhase = 0;
static double CyMultiDeadline = 0, CyMultiRenderedAt = 0;
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyWaitForMultiChat, FAutomationTestBase *, Test);
bool FCyWaitForMultiChat::Update()
{
    auto First = CyMultiWorkspace->Chat(0);
    auto Second = CyMultiWorkspace->Chat(1);
    if (CyMultiPhase == 0 && First->bPageReady && First->SmokeConnected())
    {
        First->SmokeSend();
        CyMultiWorkspace->AddChat();
        CyMultiPhase = 1;
    }
    else if (CyMultiPhase == 1 && Second && Second->bPageReady && Second->SmokeConnected())
    {
        Second->SmokeSend();
        CyMultiPhase = 2;
    }
    else if (CyMultiPhase == 2 && !First->IsBusy() && !Second->IsBusy() &&
             !First->SmokeConversationId().IsEmpty() && !Second->SmokeConversationId().IsEmpty())
    {
        CyMultiWorkspace->ActivateChat(First);
        CyMultiPhase = 3;
    }
    else if (CyMultiPhase == 3 && First->bSmokeChatRendered && Second->bSmokeChatRendered)
    {
        if (CyMultiRenderedAt == 0)
            CyMultiRenderedAt = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - CyMultiRenderedAt < .6)
            return false;
        Test->TestEqual(TEXT("Two independent native conversation tabs"), CyMultiWorkspace->Count(), 2);
        Test->TestTrue(TEXT("Independent backend conversations"),
                       First->SmokeConversationId() != Second->SmokeConversationId());
        Test->TestFalse(TEXT("Resolved model shown in first tab"), First->ActualModel.IsEmpty());
        CyCaptureWindow(CyMultiWindow, TEXT("cy-assistant-multichat.png"));
        CyMultiWorkspace->CloseChat(First);
        Test->TestEqual(TEXT("Closing one tab preserves the other"), CyMultiWorkspace->Count(), 1);
        CyMultiWindow->RequestDestroyWindow();
        CyMultiWindow.Reset();
        CyMultiWorkspace.Reset();
        CyMultiHost.Reset();
        return true;
    }
    if (FPlatformTime::Seconds() > CyMultiDeadline)
    {
        Test->AddError(TEXT("Multi-chat timed out: ") + First->SmokeDiagnostics() +
                       (Second ? Second->SmokeDiagnostics() : TEXT("No second tab")));
        CyMultiWindow->RequestDestroyWindow();
        CyMultiWindow.Reset();
        CyMultiWorkspace.Reset();
        CyMultiHost.Reset();
        return true;
    }
    return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyAssistantMultiTest, "CyUEMCP.Assistant.MultipleConversations",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyAssistantMultiTest::RunTest(const FString &)
{
    CyMultiHost = SNew(SCyPanel);
    CyMultiWorkspace = SNew(SCyChatWorkspace);
    CyMultiWorkspace->Initialize([](FName) {});
    CyMultiWindow = SNew(SWindow)
                        .Title(FText::FromString(TEXT("Cy Assistant")))
                        .ClientSize(FVector2D(720, 650))[CyMultiWorkspace.ToSharedRef()];
    FSlateApplication::Get().AddWindow(CyMultiWindow.ToSharedRef(), true);
    CyMultiPhase = 0;
    CyMultiRenderedAt = 0;
    CyMultiDeadline = FPlatformTime::Seconds() + 150;
    ADD_LATENT_AUTOMATION_COMMAND(FCyWaitForMultiChat(this));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
static TSharedPtr<SWindow> CyUnifiedWindow;
static int32 CyUnifiedPhase = 0;
static double CyUnifiedNext = 0, CyUnifiedDeadline = 0;
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyWaitForUnified, FAutomationTestBase *, Test);
bool FCyWaitForUnified::Update()
{
    auto &M = FModuleManager::GetModuleChecked<FCyUEAssistantModule>(TEXT("CyUEAssistant"));
    auto Chat = M.SmokeChat();
    if (FPlatformTime::Seconds() > CyUnifiedDeadline)
    {
        Test->AddError(TEXT("Unified Assistant UI did not become ready"));
        CyUnifiedWindow->RequestDestroyWindow();
        CyUnifiedWindow.Reset();
        return true;
    }
    if (!Chat->bPageReady || !Chat->SmokeConnected() || FPlatformTime::Seconds() < CyUnifiedNext)
        return false;
    if (CyUnifiedPhase == 0)
    {
        Test->TestTrue(TEXT("All nine connector presets visible"), M.SmokeProfiles() >= 9);
        for (auto Name : {TEXT("CyUEConnections"), TEXT("CyUELocalModel"), TEXT("CyUELogs")})
            Test->TestFalse(TEXT("No separate settings window spawner"),
                            FGlobalTabmanager::Get()->HasTabSpawner(FName(Name)));
        CyCaptureWindow(CyUnifiedWindow, TEXT("cy-assistant-unified-chat.png"));
        M.SmokePage(0);
    }
    else if (CyUnifiedPhase == 1)
    {
        CyCaptureWindow(CyUnifiedWindow, TEXT("cy-assistant-unified-settings.png"));
        M.SmokePage(1);
    }
    else if (CyUnifiedPhase == 2)
    {
        CyCaptureWindow(CyUnifiedWindow, TEXT("cy-assistant-unified-local.png"));
        M.SmokePage(2);
    }
    else if (CyUnifiedPhase == 3)
    {
        CyCaptureWindow(CyUnifiedWindow, TEXT("cy-assistant-unified-logs.png"));
        M.SmokePage(-1);
    }
    else
    {
        Test->TestTrue(TEXT("Settings retains the original conversation"), M.SmokeChat() == Chat);
        CyUnifiedWindow->RequestDestroyWindow();
        CyUnifiedWindow.Reset();
        return true;
    }
    ++CyUnifiedPhase;
    CyUnifiedNext = FPlatformTime::Seconds() + .7;
    return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyAssistantUnifiedTest, "CyUEMCP.Assistant.UnifiedSettings",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyAssistantUnifiedTest::RunTest(const FString &)
{
    auto &M = FModuleManager::GetModuleChecked<FCyUEAssistantModule>(TEXT("CyUEAssistant"));
    CyUnifiedWindow = SNew(SWindow)
                          .Title(FText::FromString(TEXT("Assistant")))
                          .ClientSize(FVector2D(920, 680))[M.SmokeRoot()];
    FSlateApplication::Get().AddWindow(CyUnifiedWindow.ToSharedRef(), true);
    CyUnifiedPhase = 0;
    CyUnifiedNext = 0;
    CyUnifiedDeadline = FPlatformTime::Seconds() + 60;
    ADD_LATENT_AUTOMATION_COMMAND(FCyWaitForUnified(this));
    return true;
}
#endif
