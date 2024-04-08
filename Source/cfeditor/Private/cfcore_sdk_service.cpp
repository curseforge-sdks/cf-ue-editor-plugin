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
#include "cfcore_sdk_service.h"
#include "cfcore_sdk_service_delegate.h"
#include "consts.h"

#include <cfcore_context.h>

using namespace cfeditor;
using namespace cfcore;

CFCoreSdkService::CFCoreSdkService(ICFCoreSdkServiceDelegate* InDelegate) :
  Delegate_(InDelegate) {
}

void CFCoreSdkService::InitializeAsync() {
	FCFCoreSettings Settings;
	Settings.gameId = kCFGameId;
	Settings.apiKey = kCFApiKey;
	Settings.maxConcurrentInstallations = 10;
	Settings.modsDirectory = FPaths::ProjectModsDir();
	Settings.userDataDirectory = FPaths::Combine(FPaths::ProjectModsDir(), TEXT(".eternal"));

	UE_LOG(LogTemp, Log, TEXT("[CFCore] Initializing CFCore..."));

	CFCoreContext::GetInstance()->Initialize(
		Settings,
		ICFCore::FInitializeDelegate::CreateLambda([this](
			TOptional<FCFCoreError> OptError) {
				if (!OptError.IsSet()) {
					Delegate_->OnCFCoreSdkInitialized();
					return;
				}

				UE_LOG(
					LogTemp,
					Error,
					TEXT("[CFCore] Error Initializng CFCore - code %d msg '%s'"),
					(int32)OptError->code,
					*OptError->description);

				Delegate_->OnCFCoreSdkInitializationError(*OptError);
			}));
}

bool CFCoreSdkService::RetrieveCategoriesAsync() {
	if (!CFCoreContext::GetInstance()->IsInitialized()) {
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[CFCore] RetrieveCategoriesAsync called before init completed"));
		return false;
	}

	FCFCoreGetCategoriesFilter Filter;
	CFCoreContext::GetInstance()->Api()->GetCategories(
		Filter,
		ICFCoreApi::FGetCategoriesDelegate::CreateLambda(
			[this](TOptional<TArray<FCategory>> OptCategories,
					const TOptional<FCFCoreApiResponseError>& OptError) {

				if (OptError.IsSet()) {
					// TCHAR Msg[] = TEXT("[CFCore] Failed to fetch categories for title - msg '%s'");
					UE_LOG(LogTemp, Error, TEXT("[CFCore] Failed to fetch categories for title - msg '%s'"), *OptError->description);
					return;
				}

				HandleRetrieveCategoriesResults(OptCategories);
			}));

	return true;
}

bool CFCoreSdkService::AuthenticateByExternalProviderAsync(
	ECFCoreExternalAuthProvider Provider,
	const FString& Token) {

	auto Authentication = CFCoreContext::GetInstance()->Authentication();
	if (!Authentication) {
		return false;
	}

	FExternalAuthAdditionalInfo AuthInfo;
	AuthInfo.eulaAcceptTime = FDateTime::UtcNow();
	Authentication->GenerateAuthTokenByExternalProvider(
		Provider,
		Token,
		AuthInfo,
		ICFCoreAuthentication::FGenerateAuthTokenDelegate::CreateLambda(
			[this, Token](const TOptional<FCFCoreError>& OptError) {
				if (OptError.IsSet()) {
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[CFCore] Error authenticating CFCore - code %d msg '%s', token '%s'"),
						(int32)OptError->code,
						*OptError->description,
						*Token);

					Delegate_->OnCFCoreSdkAuthorizationFailed(*OptError);
					return;
				}

				Delegate_->OnCFCoreSdkAuthorized();
			}));

	return true;
}

bool CFCoreSdkService::IsUserAuthenticated() {
	auto Authentication = CFCoreContext::GetInstance()->Authentication();
	if (!Authentication) {
		return false;
	}

	return Authentication->IsAuthenticated();
}

void CFCoreSdkService::Uninitialize() {
	TSharedPtr<TPromise<bool>> Promise = MakeShareable(new TPromise<bool>());

	CFCoreContext::GetInstance()->Uninitialize(
		ICFCore::FUninitializeDelegate::CreateLambda(
			[=](TOptional<FCFCoreError> OptError) {
				if (OptError.IsSet()) {
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[CFCore] Failed to uninitialize - msg '%s'"),
						*OptError->description);
				}

				Promise->SetValue(true);
			}));

	Promise->GetFuture().WaitFor(FTimespan::FromSeconds(10));
}

void CFCoreSdkService::HandleRetrieveCategoriesResults(
	TOptional<TArray<FCategory>> OptCategories) {

	TArray<FCategory> RootCategories;
	TMap<int64, TArray<FCategory>> Categories;

	if (!OptCategories.IsSet()) {
		Delegate_->OnCFCoreSdkRetrieveCategoriesResult(MoveTemp(RootCategories),
																									 MoveTemp(Categories));
		return;
	}

	for (const FCategory& Category : *OptCategories) {
		if (Category.isClass) {
			RootCategories.Add(Category);
			continue;
		}

		if (!Categories.Contains(Category.classId)) {
			Categories.Add(Category.classId, { Category });
		} else {
			Categories[Category.classId].Add(Category);
		}
	}

	Delegate_->OnCFCoreSdkRetrieveCategoriesResult(MoveTemp(RootCategories),
																								 MoveTemp(Categories));
}
