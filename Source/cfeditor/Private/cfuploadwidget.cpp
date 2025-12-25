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
#include "cfuploadwidget.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"

#include "Interfaces/IMainFrameModule.h"
#include "EditorUtilitySubsystem.h"
#include "IUATHelperModule.h"

#include <JsonObjectConverter.h>
#include <JsonUtilities.h>

#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Runtime/Core/Public/Misc/MessageDialog.h"
#include "Runtime/Projects/Public/Interfaces/IPluginManager.h"
#include "cfeditor_module.h"
#include "FileHelpers.h"
#include "cfeditor_style.h"
#include "Runtime/Core/Public/Async/Async.h"
#include "cfcore_context.h"
#include "mods_loader.h"

#define LOCTEXT_NAMESPACE "FCFEditorModule"

// -----------------------------------------------------------------------------
TMap<int64, TArray<FCategory>> UCFUploadWidget::Categories;
TArray<FCategory> UCFUploadWidget::RootCategories;
TArray<FCategory> UCFUploadWidget::EmptyCategoryList;
FName UCFUploadWidget::TabId = NAME_None;

const FString kUgcIdFieldName = TEXT("CfUgcId");

// -----------------------------------------------------------------------------
void UCFUploadWidget::OpenFileDialog(const FString& DialogTitle,
  const FString& DefaultPath,
  const FString& FileTypes,
  FString& OutputPath,
  bool bIsDirectory) {
  void* ParentWindowWindowHandle = nullptr;
  IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
  const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
  if (MainFrameParentWindow.IsValid() &&
    MainFrameParentWindow->GetNativeWindow().IsValid()) {
    ParentWindowWindowHandle =
      MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
  }

  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get()) {
    if (bIsDirectory) {
      DesktopPlatform->OpenDirectoryDialog(ParentWindowWindowHandle,
        DialogTitle,
        DefaultPath,
        OutputPath);
    }
    else {
      TArray<FString> OutFileNames;
      uint32 SelectionFlag = 0; //A value of 0 represents single file selection while a value of 1 represents multiple file selection
      DesktopPlatform->OpenFileDialog(ParentWindowWindowHandle,
        DialogTitle,
        DefaultPath,
        FString(""),
        FileTypes,
        SelectionFlag,
        OutFileNames);
      if (OutFileNames.Num()) {
        OutputPath = OutFileNames[0];
      }
    }
  }
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::ShowConfirmationDialog(const FText& DialogTitle,
  const FText& DialogText) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
  FMessageDialog::Open(EAppMsgType::Ok, DialogText, DialogTitle);
#else
  FMessageDialog::Open(EAppMsgType::Ok, DialogText, &DialogTitle);
#endif
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::UpdatePluginWithModData(const FCFCoreMod& Mod, ECFCoreAutoCookingType AutoCookingType) {
  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  TSharedPtr<IPlugin> plugin;
  for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    if (AvailableMod->GetName() == Mod.name) {
      plugin = AvailableMod;
      break;
    }
  }

  if (!plugin.IsValid()) {
    return;
  }

  FPluginDescriptor Descriptor = plugin->GetDescriptor();

  Descriptor.Description = Mod.summary;
  Descriptor.MarketplaceURL = Mod.links.websiteUrl;
  Descriptor.Category = "UGC";
  if (!UpdatePluginDescriptor(Descriptor, plugin.ToSharedRef())) {
    return;
  }
  InsertUgcIdIntoPlugin(plugin.ToSharedRef(), Mod.id);
  InsertAutoCookingTypeIntoPlugin(plugin.ToSharedRef(), AutoCookingType);
}

// -----------------------------------------------------------------------------
//intentional copy so we dont clobber ui
void UCFUploadWidget::PackageModWithSettings(
  const int64 ModID,
  const FString& Version,
  const FString& Path,
  TArray<FCModPlatformData> BuildPlatforms) {

  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);
  if (FPaths::FileExists(Path)) {
    ShowConfirmationDialog(LOCTEXT("PackageFailureAlreadyExistsTitle", "Package Failure"), LOCTEXT("PackageFailureAlreadyExists", "Failed to package the mod because there's already a packaged mod present. Please clear your mod's build folder."));
    OnModPackagingFailed();
    return;
  }

  const FString CommandLine = FString::Printf(
    TEXT("PackageUGC -Prepare=\"true\" -StagingDirectory=\"%s\""), *Path);

  const FText PackagingText = LOCTEXT("SimpleUGCEditor_PackagePluginTaskName",
    "Preparing");
  IUATHelperModule::Get().CreateUatTask(
    CommandLine,
    LOCTEXT("SimpleUGCEditor_PackageGeneral", "Mod Package Process"),
    PackagingText,
    PackagingText,
    FCFEditorStyle::Get().GetBrush(TEXT("cfeditor.ShareUGC")),
#if ENGINE_MAJOR_VERSION >= 5
    nullptr,
#endif
    [this, OutAvailableGameMods, ModID, BuildPlatforms](FString TaskResult,
      double TimeSec) {
        AsyncTask(ENamedThreads::GameThread, [this,
          TaskResult,
          OutAvailableGameMods,
          ModID,
          BuildPlatforms]() {
            if (TaskResult == "Completed") {
              for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
                const FCFModData ModData = GetPluginData(AvailableMod);
                if (ModData.Id == ModID) {
                  FPluginDescriptor Descriptor = AvailableMod->GetDescriptor();
                  Descriptor.Version = Descriptor.Version++;
                  Descriptor.VersionName = FGuid::NewGuid().ToString();
                  Descriptor.Category = "UGC";
                  if (!UpdatePluginDescriptor(Descriptor, AvailableMod)) {
                    return;
                  }

                  SaveAndPackagePlugin(AvailableMod, BuildPlatforms);
                  return;
                }
              }
            }
            else {
              ShowConfirmationDialog(LOCTEXT("buildfailtitle", "Couldn't Build UGC"), LOCTEXT("buildfail", "Your ugc failed to build. Please check the output log for more information."));
              OnModPackagingFailed();
            }
          });
    });
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::CloseTab() {
  if (UEditorUtilitySubsystem* const EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>()) {
    EditorUtilitySubsystem->CloseTabByID(TabId);
  }
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::PostUploadCleanup(const FCFModData& ModData) {
  const FString FullPath = ModData.Path + "/" + ModData.Name + ".zip";
  if (FPaths::ValidatePath(FullPath) && FPaths::FileExists(FullPath)) {
    IFileManager& FileManager = IFileManager::Get();
    FileManager.Delete(*FullPath);
  }
}

// -----------------------------------------------------------------------------
FCFModData UCFUploadWidget::GetPluginData(TSharedRef<class IPlugin> Plugin) {
  FCFModData PluginData;
  PluginData.Id = ExtractUgcIdFromPlugin(Plugin);
  PluginData.Description = ExtractDescription(Plugin);
  PluginData.Name = Plugin->GetName();
  PluginData.FriendlyName = Plugin->GetDescriptor().FriendlyName;
  PluginData.HomepageURL = Plugin->GetDescriptor().CreatedByURL;
  PluginData.Path = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
  PluginData.Version = Plugin->GetDescriptor().VersionName;
  PluginData.MarketplaceURL = Plugin->GetDescriptor().MarketplaceURL;
  PluginData.bIsValid = true;
  return MoveTemp(PluginData);
}

// -----------------------------------------------------------------------------
// NOTE: This is for backwards compatibility, before we added the UgcId field
FString UCFUploadWidget::ExtractDescription(TSharedRef<class IPlugin> Plugin) {
  TArray<FString> SplitDescription;
  Plugin->GetDescriptor().Description.ParseIntoArray(SplitDescription,
    TEXT("&cf_ugcID="));

  if (SplitDescription.Num() == 0) {
    return "";
  }

  SplitDescription[0].ParseIntoArray(SplitDescription, TEXT("&ugcID="));
  return SplitDescription[0];
}

// -----------------------------------------------------------------------------
int64 UCFUploadWidget::ExtractUgcIdFromPlugin(
  TSharedRef<class IPlugin> Plugin) {

  FString PluginDescriptorPath = Plugin->GetDescriptorFileName();
  FString JsonContent;
  if (!TryLoadPluginDescriptor(JsonContent, PluginDescriptorPath)) {
    return -1;
  }

  TSharedPtr<FJsonObject> JsonObject;
  if (!TryParseJsonPluginDescriptor(
    JsonContent, JsonObject, PluginDescriptorPath)) {

    return -1;
  }

  int64 UgcId;
  if (JsonObject->TryGetNumberField(kUgcIdFieldName, UgcId)) {
    return UgcId;
  }

  UE_LOG(LogTemp,
    Warning,
    TEXT("UgcId field not found or invalid in plugin descriptor: %s"),
    *PluginDescriptorPath);
  return -1;
}

// -----------------------------------------------------------------------------
ECFCoreAutoCookingType UCFUploadWidget::ExtractAutoCookingTypeFromPlugin(
  TSharedRef<class IPlugin> Plugin) {

  FString PluginDescriptorPath = Plugin->GetDescriptorFileName();
  FString JsonContent;
  if (!TryLoadPluginDescriptor(JsonContent, PluginDescriptorPath)) {
    UE_LOG(LogTemp, Warning, TEXT("Failed to load plugin descriptor: %s"), *PluginDescriptorPath);
    return ECFCoreAutoCookingType::PCOnly; // Safe default
  }

  TSharedPtr<FJsonObject> JsonObject;
  if (!TryParseJsonPluginDescriptor(
    JsonContent, JsonObject, PluginDescriptorPath)) {

    UE_LOG(LogTemp, Warning, TEXT("Failed to parse JSON in plugin descriptor: %s"), *PluginDescriptorPath);
    return ECFCoreAutoCookingType::PCOnly;
    }

  FString CookingTypeStr;
  if (!JsonObject->TryGetStringField(TEXT("AutoCookingType"), CookingTypeStr)) {
    UE_LOG(LogTemp, Verbose, TEXT("AutoCookingType field not found or not a string in %s. Defaulting to All."), *PluginDescriptorPath);
    return ECFCoreAutoCookingType::PCOnly;
  }

  const UEnum* EnumPtr = StaticEnum<ECFCoreAutoCookingType>();
  if (!EnumPtr) {
    UE_LOG(LogTemp, Error, TEXT("Failed to get StaticEnum for ECFCoreAutoCookingType."));
    return ECFCoreAutoCookingType::PCOnly;
  }

  const FString FullEnumName = FString::Printf(TEXT("ECFCoreAutoCookingType::%s"), *CookingTypeStr);
  int64 EnumValue = EnumPtr->GetValueByNameString(FullEnumName);
  if (EnumValue == INDEX_NONE) {
    UE_LOG(LogTemp, Warning, TEXT("Invalid AutoCookingType value '%s' in plugin descriptor: %s. Defaulting to All."), *CookingTypeStr, *PluginDescriptorPath);
    return ECFCoreAutoCookingType::PCOnly;
  }

  return static_cast<ECFCoreAutoCookingType>(EnumValue);
}

// -----------------------------------------------------------------------------
const TArray<FCategory>& UCFUploadWidget::GetRootCategories() const {
  return RootCategories;
}

// -----------------------------------------------------------------------------
const TArray<FCategory>& UCFUploadWidget::GetSubCategories(const int64 ClassID) const {
  return Categories.Contains(ClassID) ? Categories[ClassID] : EmptyCategoryList;
}

// -----------------------------------------------------------------------------
bool UCFUploadWidget::UpdatePluginDescriptor(const FPluginDescriptor& PluginDescriptor, TSharedRef<IPlugin> Plugin) {
  FText FailReason;
  if (!Plugin->UpdateDescriptor(PluginDescriptor, FailReason)) {
    ShowConfirmationDialog(LOCTEXT("uploadpluginfail", "Couldn't Update Plugin"), FailReason);
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
const FCategory& UCFUploadWidget::GetRootCategoryByName(const FString& Name) const {
  for (const FCategory& Category : RootCategories) {
    if (Category.name == Name) {
      return Category;
    }
  }

  return InvalidCategory;
}

// -----------------------------------------------------------------------------
const FCategory& UCFUploadWidget::GetSubCategoryByName(const int64 ClassID, const FString& Name) const {
  if (!Categories.Contains(ClassID)) {
    return InvalidCategory;
  }

  for (const FCategory& Category : Categories[ClassID]) {
    if (Category.name == Name) {
      return Category;
    }
  }

  return InvalidCategory;
}

// -----------------------------------------------------------------------------
FCFModData UCFUploadWidget::GetModDataByName(const FString& Name) {
  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    if (AvailableMod->GetName() != Name) {
      continue;
    }

    return GetPluginData(AvailableMod);
  }

  return FCFModData();
}

// -----------------------------------------------------------------------------
ECFCoreAutoCookingType UCFUploadWidget::GetModAutoCookingTypeByName(const FString& Name) {
  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    if (AvailableMod->GetName() == Name) {
      return ExtractAutoCookingTypeFromPlugin(AvailableMod);
    }
  }

  UE_LOG(LogTemp, Warning, TEXT("Mod with name '%s' not found among available game mods."), *Name);
  return ECFCoreAutoCookingType::PCOnly;
}

// -----------------------------------------------------------------------------
FCFModData UCFUploadWidget::GetModById(const int32 Id) {
  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    FCFModData ModData = GetPluginData(AvailableMod);
    if (ModData.Id == Id) {
      return MoveTemp(ModData);
    }
  }

  return FCFModData();
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::InsertUgcIdIntoPlugin(
  TSharedRef<IPlugin> Plugin, int64 id) {

  FString PluginDescriptorPath = Plugin->GetDescriptorFileName();
  FString JsonContent;
  if (!TryLoadPluginDescriptor(JsonContent, PluginDescriptorPath)) {
    return;
  }

  TSharedPtr<FJsonObject> JsonObject;
  if (!TryParseJsonPluginDescriptor(JsonContent, JsonObject, PluginDescriptorPath)) {
    return;
  }

  JsonObject->SetNumberField(kUgcIdFieldName, id);

  // Write updated JSON back to file
  FString UpdatedJsonContent;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&UpdatedJsonContent);
  if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer)) {
    UE_LOG(LogTemp,
    Error,
    TEXT("Failed to serialize updated JSON for plugin descriptor: %s"),
    *PluginDescriptorPath);
    return;
  }

  if (!FFileHelper::SaveStringToFile(UpdatedJsonContent, *PluginDescriptorPath)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to save updated plugin descriptor: %s"), *PluginDescriptorPath);
    return;
  }
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::InsertAutoCookingTypeIntoPlugin(
  TSharedRef<class IPlugin> Plugin,
  ECFCoreAutoCookingType AutoCookingType) {

  FString PluginDescriptorPath = Plugin->GetDescriptorFileName();
  FString JsonContent;
  if (!TryLoadPluginDescriptor(JsonContent, PluginDescriptorPath)) {
    return;
  }

  TSharedPtr<FJsonObject> JsonObject;
  if (!TryParseJsonPluginDescriptor(
    JsonContent, JsonObject, PluginDescriptorPath)) {
    return;
    }

  const UEnum* EnumPtr = StaticEnum<ECFCoreAutoCookingType>();
  if (!EnumPtr) {
    UE_LOG(LogTemp, Error, TEXT("Failed to get StaticEnum for ECFCoreAutoCookingType."));
    return;
  }

  FString EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(AutoCookingType));
  int32 DoubleColonIndex;
  if (EnumName.FindLastChar(':', DoubleColonIndex)) {
    EnumName = EnumName.Mid(DoubleColonIndex + 1);
  }

  JsonObject->SetStringField(TEXT("AutoCookingType"), EnumName);

  FString UpdatedJsonContent;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&UpdatedJsonContent);
  if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to serialize updated JSON for plugin descriptor: %s"), *PluginDescriptorPath);
    return;
  }

  if (!FFileHelper::SaveStringToFile(UpdatedJsonContent, *PluginDescriptorPath)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to save updated plugin descriptor: %s"), *PluginDescriptorPath);
    return;
  }
}

// -----------------------------------------------------------------------------
bool UCFUploadWidget::IsAllContentSaved(TSharedRef<IPlugin> Plugin) {
  bool bAllContentSaved = true;

  TArray<UPackage*> UnsavedPackages;
  FEditorFileUtils::GetDirtyContentPackages(UnsavedPackages);
  FEditorFileUtils::GetDirtyWorldPackages(UnsavedPackages);

  if (UnsavedPackages.Num() > 0) {
    FString PluginBaseDir = Plugin->GetBaseDir();

    for (UPackage* Package : UnsavedPackages) {
      FString PackageFilename;
      if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename)) {
        if (PackageFilename.Find(PluginBaseDir) == 0) {
          bAllContentSaved = false;
          break;
        }
      }
    }
  }

  return bAllContentSaved;
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::SaveAndPackagePlugin(
  TSharedRef<IPlugin> Plugin,
  TArray<FCModPlatformData> BuildPlatforms) {

  const FString DefaultDirectory = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());

  // Prompt the user to save all dirty packages. We'll ensure that if any packages from the mod that the user wants to
  // package are dirty that they will not be able to save them.
  if (!IsAllContentSaved(Plugin)) {
    FEditorFileUtils::SaveDirtyPackages(true, true, true);
  }

  if (IsAllContentSaved(Plugin)) {
    void* ParentWindowWindowHandle = nullptr;
    IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
    const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
    if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid()) {
      ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
    }

    PackagePlugin(Plugin, DefaultDirectory, BuildPlatforms);
    return;
  }

  FText PackageModError = FText::Format(LOCTEXT("PackageUGCError_UnsavedContent", "You must save all assets in {0} before you can share."), FText::FromString(Plugin->GetName()));
  FMessageDialog::Open(EAppMsgType::Ok, PackageModError);
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::PackagePlugin(TSharedRef<class IPlugin> Plugin,
  const FString& OutputDirectory,
  TArray<FCModPlatformData>& BuildPlatforms) {
  if (!BuildPlatforms.Num()) {
    return;
  }

  const FText PlatformName = BuildPlatforms[0].PlatformName;
  const FString ReleaseVersion = TEXT("UGCExampleGame_v1");
  const FString ServerText = BuildPlatforms[0].bIsServer ? "true" : "false";
  const FString CommandLine = FString::Printf(TEXT("PackageUGC -Project=\"%s\" -PluginPath=\"%s\" -basedonreleaseversion=\"%s\" -StagingDirectory=\"%s\" -Server=\"%s\" -Platform=\"%s\" -nocompile"),
    *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),
    *FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName()),
    *ReleaseVersion,
    *OutputDirectory,
    *ServerText,
    *UEnum::GetDisplayValueAsText(BuildPlatforms[0].BuildPlatform).ToString());

  BuildPlatforms.RemoveAtSwap(0);
  const FText PackagingText = FText::Format(LOCTEXT("SimpleUGCEditor_PackagePluginTaskName", "Packaging {0}"), FText::FromString(Plugin->GetName()));
  IUATHelperModule::Get().CreateUatTask(
    CommandLine,
    PlatformName,
    PackagingText,
    PackagingText,
    FCFEditorStyle::Get().GetBrush(TEXT("cfeditor.ShareUGC")),
#if ENGINE_MAJOR_VERSION >= 5
    nullptr,
#endif
    [this, Plugin, OutputDirectory, BuildPlatforms](FString TaskResult, double TimeSec) {
      AsyncTask(ENamedThreads::GameThread, [this, TaskResult, Plugin, OutputDirectory, BuildPlatforms]() {
        if (TaskResult == "Completed") {
          if (BuildPlatforms.Num()) {
            PackagePlugin(
              Plugin,
              OutputDirectory,
              const_cast<TArray<FCModPlatformData>&>(BuildPlatforms));
          } else {
            OnModPackagingComplete();
          }
        }
        else {
          ShowConfirmationDialog(
            LOCTEXT("buildfailtitle", "Couldn't Build UGC"),
            LOCTEXT("buildfail", "Your ugc failed to build. Please check the output log for more information."));
          OnModPackagingFailed();
        }
        });
    });
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::ArchivePlugin(const FString& OutputDirectory,
                                    const FString& ZipFileName) {

  auto FilesToZip = MakeShared<TArray<FString>>(
    TArray<FString>{OutputDirectory});

  auto f = cfcore::CFCoreContext::GetInstance()->Utils()->Compression()->Zip(
    FilesToZip,
    ZipFileName,
    cfcore::ICompressionService::FProgressDelegate::CreateLambda(
      [this](const cfcore::CompressionProgress& Progress) {

      UE_LOG(LogTemp,
             Log,
             TEXT("Archive Plugin progress: %d%%"),
             Progress.progress);
    })
  );

  f.Next([this](ECompressionError Error) {
    if (Error == ECompressionError::None) {
      OnArchivePluginComplete();
      return;
    }

    OnArchivePluginFailed();
  });
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::NativeConstruct() {
  Super::NativeConstruct();

  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  for (TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    AddModDataToUI(GetPluginData(AvailableMod));
  }
}

// -----------------------------------------------------------------------------
bool UCFUploadWidget::TryLoadPluginDescriptor(FString& JsonContent,
                                              FString& PluginDescriptorPath) {

  if (!FFileHelper::LoadFileToString(JsonContent, *PluginDescriptorPath)) {
    UE_LOG(LogTemp,
      Error,
      TEXT("Failed to load plugin descriptor: %s"),
      *PluginDescriptorPath);
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
bool UCFUploadWidget::TryParseJsonPluginDescriptor(
  FString& JsonContent,
  TSharedPtr<FJsonObject>& JsonObject,
  FString& PluginDescriptorPath) {

  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
  if (!FJsonSerializer::Deserialize(
    Reader, JsonObject) || !JsonObject.IsValid()) {

    UE_LOG(LogTemp,
      Error,
      TEXT("Failed to parse JSON for plugin descriptor: %s"),
      *PluginDescriptorPath);
    return false;
  }
  return true;
}