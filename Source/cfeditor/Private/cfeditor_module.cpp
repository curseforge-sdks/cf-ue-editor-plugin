// Copyright 2023 Blue Isle Studios Inc. All Rights Reserved.
/*MIT License

Copyright (c) 2022 Overwolf Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/
#include "cfeditor_module.h"

#include "authentication_provider_email_code_impl.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/World.h"
#include "LevelEditor.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "WidgetBlueprint.h"
#include "EditorUtilityWidget.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Runtime/Projects/Public/Interfaces/IPluginManager.h"
#include "cfeditor_style.h"
#include "cfeditor_commands.h"
#include "cfuploadwidget.h"
#include "cfcore_sdk_service.h"
#include "authentication_provider_steam_impl.h"
#include "authentication_provider_test_steam_impl.h"
#include "consts.h"
#include "OnlineSubsystem.h"
#include "mods_loader.h"

using namespace cfeditor;

#define LOCTEXT_NAMESPACE "FCFEditorModule"

// IModuleInterface
void FCFEditorModule::StartupModule() {
	if (IsRunningCommandlet()) {
		return;
	}

	RegisterStyles();
	InitializeCommandButtons();

	CFCoreSdkService_ = MakeShared<CFCoreSdkService>(this);

	AuthenticationProvider_ =
		MakeShared<AuthenticationProviderEmailCodeImpl>(
			CFCoreSdkService_.ToSharedRef(),
			this);

	CFCoreSdkService_->InitializeAsync();
}

// IModuleInterface
void FCFEditorModule::ShutdownModule() {
	if (IsRunningCommandlet()) 	{
		return;
	}

	CFCoreSdkService_->Uninitialize();

	UCFUploadWidget::Categories.Empty();
	FCFEditorStyle::Shutdown();
	FCFEditorCommands::Unregister();
}

// ICFCoreSdkServiceDelegate
void FCFEditorModule::OnCFCoreSdkInitialized() {
	CFCoreSdkService_->RetrieveCategoriesAsync();

	// PerformAuthentication
	AuthenticationProvider_->LoginAsync();
}

// ICFCoreSdkServiceDelegate
void FCFEditorModule::OnCFCoreSdkInitializationError(const FCFCoreError& Error) {
	FText Msg = FText::Format(
		LOCTEXT("CFCoreInitFailure", "Failed to initialize the CFCore SDK - with error message '{0}' Please restart or contact support."),
		FText::FromString(Error.description));
	FMessageDialog::Open(EAppMsgType::Ok, Msg);
}

// ICFCoreSdkServiceDelegate
void FCFEditorModule::OnCFCoreSdkRetrieveCategoriesResult(
	TArray<FCategory>&& RootCategories,
	TMap<int64, TArray<FCategory>>&& Categories) {

	UCFUploadWidget::RootCategories = MoveTemp(RootCategories);
	UCFUploadWidget::Categories = MoveTemp(Categories);
}

// ICFCoreSdkServiceDelegate
void FCFEditorModule::OnCFCoreSdkAuthorized() {
	UE_LOG_ONLINE(Log, TEXT("[CFCore] User was successfully authenticated"));
}

// ICFCoreSdkServiceDelegate
void FCFEditorModule::OnCFCoreSdkAuthorizationFailed(const FCFCoreError& Error) {
	FText Msg = FText::Format(
		LOCTEXT("SteamConnectFailure", "Failed to authenticate with Steam. Got Response code {0} with error message '{1}'"),
		FText::FromString(FString::FromInt((int32)Error.code)),
		FText::FromString(Error.description));
	FMessageDialog::Open(EAppMsgType::Ok, Msg);
}

// IAuthenticationProvider
void FCFEditorModule::OnAuthenticationError(const FText& ErrorMsg) {
	FMessageDialog::Open(EAppMsgType::Ok, ErrorMsg);
}

void FCFEditorModule::OpenSignoutWindow() {
	const TCHAR id[] = TEXT("ModUploadWindow/EUW_SignOutFlow.EUW_SignOutFlow");

#if ENGINE_MAJOR_VERSION >= 5
	FString Resource = FString::Printf(TEXT("/%s/%s"), TEXT(UE_PLUGIN_NAME), id);
	FSoftObjectPath ItemRef = Resource;
#else
	FString Resource = FString::Printf(TEXT("EditorUtilityWidgetBlueprint'/%s/%s"), TEXT(UE_PLUGIN_NAME), id);
	FStringAssetReference ItemRef = Resource;
#endif

	ItemRef.TryLoad();

	UObject* ItemClass = ItemRef.ResolveObject();
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(ItemClass);
	if(!WidgetBlueprint || !WidgetBlueprint->GeneratedClass || !GEditor) {
		return;
	}

	if (!WidgetBlueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass())) {
		return;
	}

	UEditorUtilityWidgetBlueprint* const EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(WidgetBlueprint);
	if (!EditorWidget) {
		return;
	}

	UEditorUtilitySubsystem* const EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	if (!EditorUtilitySubsystem) {
		return;
	}

	EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(EditorWidget,
																											UCFUploadWidget::TabId);
}

void FCFEditorModule::OpenUploadMODWindow() {
	const TCHAR id[] = TEXT("ModUploadWindow/ModUploadWindow.ModUploadWindow");

#if ENGINE_MAJOR_VERSION >= 5
	FString Resource = FString::Printf(TEXT("/%s/%s"), TEXT(UE_PLUGIN_NAME), id);
	FSoftObjectPath ItemRef = Resource;
#else
	FString Resource = FString::Printf(TEXT("EditorUtilityWidgetBlueprint'/%s/%s"), TEXT(UE_PLUGIN_NAME), id);
	FStringAssetReference ItemRef = Resource;
#endif

	ItemRef.TryLoad();

	UObject* ItemClass = ItemRef.ResolveObject();
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(ItemClass);
	if(!WidgetBlueprint || !WidgetBlueprint->GeneratedClass || !GEditor) {
		return;
	}

	if (!WidgetBlueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass())) {
		return;
	}

	UEditorUtilityWidgetBlueprint* const EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(WidgetBlueprint);
	if (!EditorWidget) {
		return;
	}

	UEditorUtilitySubsystem* const EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	if (!EditorUtilitySubsystem) {
		return;
	}

	EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(EditorWidget,
																											UCFUploadWidget::TabId);
}

void FCFEditorModule::RegisterStyles() {
	FCFEditorStyle::Initialize();
	FCFEditorStyle::ReloadTextures();
}

void FCFEditorModule::InitializeCommandButtons() {
	MapCommands();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// NOTE(twolf): Extending the toolbar seems to no longer work on UE5
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Misc",
		EExtensionHook::After,
		PluginCommands_,
		FToolBarExtensionDelegate::CreateRaw(this, &FCFEditorModule::AddToolbarExtension));
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

#if ENGINE_MAJOR_VERSION >= 5
	// NOTE(twolf): IN UE5 we'll extend the top menu bar
	TSharedPtr<FExtender> MenuBarExtender = MakeShareable(new FExtender);
	MenuBarExtender->AddMenuBarExtension(
		"Help",
		EExtensionHook::Before,
		PluginCommands_,
		FMenuBarExtensionDelegate::CreateRaw(this, &FCFEditorModule::AddPullDownMenu)
	);

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuBarExtender);
#endif
}

bool FCFEditorModule::Enabled_ShareUGC() {
	int32 AvailableMods = FModsLoader::GetNumAvailableGameMods();
	bool HasAvailableMods = AvailableMods > 0;

	bool IsLoggedIn = AuthenticationProvider_->IsUserAuthenticated();
	return IsLoggedIn && HasAvailableMods;
}

static bool Disabled() { return false; }

void FCFEditorModule::MapCommands() {
	PluginCommands_ = MakeShareable(new FUICommandList);

	FCFEditorCommands::Register();

	PluginCommands_->MapAction(
		FCFEditorCommands::Get().SignIn,
		FExecuteAction::CreateLambda([this]() {
			AuthenticationProvider_->LoginAsync();
		}),
		FCanExecuteAction::CreateLambda([this]() {
			bool SignedIn = AuthenticationProvider_->IsUserAuthenticated();
			return !SignedIn;
		}),
		FGetActionCheckState::CreateLambda([]() {
			return ECheckBoxState::Undetermined;
		}),
		FIsActionButtonVisible::CreateLambda([this]() {
			bool SignedIn = AuthenticationProvider_->IsUserAuthenticated();
			return !SignedIn;
		})
	);

	PluginCommands_->MapAction(
		FCFEditorCommands::Get().SignOut,
		FExecuteAction::CreateLambda([this]() {
			OpenSignoutWindow();
			//AuthenticationProvider_->LogoutAsync();
		}),
		FCanExecuteAction::CreateLambda([this]() {
			bool SignedIn = AuthenticationProvider_->IsUserAuthenticated();
			return SignedIn;
		}),
		FGetActionCheckState::CreateLambda([]() {
			return ECheckBoxState::Undetermined;
		}),
		FIsActionButtonVisible::CreateLambda([this]() {
			bool SignedIn = AuthenticationProvider_->IsUserAuthenticated();
			return SignedIn;
		})
	);

	PluginCommands_->MapAction(
    FCFEditorCommands::Get().ShareUGC,
    FExecuteAction::CreateRaw(this, &FCFEditorModule::OpenUploadMODWindow),
    FCanExecuteAction::CreateRaw(this, &FCFEditorModule::Enabled_ShareUGC));
}

void FCFEditorModule::AddToolbarExtension(FToolBarBuilder& Builder) {
	Builder.AddToolBarButton(FCFEditorCommands::Get().SignIn);
	Builder.AddToolBarButton(FCFEditorCommands::Get().SignOut);
	Builder.AddToolBarButton(FCFEditorCommands::Get().ShareUGC);
}

void FCFEditorModule::AddPullDownMenu(FMenuBarBuilder& MenuBuilder) {
	MenuBuilder.AddPullDownMenu(
		FText::FromString(kUIMenuBarLabel),
		FText::FromString(kUIMenuBarTooltip),
		FNewMenuDelegate::CreateRaw(this, &FCFEditorModule::FillMenu),
		"Custom"
	);
}

void FCFEditorModule::FillMenu(FMenuBuilder& MenuBuilder) {
	MenuBuilder.BeginSection(kUIMenuBarLabel);

	{
		MenuBuilder.AddMenuEntry(
			FCFEditorCommands::Get().ShareUGC,
			NAME_None,
			FText::FromString(kUIMenuShareUGCEntryLabel),
			FText::FromString(kUIMenuShareUGCEntryTooltip),
			FSlateIcon()
		);

		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(
			FCFEditorCommands::Get().SignIn,
			NAME_None,
			FText::FromString(kUIMenuSignInEntryLabel),
			FText::FromString(kUIMenuSignInEntryTooltip),
			FSlateIcon()
		);

		MenuBuilder.AddMenuEntry(
			FCFEditorCommands::Get().SignOut,
			NAME_None,
			FText::FromString(kUIMenuSignOutEntryLabel),
			FText::FromString(kUIMenuSignOutEntryTooltip),
			FSlateIcon()
		);
	}

	MenuBuilder.EndSection();
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCFEditorModule, cfeditor)