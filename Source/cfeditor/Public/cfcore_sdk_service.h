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
#include <api/models/category.h>
#include <api/models/enums/external_auth_provider.h>
#include <common/cfcore_error.h>

#include <api/cfcore_api.h>
#include <cfcore/Private/services/user_context/user_context_service.h>

namespace cfeditor {

class ICFCoreSdkServiceDelegate;

// This class is a simplified wrapper above the CFCore SDK
class CFCoreSdkService {
public:
  CFCoreSdkService(ICFCoreSdkServiceDelegate* InDelegate);
  virtual ~CFCoreSdkService() = default;

  DECLARE_DELEGATE_OneParam(FSendSecurityCodeDelegate, const TOptional<FCFCoreError>&);
  DECLARE_DELEGATE_OneParam(FGenerateAuthTokenDelegate, const TOptional<FCFCoreError>&);

public:
  void InitializeAsync();
  bool RetrieveCategoriesAsync();
  bool AuthenticateByExternalProviderAsync(ECFCoreExternalAuthProvider Provider,
                                           const FString& Token);

  bool LogoutAsync();
  bool IsUserAuthenticated();
  void Uninitialize();

private:
  void HandleRetrieveCategoriesResults(TOptional<TArray<FCategory>> OptCategories);

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CFCoreSdkService);

  ICFCoreSdkServiceDelegate* Delegate_;

  TSharedPtr<cfcore::ICFCoreApi> cfcore_api_;
  TSharedPtr<cfcore::IUserContextService> user_context_service_;
};

}; // namespace cfeditor