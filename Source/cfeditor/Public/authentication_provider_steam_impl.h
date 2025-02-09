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
#pragma once

#include "CoreMinimal.h"
#include "macros.h"
#include "authentication_provider.h"

namespace cfeditor {

class CFCoreSdkService;
class IAuthenticationProviderDelegate;

// Implements Steam silent authentication.
//
// To use this class, you'll need to:
// 1. Setup you Steam 'Encrypted App Ticket Key' in your game's authentication
// section on https://console.curseforge.com
// 2. Edit cfeditor.Build.cs and "OnlineSubsystemSteam" to the
// PrivateDependencyModuleNames
// 3. Follow instructions on: https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Online/Steam/
class AuthenticationProviderSteamImpl : public IAuthenticationProvider {
public:
  AuthenticationProviderSteamImpl(TSharedRef<CFCoreSdkService> InSdkService,
                                  IAuthenticationProviderDelegate* InDelegate);

// IAuthenticationProvider
public:
  virtual bool IsUserAuthenticated() override;
  virtual void LoginAsync() override;
  virtual void LogoutAsync() override;

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AuthenticationProviderSteamImpl);

  TSharedRef<CFCoreSdkService> SdkService_;
  IAuthenticationProviderDelegate* Delegate_;
};

}; // namespace cfeditor