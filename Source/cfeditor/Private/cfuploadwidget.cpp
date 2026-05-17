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
#include "cfuploadwidget_helpers.h"

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
  IMainFrameModule& MainFrameModule = 
    FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

  const TSharedPtr<SWindow>& MainFrameParentWindow = 
    MainFrameModule.GetParentWindow();

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

      // A value of 0 represents single file selection while a value of 1
      // represents multiple file selection
      uint32 SelectionFlag = 0;
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
void UCFUploadWidget::UpdatePluginWithModData(
  const FCFCoreMod& Mod,
  ECFCoreAutoCookingType AutoCookingType) {

  TArray<TSharedRef<IPlugin>> OutAvailableGameMods;
  FModsLoader::FindAvailableGameMods(OutAvailableGameMods);

  TSharedPtr<IPlugin> plugin;
  for (const TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
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
  for (const TSharedRef<IPlugin> AvailableMod : OutAvailableGameMods) {
    const FCFModData ModData = GetPluginData(AvailableMod);
    if (ModData.Id != ModID) {
      continue;
    }

    const FString PluginPath = FPaths::ConvertRelativePathToFull(AvailableMod->GetBaseDir());
    const FString CleanupPath = Path.IsEmpty() ? PluginPath : Path;

    FPluginDescriptor Descriptor = AvailableMod->GetDescriptor();
    Descriptor.Version++;
    Descriptor.VersionName = Version.IsEmpty() ? FGuid::NewGuid().ToString() : Version;
    Descriptor.Category = "UGC";
    if (!UpdatePluginDescriptor(Descriptor, AvailableMod)) {
      OnModPackagingFailed();
      return;
    }

    SaveAndPackagePlugin(AvailableMod, BuildPlatforms);
    return;
  }

  ShowConfirmationDialog(
    LOCTEXT("PackageFailureModNotFoundTitle", "Package Failure"),
    FText::Format(
      LOCTEXT("PackageFailureModNotFound", "Could not find a mod with id {0}."),
      FText::AsNumber(ModID)));
  OnModPackagingFailed();
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::ShowCookedUploadFailure(const FText& ErrorText) {
  ShowConfirmationDialog(
    LOCTEXT("CookedUploadFailureTitle", "Cooked Upload Failure"),
    ErrorText);
  OnCookedModFilesUploadFailed();
}

// -----------------------------------------------------------------------------
bool UCFUploadWidget::ValidateCookedUploadInputs(
  const int64 FileId,
  const FString& Path,
  const TArray<FCModPlatformData>& BuildPlatforms) {

  if (BuildPlatforms.Num() <= 0) {
    ShowCookedUploadFailure(
      LOCTEXT("CookedUploadNoPlatforms",
              "No build platforms were selected for "
              "cooked upload."));
    return false;
  }

  if (FileId > (uint64)MAX_int64) {
    ShowCookedUploadFailure(
      LOCTEXT("CookedUploadInvalidSourceFileId",
              "The source file id is out of range."));
    return false;
  }

  if (!FPaths::DirectoryExists(Path)) {
    ShowCookedUploadFailure(
      LOCTEXT("CookedUploadInvalidPath",
              "The cooked output path does not exist."));
    return false;
  }

  if (!cfcore::CFCoreContext::GetInstance()->IsInitialized()) {
    ShowCookedUploadFailure(
      LOCTEXT("CookedUploadNotInitialized",
              "CFCore is not initialized."));
    return false;
  }

  return true;
}

// TODO(apliscov): Refactor this function - the recursive pattern makes
// it hard to follow.
// -----------------------------------------------------------------------------
void UCFUploadWidget::UploadCookedModFilesWithSettings(
  const int64 FileId,
  const FString& Version,
  const FString& Path,
  TArray<FCModPlatformData> BuildPlatforms,
  const int64 ModID) {

  if (!ValidateCookedUploadInputs(FileId, Path, BuildPlatforms)) {
    return;
  }

  const UEnum* CFCorePlatformEnum = StaticEnum<ECFCorePlatform>();
  if (CFCorePlatformEnum == nullptr) {
    ShowCookedUploadFailure(
      LOCTEXT("CookedUploadPlatformEnumMissing",
              "Unable to resolve platform enum names."));
    return;
  }

  const FString ModName = FPaths::GetCleanFilename(Path);
  const FString SafeModName = ModName.IsEmpty() ? TEXT("Mod") : ModName;
  const FString SafeVersion = Version.IsEmpty() ? TEXT("1.0") : Version;
  const FString SafeVersionForFilename = FPaths::MakeValidFileName(SafeVersion);

  TArray<FCookedUploadTask> UploadTasks;
  FText BuildError;

  bool bBuildTasksSuccess = BuildCookedUploadTasks(
    BuildPlatforms,
    Path,
    SafeModName,
    SafeVersionForFilename,
    CFCorePlatformEnum,
    UploadTasks,
    BuildError);

  if (!bBuildTasksSuccess) {
    ShowCookedUploadFailure(BuildError);
    return;
  }

  LogCookedUploadTasks(UploadTasks);

  TWeakObjectPtr<UCFUploadWidget> WeakThis(this);
  const int64 SourceFileId = (int64)FileId;
  TSharedRef<TArray<FString>> ArchiveFiles = MakeShared<TArray<FString>>();
  TSharedPtr<TFunction<void(int32)>> UploadNextPlatform = MakeShared<TFunction<void(int32)>>();
  TWeakPtr<TFunction<void(int32)>> WeakUploadNextPlatform = UploadNextPlatform;
  ActiveCookedUploadNextPlatform = UploadNextPlatform;

  *UploadNextPlatform = [WeakThis,
                         UploadTasks,
                         ArchiveFiles,
                         WeakUploadNextPlatform,
                         ModID,
                         FileId](int32 PlatformIndex) {

    if (!WeakThis.IsValid()) {
      return;
    }

    if (PlatformIndex >= UploadTasks.Num()) {
      AsyncTask(ENamedThreads::GameThread, [WeakThis, ArchiveFiles]() {
        if (WeakThis.IsValid()) {
          WeakThis->HandleCookedUploadAllComplete(ArchiveFiles);
        }
      });
      return;
    }

    const FCookedUploadTask CurrentTask = UploadTasks[PlatformIndex];
    const FString SourceDirectory = FPaths::ConvertRelativePathToFull(CurrentTask.PackagedModDirectory);
    const FString TargetArchivePath = FPaths::ConvertRelativePathToFull(CurrentTask.ArchiveFilename);

    UE_LOG(
      LogTemp,
      Log,
      TEXT("CookedUpload ZipStart Platform='%s' SourceDir='%s' SourceExists=%s TargetArchive='%s'"),
      *CurrentTask.PlatformFolderName,
      *SourceDirectory,
      FPaths::DirectoryExists(SourceDirectory) ? TEXT("true") : TEXT("false"),
      *TargetArchivePath);

    LogCookedArchiveState(TEXT("BeforeZip"), TargetArchivePath);

    IFileManager& FileManager = IFileManager::Get();
    if (FPaths::FileExists(TargetArchivePath)) {
      const bool bDeleted = FileManager.Delete(*TargetArchivePath);
      UE_LOG(
        LogTemp,
        Log,
        TEXT("CookedUpload DeleteExistingArchive Path='%s' Deleted=%s"),
        *TargetArchivePath,
        bDeleted ? TEXT("true") : TEXT("false"));
    }

    auto FilesToZip = MakeShared<TArray<FString>>(TArray<FString>{ SourceDirectory });
    auto ZipFuture = cfcore::CFCoreContext::GetInstance()->Utils()->Compression()->Zip(
      FilesToZip,
      TargetArchivePath,
      cfcore::ICompressionService::FProgressDelegate::CreateLambda(
        [CurrentTask](const FCompressionProgress& Progress) {
          UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Archive cooked platform %s progress: %d%%"),
            *CurrentTask.PlatformFolderName,
            Progress.progress);
        }
      )
    );

    ZipFuture.Next([WeakThis,
                    UploadTasks,
                    ArchiveFiles,
                    WeakUploadNextPlatform,
                    CurrentTask,
                    PlatformIndex,
                    ModID,
                    FileId](ECompressionError CompressionError) {

      if (!WeakThis.IsValid()) {
        return;
      }

      UE_LOG(
        LogTemp,
        Log,
        TEXT("CookedUpload ZipComplete Platform='%s' CompressionError=%d"),
        *CurrentTask.PlatformFolderName,
        static_cast<int32>(CompressionError));
      LogCookedArchiveState(TEXT("AfterZip"), CurrentTask.ArchiveFilename);

      if (CompressionError != ECompressionError::None) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, ArchiveFiles, CurrentTask]() {
          if (!WeakThis.IsValid()) {
            return;
          }

          WeakThis->ActiveCookedUploadNextPlatform.Reset();
          CleanupCookedArchivesOnFailure(ArchiveFiles);
          const FText ErrorText = FText::Format(
            LOCTEXT("CookedUploadZipFailed", "Failed to package cooked files for platform {0}."),
            CurrentTask.BuildPlatform.PlatformName);
          WeakThis->ShowConfirmationDialog(
            LOCTEXT("CookedUploadFailureTitle", "Cooked Upload Failure"),
            ErrorText);
          WeakThis->OnCookedModFilesUploadFailed();
        });
        return;
      }

      AsyncTask(ENamedThreads::GameThread, [WeakThis,
                                            UploadTasks,
                                            ArchiveFiles,
                                            WeakUploadNextPlatform,
                                            CurrentTask,
                                            PlatformIndex,
                                            ModID,
                                            FileId]() {
        if (WeakThis.IsValid()) {
          WeakThis->HandleCookedZipComplete(
            UploadTasks, ArchiveFiles, WeakUploadNextPlatform,
            CurrentTask, PlatformIndex, ModID, FileId);
        }
      });
    });
  };

  (*UploadNextPlatform)(0);
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::HandleCookedUploadAllComplete(
  const TSharedRef<TArray<FString>>& ArchiveFiles) {

  ActiveCookedUploadNextPlatform.Reset();
  FFileTransferProgress CompletedProgress;
  CompletedProgress.progress = 100;
  OnCookedModFilesUploadProgress(CompletedProgress);
  OnCookedModFilesUploadComplete();
  CleanupCookedArchives(ArchiveFiles);
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::HandleCookedZipComplete(
  const TArray<FCookedUploadTask>& UploadTasks,
  const TSharedRef<TArray<FString>>& ArchiveFiles,
  const TWeakPtr<TFunction<void(int32)>>& WeakUploadNextPlatform,
  const FCookedUploadTask& CurrentTask,
  int32 PlatformIndex,
  int64 ModID,
  int64 FileId) {

  if (!cfcore::CFCoreContext::GetInstance()->IsInitialized()) {
    ActiveCookedUploadNextPlatform.Reset();
    CleanupCookedArchivesOnFailure(ArchiveFiles);
    ShowConfirmationDialog(
      LOCTEXT("CookedUploadFailureTitle", "Cooked Upload Failure"),
      LOCTEXT("CookedUploadNotInitialized", "CFCore is not initialized."));
    OnCookedModFilesUploadFailed();
    return;
  }

  const FString ExpectedArchivePath = FPaths::ConvertRelativePathToFull(CurrentTask.ArchiveFilename);
  const int64 ArchiveSizeBytes = IFileManager::Get().FileSize(*ExpectedArchivePath);
  if (!FPaths::FileExists(ExpectedArchivePath) || ArchiveSizeBytes <= 0) {
    TArray<FString> MatchingArchivePaths;
    const FString ArchiveSearchRoot = FPaths::GetPath(ExpectedArchivePath);
    if (FPaths::DirectoryExists(ArchiveSearchRoot)) {
      IFileManager::Get().FindFilesRecursive(
        MatchingArchivePaths,
        *ArchiveSearchRoot,
        *FPaths::GetCleanFilename(ExpectedArchivePath),
        true,
        false,
        false);
    }

    const FString DiscoveryMessage = MatchingArchivePaths.Num() <= 0
      ? TEXT("No archive with this name was found under the expected output directory.")
      : FString::Printf(
        TEXT("Archive with matching name was found at:\n%s"),
        *FString::Join(MatchingArchivePaths, TEXT("\n")));

    UE_LOG(
      LogTemp,
      Error,
      TEXT("Cooked archive missing/invalid. Expected='%s' Size=%lld. %s"),
      *ExpectedArchivePath,
      ArchiveSizeBytes,
      *DiscoveryMessage);

    ActiveCookedUploadNextPlatform.Reset();
    CleanupCookedArchivesOnFailure(ArchiveFiles);
    const FText ErrorText = FText::Format(
      LOCTEXT(
        "CookedUploadArchiveMissing",
        "Cooked archive was not created at the expected path.\nExpected:\n{0}\n\n{1}"),
      FText::FromString(ExpectedArchivePath),
      FText::FromString(DiscoveryMessage));
    ShowConfirmationDialog(
      LOCTEXT("CookedUploadFailureTitle", "Cooked Upload Failure"),
      ErrorText);
    OnCookedModFilesUploadFailed();
    return;
  }

  UE_LOG(
    LogTemp,
    Log,
    TEXT("CookedUpload CreateCookedModFile Platform='%s' ModId=%lld FileId=%lld Archive='%s' SizeBytes=%lld"),
    *CurrentTask.PlatformFolderName,
    ModID,
    FileId,
    *ExpectedArchivePath,
    ArchiveSizeBytes);
  LogCookedArchiveState(TEXT("BeforeCreateCookedModFile"), ExpectedArchivePath);

  TWeakObjectPtr<UCFUploadWidget> WeakThis(this);
  ArchiveFiles->Add(ExpectedArchivePath);
  cfcore::CFCoreContext::GetInstance()->Creation()->CreateCookedModFile(
    ModID,
    FileId,
    CurrentTask.Request,
    ExpectedArchivePath,
    cfcore::ICFCoreCreation::FCreateModFileRequestIdDelegate::CreateLambda(
      [CurrentTask, ExpectedArchivePath](int64 FileRequestId) {
        UE_LOG(
          LogTemp,
          Log,
          TEXT("Started cooked upload for %s. RequestId=%lld Archive='%s'"),
          *CurrentTask.PlatformFolderName,
          FileRequestId,
          *ExpectedArchivePath);
      }),
    cfcore::ICFCoreCreation::FFileTransferProgressDelegate::CreateLambda(
      [WeakThis, PlatformIndex, TotalPlatforms = UploadTasks.Num()](const FFileTransferProgress& Progress) {
        if (!WeakThis.IsValid()) {
          return;
        }

        const FFileTransferProgress AggregatedProgress = GetAggregatedProgress(
          Progress,
          PlatformIndex,
          TotalPlatforms);

        AsyncTask(ENamedThreads::GameThread, [WeakThis, AggregatedProgress]() {
          if (WeakThis.IsValid()) {
            WeakThis->OnCookedModFilesUploadProgress(AggregatedProgress);
          }
        });
      }),
    cfcore::ICFCoreCreation::FCreateModFileDelegate::CreateLambda(
      [WeakThis,
       ArchiveFiles,
       WeakUploadNextPlatform,
       CurrentTask,
       PlatformIndex](const TOptional<FUploadedModFile>& OptUploadedModFile,
                      const TOptional<FCFCoreError>& OptError) {

        AsyncTask(ENamedThreads::GameThread, [WeakThis,
                                              ArchiveFiles, 
                                              WeakUploadNextPlatform, 
                                              CurrentTask,
                                              PlatformIndex,
                                              OptUploadedModFile,
                                              OptError]() {
          if (WeakThis.IsValid()) {
            WeakThis->HandleCookedUploadFileComplete(
              ArchiveFiles, WeakUploadNextPlatform,
              CurrentTask, PlatformIndex,
              OptUploadedModFile, OptError);
          }
        });
      }
    )
  );
}

// -----------------------------------------------------------------------------
void UCFUploadWidget::HandleCookedUploadFileComplete(
  const TSharedRef<TArray<FString>>& ArchiveFiles,
  const TWeakPtr<TFunction<void(int32)>>& WeakUploadNextPlatform,
  const FCookedUploadTask& CurrentTask,
  int32 PlatformIndex,
  const TOptional<FUploadedModFile>& OptUploadedModFile,
  const TOptional<FCFCoreError>& OptError) {

  if (OptError.IsSet()) {
    UE_LOG(
      LogTemp,
      Error,
      TEXT("CookedUpload CreateCookedModFile failed Platform='%s' Archive='%s'"
           " CFCoreCode = %d ApiErrorCode = %d Description = '%s'"),
      *CurrentTask.PlatformFolderName,
      *CurrentTask.ArchiveFilename,
      static_cast<int32>(OptError->code),
      OptError->apiError.errorCode,
      *OptError->description);
    LogCookedArchiveState(TEXT("OnUploadErrorBeforeCleanup"), CurrentTask.ArchiveFilename);

    ActiveCookedUploadNextPlatform.Reset();
    CleanupCookedArchivesOnFailure(ArchiveFiles);
    const FString ErrorDescription = OptError->description.IsEmpty()
      ? TEXT("Unknown upload error.")
      : OptError->description;
    const FText ErrorText = FText::Format(
      LOCTEXT("CookedUploadApiFailed", "Failed to upload cooked file for platform {0}.\n{1}"),
      CurrentTask.BuildPlatform.PlatformName,
      FText::FromString(ErrorDescription));
    ShowConfirmationDialog(
      LOCTEXT("CookedUploadFailureTitle", "Cooked Upload Failure"),
      ErrorText);
    OnCookedModFilesUploadFailed();
    return;
  }

  if (OptUploadedModFile.IsSet()) {
    UE_LOG(
      LogTemp,
      Log,
      TEXT("Finished cooked upload for %s. UploadedFileId=%lld"),
      *CurrentTask.PlatformFolderName,
      OptUploadedModFile->fileId);
  }

  TSharedPtr<TFunction<void(int32)>> NextPlatform = WeakUploadNextPlatform.Pin();
  if (NextPlatform) {
    (*NextPlatform)(PlatformIndex + 1);
  }
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
  
  const FString PluginPath = ModData.Path;
  if (!FPaths::DirectoryExists(PluginPath)) {
    ShowConfirmationDialog(
      LOCTEXT("CleanPluginFolderInvalidPathTitle", "Cleanup Failed"),
      LOCTEXT("CleanPluginFolderInvalidPath", "Plugin path does not exist."));
    return;
  }

  TSet<FString> DirectoryNamesToDelete;
  DirectoryNamesToDelete.Add(TEXT("Saved"));

  const UEnum* CFCorePlatformEnum = StaticEnum<ECFCorePlatform>();
  if (CFCorePlatformEnum) {
    for (int32 Index = 0; Index < CFCorePlatformEnum->NumEnums(); ++Index) {
      const int64 Value = CFCorePlatformEnum->GetValueByIndex(Index);
      if (Value == INDEX_NONE || Value == static_cast<int64>(ECFCorePlatform::None)) {
        continue;
      }

      const FString EnumName = CFCorePlatformEnum->GetNameStringByValue(Value);
      if (!EnumName.IsEmpty() && !EnumName.EndsWith(TEXT("_MAX"))) {
        DirectoryNamesToDelete.Add(EnumName);
      }
    }
  }

  const UEnum* BuildPlatformEnum = StaticEnum<EBuildPlatforms>();
  if (BuildPlatformEnum) {
    for (int32 Index = 0; Index < BuildPlatformEnum->NumEnums(); ++Index) {
      const int64 Value = BuildPlatformEnum->GetValueByIndex(Index);
      if (Value == INDEX_NONE) {
        continue;
      }

      const FString EnumName = BuildPlatformEnum->GetNameStringByValue(Value);
      if (!EnumName.IsEmpty() && !EnumName.EndsWith(TEXT("_MAX"))) {
        DirectoryNamesToDelete.Add(EnumName);

        if (EnumName.Equals(TEXT("WIN"), ESearchCase::IgnoreCase) ||
          EnumName.Equals(TEXT("LINUX"), ESearchCase::IgnoreCase)) {
          DirectoryNamesToDelete.Add(EnumName + TEXT("Server"));
        }
      }
    }
  }

  IFileManager& FileManager = IFileManager::Get();
  TArray<FString> FailedDirectories;
  for (const FString& DirectoryName : DirectoryNamesToDelete) {
    const FString DirectoryPath = FPaths::Combine(PluginPath, DirectoryName);
    if (!FPaths::DirectoryExists(DirectoryPath)) {
      continue;
    }

    const bool bDeleted = FileManager.DeleteDirectory(*DirectoryPath, false, true);
    if (!bDeleted && FPaths::DirectoryExists(DirectoryPath)) {
      FailedDirectories.Add(DirectoryPath);
    }
  }

  if (FailedDirectories.Num() > 0) {
    ShowConfirmationDialog(
      LOCTEXT("CleanPluginFolderPartialFailureTitle", "Cleanup Incomplete"),
      FText::Format(
        LOCTEXT("CleanPluginFolderPartialFailure", "Failed to remove the following folders:\n{0}"),
        FText::FromString(FString::Join(FailedDirectories, TEXT("\n")))));
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
  const FString ReleaseVersion = TEXT("Istari");
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
            const FString ZipFileName = OutputDirectory / FPaths::GetBaseFilename(OutputDirectory) + ".zip";
            ArchivePlugin(OutputDirectory, ZipFileName);
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
                                    const FString& ZipFileName,
                                    bool bNotifyCallbacks) {

  auto FilesToZip = MakeShared<TArray<FString>>(
    TArray<FString>{OutputDirectory});

  auto f = cfcore::CFCoreContext::GetInstance()->Utils()->Compression()->Zip(
    FilesToZip,
    ZipFileName,
    cfcore::ICompressionService::FProgressDelegate::CreateLambda(
      [this](const FCompressionProgress& Progress) {

      UE_LOG(LogTemp,
             Log,
             TEXT("Archive Plugin progress: %d%%"),
             Progress.progress);
    })
  );

  f.Next([this, bNotifyCallbacks](ECompressionError Error) {
    if (!bNotifyCallbacks) {
      return;
    }

    if (Error == ECompressionError::None) {
      OnModPackagingComplete();
      return;
    }

    OnModPackagingFailed();
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

#undef LOCTEXT_NAMESPACE
