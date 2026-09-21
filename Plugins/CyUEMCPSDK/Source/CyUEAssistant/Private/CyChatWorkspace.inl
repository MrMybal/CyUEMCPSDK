// Each conversation owns its browser, SDK session and provider process. The settings
// controller owns the single editor companion; closing a tab finishes only that task.
class SCyChatWorkspace : public SCompoundWidget
{
    TArray<TSharedPtr<SCyPanel>> Chats;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<SHorizontalBox> Tabs;
    TFunction<void(FName)> OpenSettings;
    int32 NextNumber = 1;
    TMap<SCyPanel *, int32> Numbers;

  public:
    TFunction<void(const FString &)> LogSink;
    SLATE_BEGIN_ARGS(SCyChatWorkspace)
    {
    }
    SLATE_END_ARGS()
    void Construct(const FArguments &)
    {
        ChildSlot[SNew(SVerticalBox) +
                  SVerticalBox::Slot().AutoHeight().Padding(
                      4)[SNew(SScrollBox).Orientation(Orient_Horizontal) +
                         SScrollBox::Slot()[SAssignNew(Tabs, SHorizontalBox)]] +
                  SVerticalBox::Slot().FillHeight(1)[SAssignNew(Pages, SWidgetSwitcher)]];
    }
    void Initialize(TFunction<void(FName)> Open)
    {
        OpenSettings = Open;
        AddChat();
    }
    int32 Count() const
    {
        return Chats.Num();
    }
    TSharedPtr<SCyPanel> Chat(int32 Index)
    {
        return Chats.IsValidIndex(Index) ? Chats[Index] : nullptr;
    }
    void AddChat()
    {
        auto P = SNew(SCyPanel).OwnBridge(false);
        P->OpenNativeTab = OpenSettings;
        TWeakPtr<SCyChatWorkspace> Weak = SharedThis(this);
        int32 Number = NextNumber;
        P->ForwardLog = [Weak, Number](const FString &Text) {
            if (auto W = Weak.Pin())
                if (W->LogSink)
                    W->LogSink(FString::Printf(TEXT("[Chat %d] "), Number) + Text);
        };
        P->NewConversation = [Weak] {
            if (auto Self = Weak.Pin())
                Self->AddChat();
        };
        Chats.Add(P);
        Numbers.Add(&P.Get(), NextNumber++);
        Pages->AddSlot()[P];
        Pages->SetActiveWidgetIndex(Chats.Num() - 1);
        RefreshTabs();
    }
    void ActivateChat(TSharedPtr<SCyPanel> P)
    {
        Pages->SetActiveWidget(P.ToSharedRef());
    }
    void CloseChat(TSharedPtr<SCyPanel> P)
    {
        int32 Index = Chats.IndexOfByKey(P);
        if (Index == INDEX_NONE)
            return;
        Pages->RemoveSlot(P.ToSharedRef());
        Chats.RemoveAt(Index);
        Numbers.Remove(P.Get());
        if (Chats.IsEmpty())
            AddChat();
        else
        {
            Pages->SetActiveWidgetIndex(FMath::Min(Index, Chats.Num() - 1));
            RefreshTabs();
        }
    }
    void RefreshTabs()
    {
        Tabs->ClearChildren();
        for (auto P : Chats)
        {
            const int32 Number = Numbers[P.Get()];
            Tabs->AddSlot().AutoWidth().Padding(
                0, 0, 2, 0)[SNew(SCyChromeTab)
                                .Label_Lambda([P, Number] {
                                    return FText::FromString(FString::Printf(
                                        TEXT("Chat %d%s"), Number, P->IsBusy() ? TEXT(" ...") : TEXT("")));
                                })
                                .Active_Lambda([this, P] { return Pages->GetActiveWidget() == P; })
                                .OnSelected(FSimpleDelegate::CreateLambda([this, P] { ActivateChat(P); }))
                                .OnClosed(FSimpleDelegate::CreateLambda([this, P] { CloseChat(P); }))];
        }
        Tabs->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4, 0)[SNew(SBox).WidthOverride(26).HeightOverride(
                26)[SNew(SButton)
                        .ButtonStyle(&FCyTabAppearance::Get().Icon)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ToolTipText(FText::FromString(TEXT("New conversation")))
                        .OnClicked_Lambda([this] {
                            AddChat();
                            return FReply::Handled();
                        })[SNew(STextBlock).Text(FText::FromString(TEXT("+")))]]];
    }
};
