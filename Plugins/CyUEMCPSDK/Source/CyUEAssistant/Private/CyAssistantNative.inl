// Native profile settings. Included inside SCyPanel; transport remains in the existing controller.
struct FProfile
{
    FString Endpoint, KeyEnv;
    FString Id, Name, Provider, ModelId, Executable, Transport, Status = TEXT("Not checked"), Detail,
                                                                ConversationId;
    bool bStartup = true;
    FString Access = TEXT("read-only");
    TArray<TSharedPtr<FString>> Models;
};
struct FConnectorDefinition
{
    FString Id, Label, Provider, Transport, Description, Endpoint, KeyEnv;
    bool bStartup = true;
};
TArray<TSharedPtr<FConnectorDefinition>> Definitions;
TArray<TSharedPtr<FProfile>> Profiles;
TSharedPtr<SWidgetSwitcher> SettingsPages;
TSharedPtr<SComboBox<TSharedPtr<FString>>> ModelChoices;
TArray<TSharedPtr<FString>> AvailableModels;
TFunction<void()> NewConversation;
FString ActualModel, ActualConnector, ActualAccess, DraftAccess;
bool bDraftStartup = true;
bool bOwnBridge = true;
FString ConnectorLabel(TSharedPtr<FProfile> P) const
{
    if (!P)
        return TEXT("Assistant");
    for (auto D : Definitions)
        if (D->Provider == P->Provider && D->Transport == P->Transport)
            return D->Label;
    return P->Provider + TEXT(" / ") + (P->Transport.IsEmpty() ? TEXT("default transport") : P->Transport);
}
void LoadDefinitions()
{
    FString Folder =
        IPluginManager::Get().FindPlugin(TEXT("CyUEMCPSDK"))->GetBaseDir() / TEXT("Resources/Connectors");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*.json")), true, false);
    Files.Sort();
    for (auto File : Files)
    {
        FString Text;
        FFileHelper::LoadFileToString(Text, *(Folder / File));
        auto O = CyUEJson::Parse(Text);
        if (!O)
            continue;
        auto D = MakeShared<FConnectorDefinition>();
        O->TryGetStringField(TEXT("id"), D->Id);
        O->TryGetStringField(TEXT("label"), D->Label);
        O->TryGetStringField(TEXT("provider"), D->Provider);
        O->TryGetStringField(TEXT("transport"), D->Transport);
        O->TryGetStringField(TEXT("description"), D->Description);
        O->TryGetStringField(TEXT("endpoint"), D->Endpoint);
        O->TryGetStringField(TEXT("apiKeyEnv"), D->KeyEnv);
        O->TryGetBoolField(TEXT("checkOnStartup"), D->bStartup);
        Definitions.Add(D);
    }
}
void RefreshModels()
{
    AvailableModels.Empty();
    AvailableModels.Add(MakeShared<FString>(TEXT("Provider default")));
    if (Editing)
        for (auto M : Editing->Models)
            AvailableModels.Add(M);
    if (ModelChoices)
        ModelChoices->RefreshOptions();
}
TSharedPtr<FProfile> Selected, Checking;
TArray<TSharedPtr<FProfile>> CheckQueue;
TSharedPtr<FJsonObject> PendingSend;
TSharedPtr<SVerticalBox> ProfilesBox;
TSharedPtr<SComboBox<TSharedPtr<FProfile>>> ProfileCombo;
TSharedPtr<SEditableTextBox> ProfileName, ProviderField, ModelField, ExeField, TransportField, PortField,
    GPUField, EndpointField, KeyEnvField;
TSharedPtr<FProfile> Editing;
#if WITH_DEV_AUTOMATION_TESTS
TMap<FString, TFunction<void()>> NativeTestActions;
#endif
FString NativeStatus = TEXT("Starting bridge...");
TFunction<void(FName)> OpenNativeTab;
bool bStartupChecksQueued = false;

bool IsBusy() const
{
    return !Job.IsEmpty() || bSubmitting || PendingSend.IsValid();
}
FString Setting(const TCHAR *Key, const TCHAR *Default = TEXT("")) const
{
    FString V;
    GConfig->GetString(TEXT("CyUEMCP"), Key, V, GEditorPerProjectIni);
    return V.IsEmpty() ? FString(Default) : V;
}
void SaveProfiles()
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetStringField(TEXT("selected"), Selected ? Selected->Id : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (auto P : Profiles)
    {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("id"), P->Id);
        O->SetStringField(TEXT("name"), P->Name);
        O->SetStringField(TEXT("connector"), P->Provider);
        O->SetStringField(TEXT("model"), P->ModelId);
        O->SetStringField(TEXT("executable"), P->Executable);
        O->SetStringField(TEXT("transport"), P->Transport);
        O->SetBoolField(TEXT("startup"), P->bStartup);
        O->SetStringField(TEXT("access"), P->Access);
        O->SetStringField(TEXT("endpoint"), P->Endpoint);
        O->SetStringField(TEXT("api_key_env"), P->KeyEnv);
        TArray<TSharedPtr<FJsonValue>> Models;
        for (auto M : P->Models)
            Models.Add(MakeShared<FJsonValueString>(*M));
        O->SetArrayField(TEXT("models"), Models);
        Rows.Add(MakeShared<FJsonValueObject>(O));
    }
    Root->SetArrayField(TEXT("profiles"), Rows);
    GConfig->SetString(TEXT("CyUEMCP"), TEXT("ProfilesV1"), *CyUEJson::Stringify(Root), GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}
void LoadProfiles()
{
    Profiles.Empty();
    Selected.Reset();
    if (Definitions.IsEmpty())
        LoadDefinitions();
    auto Root = CyUEJson::Parse(Setting(TEXT("ProfilesV1")));
    FString Active;
    const TArray<TSharedPtr<FJsonValue>> *Rows = nullptr;
    if (Root && Root->TryGetArrayField(TEXT("profiles"), Rows))
    {
        Root->TryGetStringField(TEXT("selected"), Active);
        for (auto V : *Rows)
        {
            auto O = V->AsObject();
            if (!O)
                continue;
            auto P = MakeShared<FProfile>();
            O->TryGetStringField(TEXT("id"), P->Id);
            O->TryGetStringField(TEXT("name"), P->Name);
            O->TryGetStringField(TEXT("connector"), P->Provider);
            O->TryGetStringField(TEXT("model"), P->ModelId);
            O->TryGetStringField(TEXT("executable"), P->Executable);
            O->TryGetStringField(TEXT("transport"), P->Transport);
            O->TryGetBoolField(TEXT("startup"), P->bStartup);
            O->TryGetStringField(TEXT("access"), P->Access);
            O->TryGetStringField(TEXT("endpoint"), P->Endpoint);
            O->TryGetStringField(TEXT("api_key_env"), P->KeyEnv);
            const TArray<TSharedPtr<FJsonValue>> *Models = nullptr;
            if (O->TryGetArrayField(TEXT("models"), Models))
                for (auto M : *Models)
                    P->Models.Add(MakeShared<FString>(M->AsString()));
            if (P->Transport.IsEmpty())
                P->Transport = P->Provider == TEXT("codex")   ? TEXT("app-server")
                               : P->Provider == TEXT("local") ? TEXT("llama-http")
                                                              : TEXT("cli-jsonl");
            if (P->Name == TEXT("Default assistant"))
                P->Name = ConnectorLabel(P);
            if (P->Id.IsEmpty() || P->Provider.IsEmpty())
                continue;
            Profiles.Add(P);
            if (P->Id == Active)
                Selected = P;
        }
    }
    // Seed one usable preset per connector; preserve all user-created profiles.
    for (auto D : Definitions)
    {
        bool Found = false;
        for (auto P : Profiles)
            if (P->Provider == D->Provider && P->Transport == D->Transport)
                Found = true;
        if (!Found)
        {
            auto P = MakeShared<FProfile>();
            P->Id = D->Id;
            P->Name = D->Label;
            P->Provider = D->Provider;
            P->Transport = D->Transport;
            P->bStartup = D->bStartup;
            P->Endpoint = D->Endpoint;
            P->KeyEnv = D->KeyEnv;
            Profiles.Add(P);
        }
    }
    if (!Selected)
        for (auto P : Profiles)
            if (P->Provider == TEXT("codex") && P->Transport == TEXT("app-server"))
            {
                Selected = P;
                break;
            }
    if (!Selected && Profiles.Num())
        Selected = Profiles[0];
    if (bOwnBridge)
        SaveProfiles();
}

TSharedRef<FJsonObject> ProfileParams(TSharedPtr<FProfile> P)
{
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("connector"), P->Provider);
    O->SetStringField(TEXT("model"), P->ModelId);
    O->SetStringField(TEXT("executable"), P->Executable);
    O->SetStringField(TEXT("transport"), P->Transport);
    O->SetStringField(TEXT("access"), P->Access);
    O->SetStringField(TEXT("endpoint"), P->Endpoint);
    O->SetStringField(TEXT("api_key_env"), P->KeyEnv);
    return O;
}
void NotifyProfile()
{
    if (!bPageReady || !Selected)
        return;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("id"), Selected->Id);
    O->SetStringField(TEXT("name"), Selected->Name);
    O->SetStringField(TEXT("connector"),
                      ActualConnector.IsEmpty() ? ConnectorLabel(Selected) : ActualConnector);
    O->SetStringField(TEXT("model"),
                      ActualModel.IsEmpty()
                          ? (Selected->ModelId.IsEmpty() ? TEXT("Provider default") : Selected->ModelId)
                          : ActualModel);
    O->SetStringField(TEXT("access"), ActualAccess.IsEmpty() ? Selected->Access : ActualAccess);
    Send(TEXT("profile"), CyUEJson::Stringify(O));
}
void SelectProfile(TSharedPtr<FProfile> P)
{
    if (!P || P == Selected)
        return;
    if (IsBusy() || !Conversation.IsEmpty())
    {
        Append(TEXT("Choose a profile in a new conversation."));
        return;
    }
    Selected = P;
    ActualModel.Empty();
    ActualConnector.Empty();
    ActualAccess.Empty();
    NotifyProfile();
}
void QueueCheck(TSharedPtr<FProfile> P)
{
    if (P && !CheckQueue.Contains(P))
    {
        P->Status = TEXT("Queued");
        CheckQueue.Add(P);
    }
    if (Session.IsEmpty())
        Start();
}
void PumpAssistant()
{
    if (Session.IsEmpty() || !Job.IsEmpty() || bSubmitting)
        return;
    if (PendingSend)
    {
        if (Conversation.IsEmpty())
        {
            NativeStatus = TEXT("Opening conversation...");
            ActualConnector = ConnectorLabel(Selected);
            ActualAccess = Selected->Access;
            ActualModel = Selected->ModelId;
            NotifyProfile();
            Submit(TEXT("assistant_open"), ProfileParams(Selected));
        }
        else
        {
            auto P = PendingSend.ToSharedRef();
            PendingSend.Reset();
            P->SetStringField(TEXT("conversationId"), Conversation);
            NativeStatus = TEXT("Waiting for assistant...");
            Submit(TEXT("assistant_send"), P);
        }
        return;
    }
    if (CheckQueue.Num())
    {
        Checking = CheckQueue[0];
        CheckQueue.RemoveAt(0);
        Checking->Status = TEXT("Checking...");
        auto P = ProfileParams(Checking);
        P->RemoveField(TEXT("access"));
        P->SetNumberField(TEXT("timeout"), 20);
        Submit(TEXT("assistant_probe"), P);
    }
}
void AssistantFailure(const FString &Error)
{
    PendingSend.Reset();
    NativeStatus = Error;
    if (Checking)
    {
        Checking->Status = TEXT("Failed");
        Checking->Detail = Error;
        Checking.Reset();
    }
    Send(TEXT("error"), Error);
    Append(Error);
}
TSharedRef<SWidget> NativeButton(const TCHAR *Label, TFunction<void()> Fn)
{
#if WITH_DEV_AUTOMATION_TESTS
    NativeTestActions.Add(Label, Fn);
#endif
    return SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda([Fn] {
        Fn();
        return FReply::Handled();
    });
}
void AddField(TSharedRef<SVerticalBox> Box, const TCHAR *Label, TSharedPtr<SEditableTextBox> &Field,
              const FString &Value, bool bBrowse = false)
{
    Box->AddSlot().AutoHeight().Padding(0, 6, 0, 2)[SNew(STextBlock).Text(FText::FromString(Label))];
    Field = SNew(SEditableTextBox).Text(FText::FromString(Value));
    auto Row = SNew(SHorizontalBox);
    Row->AddSlot().FillWidth(1)[Field.ToSharedRef()];
    if (bBrowse)
        Row->AddSlot().AutoWidth().Padding(4, 0)[NativeButton(TEXT("Browse"), [Field] {
            TArray<FString> Files;
            auto *Desktop = FDesktopPlatformModule::Get();
            if (Desktop &&
                Desktop->OpenFileDialog(nullptr, TEXT("Select file"), TEXT(""), TEXT(""),
                                        TEXT("All files|*.*"), 0, Files) &&
                Files.Num())
                Field->SetText(FText::FromString(Files[0]));
        })];
    Box->AddSlot().AutoHeight()[Row];
}
void EditProfile(TSharedPtr<FProfile> P)
{
    Editing = P;
    DraftAccess = P->Access;
    bDraftStartup = P->bStartup;
    ProfileName->SetText(FText::FromString(P->Name));
    ProviderField->SetText(FText::FromString(P->Provider));
    ModelField->SetText(FText::FromString(P->ModelId));
    ExeField->SetText(FText::FromString(P->Executable));
    TransportField->SetText(FText::FromString(P->Transport));
    EndpointField->SetText(FText::FromString(P->Endpoint));
    KeyEnvField->SetText(FText::FromString(P->KeyEnv));
    RefreshModels();
}
void RefreshProfiles()
{
    if (ProfileCombo)
        ProfileCombo->RefreshOptions();
    if (!ProfilesBox)
        return;
    ProfilesBox->ClearChildren();
    for (auto P : Profiles)
    {
        ProfilesBox->AddSlot().AutoHeight().Padding(0, 4)[SNew(SButton).OnClicked_Lambda([this, P] {
            EditProfile(P);
            return FReply::Handled();
        })[SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(P->Name))] +
           SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                                 .Text_Lambda([this, P] {
                                                     return FText::FromString(ConnectorLabel(P) +
                                                                              TEXT(" | ") + P->Status);
                                                 })
                                                 .AutoWrapText(true)]]];
    }
}
void SaveEditedProfile(bool bTest)
{
    if (!Editing)
        return;
    auto P = Editing;
    P->Name = ProfileName->GetText().ToString();
    P->Provider = ProviderField->GetText().ToString();
    P->ModelId = ModelField->GetText().ToString();
    P->Executable = ExeField->GetText().ToString();
    P->Transport = TransportField->GetText().ToString();
    P->Access = DraftAccess;
    P->bStartup = bDraftStartup;
    P->Endpoint = EndpointField->GetText().ToString();
    P->KeyEnv = KeyEnvField->GetText().ToString();
    SaveProfiles();
    RefreshProfiles();
    if (bTest)
        QueueCheck(P);
    NotifyProfile();
}
TSharedRef<SWidget> ConnectionsWidget()
{
    auto Basic = SNew(SVerticalBox), Rights = SNew(SVerticalBox), Advanced = SNew(SVerticalBox);
    ProviderField = SNew(SEditableTextBox);
    TransportField = SNew(SEditableTextBox);
    ModelField = SNew(SEditableTextBox);
    auto ProfileList = SNew(SVerticalBox);
    ProfileList->AddSlot().AutoHeight().Padding(
        0, 0, 0, 8)[SNew(STextBlock).Text(FText::FromString(TEXT("PROVIDERS")))];
    ProfileList->AddSlot().AutoHeight()[SAssignNew(ProfilesBox, SVerticalBox)];
    ProfileList->AddSlot().AutoHeight().Padding(0, 8)[NativeButton(TEXT("+ New profile"), [this] {
        auto P = MakeShared<FProfile>();
        P->Id = FGuid::NewGuid().ToString();
        P->Provider = Editing->Provider;
        P->Transport = Editing->Transport;
        P->Name = ConnectorLabel(Editing) + TEXT(" - Custom");
        P->ModelId = Editing->ModelId;
        P->Models = Editing->Models;
        P->Endpoint = Editing->Endpoint;
        P->KeyEnv = Editing->KeyEnv;
        Profiles.Add(P);
        EditProfile(P);
        SaveProfiles();
        RefreshProfiles();
    })];
    ProfileList->AddSlot().AutoHeight()[NativeButton(TEXT("Detect / test all"), [this] {
        for (auto P : Profiles)
            QueueCheck(P);
    })];
    ProfileList->AddSlot().AutoHeight().Padding(0, 8)[NativeButton(TEXT("Delete custom profile"), [this] {
        if (!Editing || Checking == Editing)
        {
            Append(TEXT("Wait for this profile's check to finish."));
            return;
        }
        for (auto D : Definitions)
            if (D->Id == Editing->Id)
            {
                Append(TEXT("Built-in profiles are kept as defaults. Duplicate one to customize it."));
                return;
            }
        CheckQueue.Remove(Editing);
        auto P = Editing;
        Profiles.Remove(P);
        if (Selected == P)
            Selected = Profiles[0];
        EditProfile(Selected);
        SaveProfiles();
        RefreshProfiles();
    })];

    Basic->AddSlot().AutoHeight().Padding(
        0, 0, 0,
        12)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(ConnectorLabel(Editing)); })];
    AddField(Basic, TEXT("Profile name"), ProfileName, TEXT(""));
    auto HttpFields = SNew(SVerticalBox).Visibility_Lambda([this] {
        return Editing && (Editing->Provider == TEXT("ollama") || Editing->Provider == TEXT("openrouter") ||
                           Editing->Provider == TEXT("anthropic"))
                   ? EVisibility::Visible
                   : EVisibility::Collapsed;
    });
    AddField(HttpFields, TEXT("Server URL"), EndpointField, TEXT(""));
    AddField(HttpFields, TEXT("API key environment variable (name only)"), KeyEnvField, TEXT(""));
    HttpFields->AddSlot().AutoHeight().Padding(
        0, 6)[SNew(STextBlock)
                  .AutoWrapText(true)
                  .Text(FText::FromString(
                      TEXT("Ollama needs no key by default. Cloud APIs use the named environment variable; "
                           "never paste a key here. Restart Unreal after changing environment variables. "
                           "Choose a model after testing.")))];
    Basic->AddSlot().AutoHeight()[HttpFields];
    Basic->AddSlot().AutoHeight().Padding(0, 12, 0,
                                          4)[SNew(STextBlock).Text(FText::FromString(TEXT("Model")))];
    Basic->AddSlot()
        .AutoHeight()[SAssignNew(ModelChoices, SComboBox<TSharedPtr<FString>>)
                          .OptionsSource(&AvailableModels)
                          .OnGenerateWidget_Lambda([](TSharedPtr<FString> M) {
                              return SNew(STextBlock).Text(FText::FromString(*M));
                          })
                          .OnSelectionChanged_Lambda([this](TSharedPtr<FString> M, ESelectInfo::Type) {
                              if (M)
                                  ModelField->SetText(
                                      FText::FromString(*M == TEXT("Provider default") ? TEXT("") : *M));
                          })[SNew(STextBlock).Text_Lambda([this] {
                              FString M = ModelField->GetText().ToString();
                              return FText::FromString(M.IsEmpty() ? TEXT("Provider default") : M);
                          })]];
    Basic->AddSlot().AutoHeight().Padding(
        0, 8)[SNew(STextBlock)
                  .AutoWrapText(true)
                  .Text(FText::FromString(TEXT("Installed connectors and models are detected automatically. "
                                               "Use Advanced for a custom model ID or executable.")))];
    Basic->AddSlot().AutoHeight().Padding(
        0, 8)[SNew(SCheckBox)
                  .IsChecked_Lambda(
                      [this] { return bDraftStartup ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                  .OnCheckStateChanged_Lambda([this](ECheckBoxState V) {
                      bDraftStartup = V == ECheckBoxState::Checked;
                  })[SNew(STextBlock).Text(FText::FromString(TEXT("Check provider on launch")))]];
    Basic->AddSlot().AutoHeight().Padding(
        0, 8)[SNew(SHorizontalBox) +
              SHorizontalBox::Slot()
                  .AutoWidth()[NativeButton(TEXT("Save profile"), [this] { SaveEditedProfile(false); })] +
              SHorizontalBox::Slot().AutoWidth().Padding(
                  8, 0)[NativeButton(TEXT("Test connection"), [this] { SaveEditedProfile(true); })]];
    Basic->AddSlot().AutoHeight().Padding(0, 12)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
        return FText::FromString(Editing ? Editing->Status + TEXT("\n") + Editing->Detail : TEXT(""));
    })];
    Basic->AddSlot()
        .AutoHeight()[SNew(STextBlock)
                          .AutoWrapText(true)
                          .Text(FText::FromString(TEXT("Changes apply to new conversations. Each chat keeps "
                                                       "the connection and permissions it started with.")))];
    Rights->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[SNew(STextBlock).Text_Lambda([this] {
        return FText::FromString(TEXT("Permissions - ") + (Editing ? Editing->Name : TEXT("")));
    })];
    Rights->AddSlot()
        .AutoHeight()[SNew(SCheckBox)
                          .IsEnabled_Lambda([this] {
                              return Editing && (Editing->Provider == TEXT("codex") ||
                                                 Editing->Provider == TEXT("claude-code"));
                          })
                          .IsChecked_Lambda([this] {
                              return DraftAccess == TEXT("workspace-write") ? ECheckBoxState::Checked
                                                                            : ECheckBoxState::Unchecked;
                          })
                          .OnCheckStateChanged_Lambda([this](ECheckBoxState V) {
                              DraftAccess =
                                  V == ECheckBoxState::Checked ? TEXT("workspace-write") : TEXT("read-only");
                          })[SNew(STextBlock).Text(FText::FromString(TEXT("Allow project file editing")))]];
    Rights->AddSlot().AutoHeight().Padding(
        0, 8)[SNew(STextBlock)
                  .AutoWrapText(true)
                  .Text(FText::FromString(
                      TEXT("Off: read-only. Codex uses its read-only / workspace-write sandbox. Claude uses "
                           "Read/Glob/Grep, with Edit/Write added only when enabled; shell and web tools "
                           "remain disabled. Local models have no file tools in this adapter.")))];
    Rights->AddSlot()
        .AutoHeight()[NativeButton(TEXT("Save permissions"), [this] { SaveEditedProfile(false); })];
    Rights->AddSlot().AutoHeight().Padding(
        0, 24, 0, 8)[SNew(STextBlock).Text(FText::FromString(TEXT("UNREAL MCP - THIS EDITOR")))];
    Rights->AddSlot().AutoHeight()
        [SNew(SCheckBox)
             .IsChecked_Lambda([] {
                 return FCyUEBridgeModule::Get().bAllowWrites ? ECheckBoxState::Checked
                                                              : ECheckBoxState::Unchecked;
             })
             .OnCheckStateChanged_Lambda([](ECheckBoxState V) {
                 FCyUEBridgeModule::Get().bAllowWrites = V == ECheckBoxState::Checked;
             })[SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Allow registered editor modifications via MCP")))]];
    Rights->AddSlot().AutoHeight().Padding(
        0,
        8)[SNew(STextBlock)
               .AutoWrapText(true)
               .Text(FText::FromString(TEXT("Applies to external MCP clients for this editor session. The "
                                            "embedded chat does not automatically receive Unreal tools.")))];
    AddField(Advanced, TEXT("Custom model ID"), ModelField, TEXT(""));
    AddField(Advanced, TEXT("Executable override - leave empty for automatic detection"), ExeField, TEXT(""),
             true);
    AddField(Advanced, TEXT("Transport override"), TransportField, TEXT(""));
    Advanced->AddSlot().AutoHeight().Padding(
        0, 8)[NativeButton(TEXT("Save advanced settings"), [this] { SaveEditedProfile(false); })];
    Advanced->AddSlot().AutoHeight().Padding(
        0, 20, 0, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("Shared local bridge")))];
    AddField(Advanced, TEXT("Python executable"), Python, Python->GetText().ToString(), true);
    AddField(Advanced, TEXT("Bridge script"), Script, Script->GetText().ToString(), true);
    Advanced->AddSlot().AutoHeight().Padding(0, 8)[NativeButton(TEXT("Save bridge / connect"), [this] {
        GConfig->SetString(TEXT("CyUEMCP"), TEXT("Python"), *Python->GetText().ToString(),
                           GEditorPerProjectIni);
        GConfig->SetString(TEXT("CyUEMCP"), TEXT("Script"), *Script->GetText().ToString(),
                           GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
        Start();
    })];
    auto Body = SNew(SVerticalBox);
    auto Tabs = SNew(SHorizontalBox);
    int I = 0;
    for (auto Label : {TEXT("Connection"), TEXT("Permissions"), TEXT("Advanced")})
    {
        int Index = I++;
        Tabs->AddSlot().AutoWidth()[SNew(SCyChromeTab)
                                        .Label(FText::FromString(Label))
                                        .Width(125)
                                        .Active_Lambda([this, Index] {
                                            return SettingsPages &&
                                                   SettingsPages->GetActiveWidgetIndex() == Index;
                                        })
                                        .OnSelected(FSimpleDelegate::CreateLambda(
                                            [this, Index] { SettingsPages->SetActiveWidgetIndex(Index); }))];
    }
    Body->AddSlot().AutoHeight().Padding(0, 0, 0, 16)[Tabs];
    Body->AddSlot().FillHeight(1)[SAssignNew(SettingsPages, SWidgetSwitcher) +
                                  SWidgetSwitcher::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[Basic]] +
                                  SWidgetSwitcher::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[Rights]] +
                                  SWidgetSwitcher::Slot()[SNew(SScrollBox) + SScrollBox::Slot()[Advanced]]];
    // Local engine setup belongs to the local provider, not a separate settings window.
    Basic->AddSlot().AutoHeight().Padding(0, 16)[SNew(SBox).Visibility_Lambda([this] {
        return Editing && Editing->Provider == TEXT("local") ? EVisibility::Visible : EVisibility::Collapsed;
    })[LocalWidget()]];
    EditProfile(Selected);
    RefreshProfiles();
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               12)[SNew(SBox).WidthOverride(240)[SNew(SScrollBox) + SScrollBox::Slot()[ProfileList]]] +
           SHorizontalBox::Slot().FillWidth(1).Padding(12)[Body];
}
TSharedRef<SWidget> LocalWidget()
{
    auto Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()
        [SNew(STextBlock)
             .AutoWrapText(true)
             .Text(FText::FromString(
                 TEXT("Optional llama.cpp engine and GGUF. Use existing files; models are not copied. Check "
                      "their licenses before downloading. CPU: 0 GPU layers. Context: 4096 tokens.")))];
    AddField(Box, TEXT("llama-server executable"), LocalServer, Setting(TEXT("server")), true);
    AddField(Box, TEXT("GGUF model"), LocalModel, Setting(TEXT("gguf")), true);
    AddField(Box, TEXT("Loopback port"), PortField, Setting(TEXT("port"), TEXT("39299")));
    AddField(Box, TEXT("GPU layers"), GPUField, Setting(TEXT("gpu"), TEXT("0")));
    Box->AddSlot().AutoHeight().Padding(0, 8)[NativeButton(TEXT("Save / start local model"), [this] {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("action"), TEXT("localStart"));
        O->SetStringField(TEXT("server"), LocalServer->GetText().ToString());
        O->SetStringField(TEXT("gguf"), LocalModel->GetText().ToString());
        O->SetStringField(TEXT("port"), PortField->GetText().ToString());
        O->SetStringField(TEXT("gpu"), GPUField->GetText().ToString());
        for (auto K : {TEXT("server"), TEXT("gguf"), TEXT("port"), TEXT("gpu")})
            GConfig->SetString(TEXT("CyUEMCP"), K, *O->GetStringField(K), GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
        Action(CyUEJson::Stringify(O));
    })];
    Box->AddSlot().AutoHeight()[NativeButton(TEXT("Stop local model"),
                                             [this] { Action(TEXT("{\"action\":\"localStop\"}")); })];
    Box->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
        return FText::FromString(NativeStatus);
    })];
    return SNew(SScrollBox) + SScrollBox::Slot().Padding(10)[Box];
}
TSharedRef<SWidget> LogsWidget()
{
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               6)[SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().AutoWidth()[NativeButton(
                      TEXT("Copy all"), [this] { FPlatformApplicationMisc::ClipboardCopy(*Log); })] +
                  SHorizontalBox::Slot().AutoWidth()[NativeButton(
                      TEXT("Export log"), [this] { Action(TEXT("{\"action\":\"exportLog\"}")); })]] +
           SVerticalBox::Slot().FillHeight(1).Padding(6)
               [SAssignNew(Output, SMultiLineEditableTextBox).IsReadOnly(true).Text(FText::FromString(Log))];
}
TSharedRef<SWidget> ChatToolbar()
{
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(
               1)[SAssignNew(ProfileCombo, SComboBox<TSharedPtr<FProfile>>)
                      .OptionsSource(&Profiles)
                      .IsEnabled_Lambda([this] { return !IsBusy() && Conversation.IsEmpty(); })
                      .OnComboBoxOpening_Lambda([this] {
                          FString Id = Selected->Id;
                          LoadProfiles();
                          for (auto P : Profiles)
                              if (P->Id == Id)
                                  Selected = P;
                          ProfileCombo->RefreshOptions();
                          NotifyProfile();
                      })
                      .OnGenerateWidget_Lambda([this](TSharedPtr<FProfile> P) {
                          return SNew(STextBlock)
                              .Text(FText::FromString(P->Name + TEXT(" - ") + ConnectorLabel(P)));
                      })
                      .OnSelectionChanged_Lambda([this](TSharedPtr<FProfile> P, ESelectInfo::Type) {
                          SelectProfile(P);
                      })[SNew(STextBlock).Text_Lambda([this] {
                          return FText::FromString(Selected->Name == ConnectorLabel(Selected)
                                                       ? Selected->Name
                                                       : Selected->Name + TEXT(" | ") +
                                                             ConnectorLabel(Selected));
                      })]];
}
