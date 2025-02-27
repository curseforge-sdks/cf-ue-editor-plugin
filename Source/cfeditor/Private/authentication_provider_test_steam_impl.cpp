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
#include "authentication_provider_test_steam_impl.h"
#include "authentication_provider_delegate.h"
#include "cfcore_sdk_service.h"

using namespace cfeditor;

// Set your Steam token here (base64)
const TCHAR kHardCodedSteamTokenBase64[] = TEXT("");

AuthenticationProviderTestSteamImpl::AuthenticationProviderTestSteamImpl(
	TSharedRef<CFCoreSdkService> InSdkService,
	IAuthenticationProviderDelegate* InDelegate) : SdkService_(InSdkService),
																								 Delegate_(InDelegate) {
}

// IAuthenticationProvider
bool AuthenticationProviderTestSteamImpl::IsUserAuthenticated() {
	return SdkService_->IsUserAuthenticated();
}

void AuthenticationProviderTestSteamImpl::LoginAsync() {
	bool Success = SdkService_->AuthenticateByExternalProviderAsync(
		ECFCoreExternalAuthProvider::Steam,
		kHardCodedSteamTokenBase64);

	if (!Success) {
		Delegate_->OnAuthenticationError(
			FText::FromString("Failed to Authenticate with Steam"));
	}
}

void AuthenticationProviderTestSteamImpl::LogoutAsync() {
	SdkService_->LogoutAsync();
}
