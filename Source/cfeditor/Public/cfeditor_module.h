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
#include "Modules/ModuleManager.h"
#include "cfcore_sdk_service_delegate.h"
#include "authentication_provider_delegate.h"

class FToolBarBuilder;
class FMenuBuilder;

namespace cfeditor {
class IAuthenticationProvider;
class CFCoreSdkService;
};

class FCFEditorModule : public IModuleInterface,
												public cfeditor::ICFCoreSdkServiceDelegate,
												public cfeditor::IAuthenticationProviderDelegate {

// IModuleInterface
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

// ICFCoreSdkServiceDelegate
public:
	virtual void OnCFCoreSdkInitialized() override;
	virtual void OnCFCoreSdkInitializationError(const FCFCoreError& Error) override;
	virtual void OnCFCoreSdkRetrieveCategoriesResult(TArray<FCategory>&& RootCategories, TMap<int64, TArray<FCategory>>&& Categories) override;
	virtual void OnCFCoreSdkAuthorized() override;
	virtual void OnCFCoreSdkAuthorizationFailed(const FCFCoreError& Error) override;

// IAuthenticationProviderDelegate
public:
	virtual void OnAuthenticationError(const FText& ErrorMsg) override;

public:
	void OpenWindow();

	static void FindAvailableGameMods(TArray<TSharedRef<class IPlugin>>& OutAvailableGameMods);
	static int32 GetNumAvailableGameMods();

private:
	void RegisterStyles();
	void InitializeCommandButton();

	/** Returns true if the toolbar button is enabled (user must be logged in) */
	bool Enabled_ShareUGC();

	void MapCommands();
	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddPullDownMenu(FMenuBarBuilder& MenuBuilder);
	void FillMenu(FMenuBuilder& MenuBuilder);

private:
	TSharedPtr<class FUICommandList> PluginCommands_;
	TSharedPtr<cfeditor::IAuthenticationProvider> AuthenticationProvider_;
	TSharedPtr<cfeditor::CFCoreSdkService> CFCoreSdkService_;
};
