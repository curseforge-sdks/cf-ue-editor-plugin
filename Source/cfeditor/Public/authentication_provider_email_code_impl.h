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
#pragma once

#include "CoreMinimal.h"
#include "macros.h"
#include "authentication_provider.h"

#include <api/cfcore_api.h>
#include <api/cfcore_api_authentication.h>
#include <api/cfcore_api_authorized.h>
#include <api/cfcore_api_creation.h>

namespace cfeditor {

class CFCoreSdkService;
class IAuthenticationProviderDelegate;

// This is a two-step authentication provider that requires input from the
// end-user (email address and then a code)
class AuthenticationProviderEmailCodeImpl : public IAuthenticationProvider {
public:
  AuthenticationProviderEmailCodeImpl(
    TSharedRef<CFCoreSdkService> InSdkService,
    IAuthenticationProviderDelegate* InDelegate);

// IAuthenticationProvider
public:
  virtual bool IsUserAuthenticated() override;
  virtual void LoginAsync() override;
  virtual void LogoutAsync() override;

private:
  void OpenSignInWindow();

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AuthenticationProviderEmailCodeImpl);

  TSharedRef<CFCoreSdkService> SdkService_;
  IAuthenticationProviderDelegate* Delegate_;
};

}; // namespace cfeditor