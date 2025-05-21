/*MIT License

Copyright (c) 2024 Overwolf Ltd.

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
#include "authentication_provider_email_code_impl.h"
#include "authentication_provider_delegate.h"
#include "cfcore_sdk_service.h"
#include "WidgetBlueprint.h"
#include "EditorUtilityWidget.h"
#include "Framework/Docking/TabManager.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "OnlineSubsystem.h"
#include "cfuploadwidget.h"

using namespace cfeditor;

#define LOCTEXT_NAMESPACE "AuthenticationProviderEmailCodeImpl"

AuthenticationProviderEmailCodeImpl::AuthenticationProviderEmailCodeImpl(
	TSharedRef<CFCoreSdkService> InSdkService,
	IAuthenticationProviderDelegate* InDelegate) : SdkService_(InSdkService), Delegate_(InDelegate) {
}

// IAuthenticationProvider
bool AuthenticationProviderEmailCodeImpl::IsUserAuthenticated() {
	return SdkService_->IsUserAuthenticated();
}

void AuthenticationProviderEmailCodeImpl::LoginAsync() {
	bool bUserAuthenticated = SdkService_->IsUserAuthenticated();

	if (bUserAuthenticated) {
		UE_LOG_ONLINE(Log, TEXT("[CFCore] User is already authenticated"));
		return;
	}

	OpenSignInWindow();
}

void AuthenticationProviderEmailCodeImpl::LogoutAsync() {
	SdkService_->LogoutAsync();
}

void AuthenticationProviderEmailCodeImpl::OpenSignInWindow() {
	const TCHAR id[] = TEXT("ModUploadWindow/EUW_SignInFlow.EUW_SignInFlow");

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

	EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(
		EditorWidget,
		UCFUploadWidget::TabId);
}

#undef LOCTEXT_NAMESPACE