// One bootstrap controller per embedded integration. Only immutable files are shared on disk.
struct FCyAssistantRuntime
{
    FString Mode = TEXT("Shared"), State, Message = TEXT("Not checked"), Root, Python, Script, Report, CancelFile;
#if WITH_DEV_AUTOMATION_TESTS
    FString TestStorageRoot;
#endif
    int32 Progress = 0;
    bool bReady = false, bAsked = false;
    FProcHandle Process;
    TWeakPtr<SWindow> Dialog;

    bool Running() const { return Process.IsValid(); }
    FString PluginRoot() const { return IPluginManager::Get().FindPlugin(TEXT("CyUEMCPSDK"))->GetBaseDir(); }
    bool Custom() const
    {
        bool Value = false;
        GConfig->GetBool(TEXT("CyUEMCP"), TEXT("UseCustomRuntime"), Value, GEditorPerProjectIni);
        return Value;
    }
    void SelectMode(const FString &Value)
    {
        Mode = Value;
        bReady = false;
        State.Empty();
        Message = TEXT("Not checked");
        GConfig->SetString(TEXT("CyUEMCP"), TEXT("RuntimeStorage"), *Mode, GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
    }
    void Launch(const TCHAR *Action)
    {
        if (Running()) return;
        bReady = false;
        FString Folder = FPlatformProcess::UserSettingsDir();
        Folder /= TEXT("CyTools/UEAssistant/setup");
        IFileManager::Get().MakeDirectory(*Folder, true);
        Report = Folder / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
        CancelFile = Report + TEXT(".cancel");
        FString Setup = FPaths::ConvertRelativePathToFull(PluginRoot() / TEXT("Resources/AssistantRuntime/SetupRuntime.ps1"));
        FString SystemRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"));
        FString Exe = SystemRoot / TEXT("System32/WindowsPowerShell/v1.0/powershell.exe");
        FString Base = FPaths::ConvertRelativePathToFull(PluginRoot());
        if (!FPaths::FileExists(Setup) || !FPaths::FileExists(Exe) || Base.Contains(TEXT("\"")))
        {
            State = TEXT("Error");
            Message = TEXT("The offline runtime installer is missing from this plugin package.");
            return;
        }
        GConfig->GetString(TEXT("CyUEMCP"), TEXT("RuntimeStorage"), Mode, GEditorPerProjectIni);
        if (Mode != TEXT("Local")) Mode = TEXT("Shared");
        FString Args = FString::Printf(TEXT("-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%s\" -Action %s -Mode %s -PluginRoot \"%s\" -Report \"%s\" -CancelFile \"%s\""), *Setup, Action, *Mode, *Base, *Report, *CancelFile);
#if WITH_DEV_AUTOMATION_TESTS
        if (!TestStorageRoot.IsEmpty()) Args += FString::Printf(TEXT(" -StorageRoot \"%s\""), *TestStorageRoot);
#endif
        State = FString(Action) == TEXT("Install") ? TEXT("Installing") : TEXT("Checking");
        Message = State == TEXT("Installing") ? TEXT("Installing the offline assistant runtime...") : TEXT("Checking the assistant runtime...");
        Progress = 0;
        Process = FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, 0, *Base, nullptr);
        if (!Process.IsValid()) { State = TEXT("Error"); Message = TEXT("Cannot start the offline runtime installer."); }
    }
    void Install()
    {
        bAsked = true;
        GConfig->SetBool(TEXT("CyUEMCP"), TEXT("UseCustomRuntime"), false, GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
        Launch(TEXT("Install"));
    }
    void Cancel()
    {
        if (Running()) FFileHelper::SaveStringToFile(TEXT("cancel"), *CancelFile);
    }
    bool Poll()
    {
        if (!Running()) return false;
        FString Text;
        if (FFileHelper::LoadFileToString(Text, *Report))
        {
            auto O = CyUEJson::Parse(Text);
            if (O)
            {
                O->TryGetStringField(TEXT("state"), State);
                O->TryGetStringField(TEXT("message"), Message);
                O->TryGetStringField(TEXT("root"), Root);
                double Number = 0;
                O->TryGetNumberField(TEXT("progress"), Number);
                Progress = int32(Number);
                if (State == TEXT("Ready"))
                {
                    O->TryGetStringField(TEXT("python"), Python);
                    O->TryGetStringField(TEXT("script"), Script);
                }
            }
        }
        if (FPlatformProcess::IsProcRunning(Process)) return false;
        int32 Code = 1;
        FPlatformProcess::GetProcReturnCode(Process, &Code);
        FPlatformProcess::CloseProc(Process);
        Process.Reset();
        bReady = Code == 0 && State == TEXT("Ready") && FPaths::FileExists(Python) && FPaths::FileExists(Script);
        IFileManager::Get().Delete(*Report);
        IFileManager::Get().Delete(*CancelFile);
        if (!bReady && State != TEXT("Missing") && State != TEXT("Cancelled") && State != TEXT("Error"))
        { State = TEXT("Error"); Message = TEXT("The runtime check did not complete. Retry from Settings > Runtime."); }
        if (bReady)
        {
            if (auto Window = Dialog.Pin()) Window->RequestDestroyWindow();
            Dialog.Reset();
        }
        return true;
    }
    TSharedRef<SWidget> Widget()
    {
        auto Body = SNew(SVerticalBox);
        Body->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
            [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("The optional assistant needs Python and its dependencies. Install the included offline archive once; compatible plugins reuse it. No account or model is installed.")))];
        Body->AddSlot().AutoHeight().Padding(0, 4)
            [SNew(SButton).IsEnabled_Lambda([this] { return !Running(); })
                .Visibility_Lambda([this] { return Custom() ? EVisibility::Visible : EVisibility::Collapsed; })
                .Text(FText::FromString(TEXT("Switch to packaged runtime")))
                .OnClicked_Lambda([this] {
                    GConfig->SetBool(TEXT("CyUEMCP"), TEXT("UseCustomRuntime"), false, GEditorPerProjectIni);
                    GConfig->Flush(false, GEditorPerProjectIni);
                    Launch(TEXT("Check"));
                    return FReply::Handled();
                })];
        for (const auto &Choice : {FString(TEXT("Shared")), FString(TEXT("Local"))})
        {
            Body->AddSlot().AutoHeight().Padding(0, 3)
                [SNew(SCheckBox).IsEnabled_Lambda([this] { return !Running(); })
                    .IsChecked_Lambda([this, Choice] { return Mode == Choice ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this, Choice](ECheckBoxState Checked) { if (Checked == ECheckBoxState::Checked) SelectMode(Choice); })
                    [SNew(STextBlock).Text(FText::FromString(Choice == TEXT("Shared") ? TEXT("Shared user folder (recommended)") : TEXT("This plugin folder (requires write access)")))]];
        }
        Body->AddSlot().AutoHeight().Padding(0, 8)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
                FString Path = Mode == TEXT("Shared") ? FString(FPlatformProcess::UserSettingsDir()) / TEXT("CyTools/UEAssistant/runtimes") : PluginRoot() / TEXT("Runtime/Assistant");
                return FText::FromString(TEXT("Location: ") + Path);
            })];
        FString Data;
        if (FFileHelper::LoadFileToString(Data, *(PluginRoot() / TEXT("Resources/AssistantRuntime/bundle.json"))))
        {
            auto M = CyUEJson::Parse(Data);
            if (M)
            {
                double Size = 0;
                M->TryGetNumberField(TEXT("extractedBytes"), Size);
                Body->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Installed size: %.1f MB. Python and component licenses are included."), Size / 1000000.0)))];
            }
        }
        Body->AddSlot().AutoHeight().Padding(0, 10)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Message); })];
        Body->AddSlot().AutoHeight().Padding(0, 2)
            [SNew(SProgressBar).Visibility_Lambda([this] { return Running() ? EVisibility::Visible : EVisibility::Collapsed; })
                .Percent_Lambda([this] { return TOptional<float>(Progress / 100.0f); })];
        Body->AddSlot().AutoHeight().Padding(0, 12)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [SNew(SButton).Text(FText::FromString(TEXT("Install / repair")))
                    .IsEnabled_Lambda([this] { return !Running() && !bReady; })
                    .OnClicked_Lambda([this] {
                        Install(); return FReply::Handled();
                    })]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [SNew(SButton).Text(FText::FromString(TEXT("Check again")))
                    .IsEnabled_Lambda([this] { return !Running(); })
                    .OnClicked_Lambda([this] { Launch(TEXT("Check")); return FReply::Handled(); })]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(FText::FromString(TEXT("Cancel")))
                    .IsEnabled_Lambda([this] { return Running(); })
                    .OnClicked_Lambda([this] { Cancel(); return FReply::Handled(); })]];
        Body->AddSlot().AutoHeight().Padding(0, 4)
            [SNew(SButton).Text(FText::FromString(TEXT("Component licenses")))
                .OnClicked_Lambda([this] {
                    FPlatformProcess::LaunchFileInDefaultExternalApplication(*(PluginRoot() / TEXT("Resources/AssistantRuntime/Licenses.txt")));
                    return FReply::Handled();
                })];
        return SNew(SScrollBox) + SScrollBox::Slot().Padding(12)[Body];
    }
    void Offer()
    {
        if (bAsked || IsRunningCommandlet() || FApp::IsUnattended()) return;
        bAsked = true;
        auto Window = SNew(SWindow).Title(FText::FromString(TEXT("Set up the optional assistant")))
            .ClientSize(FVector2D(570, 380)).SupportsMaximize(false).SupportsMinimize(false);
        Dialog = Window;
        Window->SetContent(SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1)[Widget()]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12)
            [SNew(SButton).Text(FText::FromString(TEXT("Later"))).OnClicked_Lambda([this] {
                if (auto W = Dialog.Pin()) W->RequestDestroyWindow();
                return FReply::Handled();
            })]);
        FSlateApplication::Get().AddWindow(Window);
    }
    void Shutdown()
    {
        Cancel(); // Cooperative extraction cancellation; never kill another plugin's process.
        if (Running()) FPlatformProcess::CloseProc(Process);
        Process.Reset();
        if (auto Window = Dialog.Pin()) Window->RequestDestroyWindow();
        Dialog.Reset();
    }
};
static FCyAssistantRuntime CyAssistantRuntime;
