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
#include "authentication_provider_steam_impl.h"
#include "authentication_provider_delegate.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemSteam.h"
#include "OnlineEncryptedAppTicketInterfaceSteam.h"

#include "cfcore_sdk_service.h"

#include "consts.h"

using namespace cfeditor;

// #define LOCTEXT_NAMESPACE "AuthenticationProviderSteamImpl"

const TCHAR kErrorSteamConnectFailureGeneric[] =
	TEXT("Failed to authenticate with Steam. Please make sure that your client is logged in to an account that owns %s");

AuthenticationProviderSteamImpl::AuthenticationProviderSteamImpl(
	TSharedRef<CFCoreSdkService> InSdkService,
	IAuthenticationProviderDelegate* InDelegate) :
	SdkService_(InSdkService),
	Delegate_(InDelegate) {
}

// IAuthenticationProvider
bool AuthenticationProviderSteamImpl::IsUserAuthenticated() {
	return SdkService_->IsUserAuthenticated();
}

void AuthenticationProviderSteamImpl::LoginAsync() {
	const IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get("STEAM");
	const IOnlineIdentityPtr OnlineIdentity = OnlineSub ?
		OnlineSub->GetIdentityInterface() : nullptr;

	FString GenericError = FString::Printf(kErrorSteamConnectFailureGeneric, kGameName);

	if (!OnlineIdentity.IsValid()) {
		Delegate_->OnAuthenticationError(FText::FromString(GenericError));
		return;
	}

	const FOnlineSubsystemSteam* OnlineSubSteam =
		static_cast<const FOnlineSubsystemSteam*>(OnlineSub);
	const FOnlineEncryptedAppTicketSteamPtr EncryptedAppTicketPtr =
		OnlineSubSteam->GetEncryptedAppTicketInterface();

	auto UniqueNetId = OnlineIdentity->GetUniquePlayerId(0);

	if (!EncryptedAppTicketPtr || !UniqueNetId.IsValid()) {
		Delegate_->OnAuthenticationError(FText::FromString(GenericError));
		return;
	}

	UE_LOG_ONLINE(Log, TEXT("[CFCore] Attempting autologin with external credential type Steam"));
	EncryptedAppTicketPtr->OnEncryptedAppTicketResultDelegate.AddLambda(
		[this, GenericError, EncryptedAppTicketPtr](bool bEncryptedDataAvailable,
																								int32 ResultCode) {
			if (!bEncryptedDataAvailable) {
				Delegate_->OnAuthenticationError(FText::FromString(GenericError));
				return;
			}

			TArray<uint8> EncryptedAppTicket;
			if (!EncryptedAppTicketPtr->GetEncryptedAppTicket(EncryptedAppTicket) ||
					EncryptedAppTicket.Num() == 0) {
				Delegate_->OnAuthenticationError(FText::FromString(GenericError));
				return;
			}

			const FString EncodedToken = FBase64::Encode(
				EncryptedAppTicket.GetData(),
				EncryptedAppTicket.Num());

			bool Success = SdkService_->AuthenticateByExternalProviderAsync(
				ECFCoreExternalAuthProvider::Steam,
				EncodedToken);

			if (!Success) {
				Delegate_->OnAuthenticationError(
					FText::FromString("Failed to Authenticate with Steam"));
			}

			EncryptedAppTicketPtr->OnEncryptedAppTicketResultDelegate.Clear();
		});

	EncryptedAppTicketPtr->RequestEncryptedAppTicket(nullptr, 0);
}

void AuthenticationProviderSteamImpl::LogoutAsync() {
	SdkService_->LogoutAsync();
}

#undef LOCTEXT_NAMESPACE
