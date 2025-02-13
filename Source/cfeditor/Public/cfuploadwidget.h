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
#include "EditorUtilityWidget.h"
#include "api/models/creation/create_mod_file_request.h"
#include "api/models/creation/create_mod_request.h"
#include "api/models/category.h"
#include "api/models/mod.h"
#include "cfuploadwidget.generated.h"

USTRUCT(Blueprintable)
struct FCFModData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int64 Id = -1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Name = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString LogoPath = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Summary = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Description = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString HomepageURL = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Version = "1.0";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString Path = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FString MarketplaceURL = "";
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bIsValid = false;
};

UENUM(BlueprintType)
enum class EBuildPlatforms : uint8 {
	PS4,
	PS5,
	XB1,
	XSX,
	MAC,
	WIN,
	LINUX,
};

USTRUCT(Blueprintable)
struct FCModPlatformData {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBuildPlatforms BuildPlatform = EBuildPlatforms::WIN;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FText PlatformName = NSLOCTEXT("LEAP", "PlatformName_Windows", "Windows Client");
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bIsServer = false;

	uint32 GetTypeHash(const FCModPlatformData& ModPlatformData) { return HashCombine((uint32)ModPlatformData.BuildPlatform, (uint32)bIsServer); }
};

UCLASS(Config=ModConfig)
class UCFUploadWidget : public UEditorUtilityWidget {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FilePicker")
	void OpenFileDialog(const FString& DialogTitle, const FString& DefaultPath, const FString& FileTypes, FString& OutputPath, bool bIsDirectory);
	UFUNCTION(BlueprintCallable)
	void ShowConfirmationDialog(const FText& DialogTitle, const FText& DialogText);
	UFUNCTION(BlueprintCallable)
	void UpdatePluginWithModData(const FCFCoreMod& Mod);
	UFUNCTION(BlueprintCallable)
	void PackageModWithSettings(const int64 ModID, const FString& Version, const FString& Path, TArray<FCModPlatformData> BuildPlatforms);
	UFUNCTION(BlueprintCallable)
	void CloseTab();
	UFUNCTION(BlueprintCallable)
	void PostUploadCleanup(const FCFModData& ModData);
	UFUNCTION(BlueprintPure)
	const TArray<FCategory>& GetRootCategories() const;
	UFUNCTION(BlueprintPure)
	const TArray<FCategory>& GetSubCategories(const int64 ClassID) const;
	UFUNCTION(BlueprintPure)
	const FCategory& GetRootCategoryByName(const FString& Name) const;
	UFUNCTION(BlueprintPure)
	const FCategory& GetSubCategoryByName(const int64 ClassID, const FString& Name) const;
	UFUNCTION(BlueprintPure)
	FCFModData GetModDataByName(const FString& Name);
	UFUNCTION(BlueprintPure)
	FCFModData GetModById(const int32 Id);

	UFUNCTION(BlueprintImplementableEvent)
	void AddModDataToUI(const FCFModData& ModData);
	UFUNCTION(BlueprintImplementableEvent)
	void OnModPackagingFailed();
	UFUNCTION(BlueprintImplementableEvent)
	void OnModPackagingComplete();

	static TMap<int64, TArray<FCategory>> Categories;
	static TArray<FCategory> RootCategories;
	static TArray<FCategory> EmptyCategoryList;
	static FName TabId;
	FCategory InvalidCategory;

private:
	bool UpdatePluginDescriptor(const struct FPluginDescriptor& PluginDescriptor, TSharedRef<class IPlugin> Plugin);
	void NativeConstruct() override;
	FCFModData GetPluginData(TSharedRef<class IPlugin>);
	FString ExtractDescription(TSharedRef<class IPlugin> Plugin);
	int64 ExtractUgcIdFromPlugin(TSharedRef<class IPlugin> Plugin);
	void InsertUgcIdIntoPlugin(TSharedRef<class IPlugin> Plugin, int64 id);
	bool IsAllContentSaved(TSharedRef<IPlugin> Plugin);
	void SaveAndPackagePlugin(TSharedRef<IPlugin> Plugin, TArray<FCModPlatformData> BuildPlatforms);
	void PackagePlugin(TSharedRef<IPlugin> Plugin, const FString& OutputDirectory, TArray<FCModPlatformData>& BuildPlatforms);
	void ArchivePlugin(const FString& OutputDirectory);
	bool TryLoadPluginDescriptor(FString& JsonContent,
															 FString& PluginDescriptorPath);

	bool TryParseJsonPluginDescriptor(FString& JsonContent,
																		TSharedPtr<FJsonObject>& JsonObject,
																		FString& PluginDescriptorPath);
};
