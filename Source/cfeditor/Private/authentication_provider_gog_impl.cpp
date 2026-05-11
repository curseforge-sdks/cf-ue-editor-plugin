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
#include "authentication_provider_gog_impl.h"
#include "authentication_provider_delegate.h"
#include "cfcore_sdk_service.h"
#include "consts.h"

#include "Misc/Base64.h"

using namespace cfeditor;

constexpr TCHAR kErrorGOGConnectFailureGeneric[] =
    TEXT("Failed to authenticate with GOG. Please make sure that GOG Galaxy is running and you are signed in to an account that owns %s");

AuthenticationProviderGOGImpl::AuthenticationProviderGOGImpl(
    TSharedRef<CFCoreSdkService> InSdkService,
    IAuthenticationProviderDelegate* InDelegate)
    : SdkService_(InSdkService), Delegate_(InDelegate) {
}

// IAuthenticationProvider

bool AuthenticationProviderGOGImpl::IsUserAuthenticated() {
  return SdkService_->IsUserAuthenticated();
}

void AuthenticationProviderGOGImpl::LoginAsync() {
  if (!galaxy::api::User()) {
    Delegate_->OnAuthenticationError(FText::FromString(
        FString::Printf(kErrorGOGConnectFailureGeneric, kGameName)));
    return;
  }

  if (!galaxy::api::User()->IsLoggedOn()) {
    Delegate_->OnAuthenticationError(FText::FromString(
        FString::Printf(kErrorGOGConnectFailureGeneric, kGameName)));
    return;
  }

  UE_LOG_ONLINE(Log, TEXT("[CFCore] Attempting autologin with external credential type GOG"));
  galaxy::api::User()->RequestEncryptedAppTicket(nullptr, 0, this);
}

void AuthenticationProviderGOGImpl::LogoutAsync() {
  SdkService_->LogoutAsync();
}

// galaxy::api::IEncryptedAppTicketListener

void AuthenticationProviderGOGImpl::OnEncryptedAppTicketRetrieveSuccess() {
  uint32_t TicketSize = 0;
  galaxy::api::User()->GetEncryptedAppTicket(nullptr, TicketSize);

  if (TicketSize == 0) {
    Delegate_->OnAuthenticationError(
        FText::FromString("Failed to retrieve GOG encrypted app ticket"));
    return;
  }

  TArray<uint8> Ticket;
  Ticket.SetNumUninitialized(static_cast<int32>(TicketSize));
  galaxy::api::User()->GetEncryptedAppTicket(Ticket.GetData(), TicketSize);

  const FString EncodedToken = FBase64::Encode(Ticket.GetData(), Ticket.Num());

  bool Success = SdkService_->AuthenticateByExternalProviderAsync(
      ECFCoreExternalAuthProvider::GOG, EncodedToken);

  if (!Success) {
    Delegate_->OnAuthenticationError(
        FText::FromString("Failed to authenticate with GOG"));
  }
}

void AuthenticationProviderGOGImpl::OnEncryptedAppTicketRetrieveFailure(
    FailureReason reason) {
  UE_LOG_ONLINE(Warning, TEXT("[CFCore] GOG encrypted app ticket retrieval failed, reason: %d"),
      static_cast<int32>(reason));
  Delegate_->OnAuthenticationError(FText::FromString(
      FString::Printf(kErrorGOGConnectFailureGeneric, kGameName)));
}
