#include "Snippet/AngelscriptSnippetRunnerWindow.h"

#include "AngelscriptEngine.h"
#include "AngelscriptSnippet.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AngelscriptSnippetRunnerWindow"

namespace AngelscriptSnippetRunnerWindow_Private
{
	FString ResultCodeToString(const EAngelscriptSnippetResultCode ResultCode)
	{
		switch (ResultCode)
		{
		case EAngelscriptSnippetResultCode::Succeeded:
			return TEXT("Succeeded");
		case EAngelscriptSnippetResultCode::DisabledInShipping:
			return TEXT("DisabledInShipping");
		case EAngelscriptSnippetResultCode::InvalidRequest:
			return TEXT("InvalidRequest");
		case EAngelscriptSnippetResultCode::PreprocessFailed:
			return TEXT("PreprocessFailed");
		case EAngelscriptSnippetResultCode::CompileFailed:
			return TEXT("CompileFailed");
		case EAngelscriptSnippetResultCode::EntryPointMissing:
			return TEXT("EntryPointMissing");
		case EAngelscriptSnippetResultCode::ExecutionException:
			return TEXT("ExecutionException");
		default:
			return TEXT("Unknown");
		}
	}

	FAngelscriptEngine* ResolveEngine()
	{
		FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
		if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
		{
			Engine = &FAngelscriptEngine::Get();
		}

		return Engine;
	}

	UObject* ResolveEditorWorldContextObject()
	{
		if (GEditor == nullptr)
		{
			return nullptr;
		}

		return GEditor->GetEditorWorldContext().World();
	}

	FText FormatResult(const FAngelscriptSnippetResult& Result)
	{
		FString Text;
		if (Result.bSucceeded)
		{
			Text = FString::Printf(
				TEXT("Succeeded\nPath: %s\nModule: %s"),
				*Result.VirtualPath,
				*Result.ModuleName);
			return FText::FromString(Text);
		}

		Text = FString::Printf(
			TEXT("%s\n%s\nPath: %s"),
			*ResultCodeToString(Result.ResultCode),
			Result.ErrorMessage.IsEmpty() ? TEXT("Snippet execution failed.") : *Result.ErrorMessage,
			*Result.VirtualPath);

		if (!Result.ExceptionMessage.IsEmpty())
		{
			Text += FString::Printf(
				TEXT("\nException: %s\nLine: %d"),
				*Result.ExceptionMessage,
				Result.ExceptionLine);
		}

		for (const FAngelscriptSnippetDiagnostic& Diagnostic : Result.Diagnostics)
		{
			Text += FString::Printf(
				TEXT("\n%s(%d:%d): %s"),
				*Diagnostic.Section,
				Diagnostic.UserRow,
				Diagnostic.Column,
				*Diagnostic.Message);
		}

		return FText::FromString(Text);
	}

	class SAngelscriptSnippetRunnerDialog : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAngelscriptSnippetRunnerDialog) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ModeLabel", "Mode"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SCheckBox)
							.Type(ESlateCheckBoxType::ToggleButton)
							.IsChecked(this, &SAngelscriptSnippetRunnerDialog::IsModeChecked, EAngelscriptSnippetSourceMode::Statements)
							.OnCheckStateChanged(this, &SAngelscriptSnippetRunnerDialog::OnModeChanged, EAngelscriptSnippetSourceMode::Statements)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("StatementsModeLabel", "Statements"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(SCheckBox)
							.Type(ESlateCheckBoxType::ToggleButton)
							.IsChecked(this, &SAngelscriptSnippetRunnerDialog::IsModeChecked, EAngelscriptSnippetSourceMode::FullSource)
							.OnCheckStateChanged(this, &SAngelscriptSnippetRunnerDialog::OnModeChanged, EAngelscriptSnippetSourceMode::FullSource)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("FullSourceModeLabel", "Full Source"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SAssignNew(KeepModuleCheckBox, SCheckBox)
							.IsChecked(ECheckBoxState::Unchecked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("KeepModuleLabel", "Keep Module"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("ExecuteButtonLabel", "Execute"))
							.OnClicked(this, &SAngelscriptSnippetRunnerDialog::OnExecuteClicked)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(SourceTextBox, SMultiLineEditableTextBox)
						.Text(FText::GetEmpty())
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.MinDesiredHeight(128.0f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
							.Padding(6.0f)
							[
								SAssignNew(ResultTextBlock, STextBlock)
								.AutoWrapText(true)
								.Text(LOCTEXT("ReadyResultText", "Ready"))
							]
						]
					]
				]
			];
		}

	private:
		ECheckBoxState IsModeChecked(const EAngelscriptSnippetSourceMode InMode) const
		{
			return SourceMode == InMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		void OnModeChanged(const ECheckBoxState NewState, const EAngelscriptSnippetSourceMode InMode)
		{
			if (NewState == ECheckBoxState::Checked)
			{
				SourceMode = InMode;
			}
		}

		FReply OnExecuteClicked()
		{
			FAngelscriptEngine* Engine = ResolveEngine();
			if (Engine == nullptr)
			{
				SetResultText(LOCTEXT("EngineMissingResult", "Angelscript engine is not initialized."));
				return FReply::Handled();
			}

			FAngelscriptSnippetRequest Request;
			Request.SourceText = SourceTextBox.IsValid() ? SourceTextBox->GetText().ToString() : FString();
			Request.SourceMode = SourceMode;
			Request.Label = TEXT("Editor");
			Request.WorldContextObject = ResolveEditorWorldContextObject();
			Request.bKeepModuleForDebugging = KeepModuleCheckBox.IsValid() && KeepModuleCheckBox->IsChecked();

			const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(*Engine, Request);
			SetResultText(FormatResult(Result));
			return FReply::Handled();
		}

		void SetResultText(const FText& Text)
		{
			if (ResultTextBlock.IsValid())
			{
				ResultTextBlock->SetText(Text);
			}
		}

		TSharedPtr<SMultiLineEditableTextBox> SourceTextBox;
		TSharedPtr<SCheckBox> KeepModuleCheckBox;
		TSharedPtr<STextBlock> ResultTextBlock;
		EAngelscriptSnippetSourceMode SourceMode = EAngelscriptSnippetSourceMode::Statements;
	};
}

void FAngelscriptSnippetRunnerWindow::OpenWindow()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Angelscript Snippet"))
		.ClientSize(FVector2D(860.0f, 620.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(SNew(AngelscriptSnippetRunnerWindow_Private::SAngelscriptSnippetRunnerDialog));

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

#undef LOCTEXT_NAMESPACE
