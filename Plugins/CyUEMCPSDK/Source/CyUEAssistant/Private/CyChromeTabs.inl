// Compact native tabs: one continuous shape, integrated close, no button bevels.
struct FCyTabAppearance
{
    FSlateRoundedBoxBrush Active{FLinearColor(.06f, .065f, .075f), FVector4(9, 9, 0, 0)};
    FSlateRoundedBoxBrush Hover{FLinearColor(.035f, .038f, .045f), FVector4(9, 9, 0, 0)};
    FSlateBrush Empty;
    FButtonStyle Icon;
    FCyTabAppearance()
    {
        Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
        Icon.SetNormal(Empty)
            .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.09f, .10f, .12f), 12.f))
            .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.13f, .14f, .17f), 12.f))
            .SetNormalPadding(FMargin(0))
            .SetPressedPadding(FMargin(0));
    }
    static const FCyTabAppearance &Get()
    {
        static FCyTabAppearance Style;
        return Style;
    }
};
class SCyChromeTab : public SCompoundWidget
{
    TAttribute<bool> Active;
    FSimpleDelegate Select, Close;

  public:
    SLATE_BEGIN_ARGS(SCyChromeTab) : _Active(false), _Width(124.f)
    {
    }
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ATTRIBUTE(bool, Active)
    SLATE_ARGUMENT(float, Width)
    SLATE_EVENT(FSimpleDelegate, OnSelected)
    SLATE_EVENT(FSimpleDelegate, OnClosed)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args)
    {
        Active = Args._Active;
        Select = Args._OnSelected;
        Close = Args._OnClosed;
        SetCursor(EMouseCursor::Default);
        ChildSlot[SNew(SBox)
                      .WidthOverride(Args._Width)
                      .HeightOverride(28)
                          [SNew(SBorder)
                               .Padding(FMargin(9, 0, 4, 0))
                               .BorderImage_Lambda([this]() -> const FSlateBrush * {
                                   const auto &Style = FCyTabAppearance::Get();
                                   return Active.Get()  ? &Style.Active
                                          : IsHovered() ? &Style.Hover
                                                        : &Style.Empty;
                               })[SNew(SHorizontalBox) +
                                  SHorizontalBox::Slot().FillWidth(1).VAlign(
                                      VAlign_Center)[SNew(STextBlock)
                                                         .Text(Args._Label)
                                                         .Clipping(EWidgetClipping::ClipToBounds)
                                                         .ColorAndOpacity_Lambda([this] {
                                                             return FSlateColor(
                                                                 Active.Get()
                                                                     ? FLinearColor(.9f, .92f, .95f)
                                                                     : FLinearColor(.55f, .58f, .64f));
                                                         })] +
                                  SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                      [SNew(SBox).WidthOverride(20).HeightOverride(20).Visibility(
                                          Close.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
                                           [SNew(SButton)
                                                .ButtonStyle(&FCyTabAppearance::Get().Icon)
                                                .ToolTipText(FText::FromString(TEXT("Close conversation")))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .OnClicked_Lambda([this] {
                                                    Close.ExecuteIfBound();
                                                    return FReply::Handled();
                                                })[SNew(STextBlock).Text(FText::FromString(TEXT("×")))]]]]]];
    }
    virtual bool SupportsKeyboardFocus() const override
    {
        return true;
    }
    virtual FReply OnMouseButtonDown(const FGeometry &, const FPointerEvent &Event) override
    {
        if (Event.GetEffectingButton() == EKeys::MiddleMouseButton && Close.IsBound())
        {
            Close.Execute();
            return FReply::Handled();
        }
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            Select.ExecuteIfBound();
            return FReply::Handled().SetUserFocus(SharedThis(this));
        }
        return FReply::Unhandled();
    }
    virtual FReply OnKeyDown(const FGeometry &, const FKeyEvent &Event) override
    {
        if (Event.GetKey() == EKeys::Enter || Event.GetKey() == EKeys::SpaceBar)
        {
            Select.ExecuteIfBound();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }
};
