// Copyright 2023 Blue Isle Studios Inc. All Rights Reserved.
#include "cfuploadwidget_helpers.h"

#define LOCTEXT_NAMESPACE "FCFEditorModule"

// -----------------------------------------------------------------------------
bool TryResolvePackagedModDirectory(const FString& PlatformPath,
                                    const FString& ExpectedModName,
                                    FString& OutPackagedModDirectory,
                                    FString& OutError) {

  if (!FPaths::DirectoryExists(PlatformPath)) {
    OutError = FString::Printf(TEXT("Missing cooked output folder: %s"),
                               *PlatformPath);
    return false;
  }

  TArray<FString> UpluginFiles;
  IFileManager::Get().FindFilesRecursive(UpluginFiles,
                                         *PlatformPath,
                                         TEXT("*.uplugin"),
                                         true,
                                         false,
                                         false);

  if (UpluginFiles.Num() <= 0) {
    OutError = FString::Printf(
      TEXT("No .uplugin file found under cooked platform folder: %s"),
      *PlatformPath);
    return false;
  }

  UpluginFiles.Sort();

  auto IsExpectedMod = [&ExpectedModName](const FString& UpluginPath) {
    if (ExpectedModName.IsEmpty()) {
      return false;
    }

    return FPaths::GetBaseFilename(UpluginPath).Equals(
      ExpectedModName,
      ESearchCase::IgnoreCase);
  };

  for (const FString& UpluginPath : UpluginFiles) {
    if (IsExpectedMod(UpluginPath)) {
      OutPackagedModDirectory = FPaths::GetPath(UpluginPath);
      return true;
    }
  }

  if (UpluginFiles.Num() == 1) {
    OutPackagedModDirectory = FPaths::GetPath(UpluginFiles[0]);
    return true;
  }

  TArray<FString> UpluginFilesUnderMods;
  for (const FString& UpluginPath : UpluginFiles) {
    if (UpluginPath.Contains(TEXT("/Mods/"), ESearchCase::IgnoreCase) ||
        UpluginPath.Contains(TEXT("\\Mods\\"), ESearchCase::IgnoreCase)) {

      UpluginFilesUnderMods.Add(UpluginPath);
    }
  }

  if (UpluginFilesUnderMods.Num() == 1) {
    OutPackagedModDirectory = FPaths::GetPath(UpluginFilesUnderMods[0]);
    return true;
  }

  OutError = FString::Printf(
    TEXT("Found multiple .uplugin files under %s and could not select one "
         "automatically."),
    *PlatformPath);
  return false;
}

// -----------------------------------------------------------------------------
FFileTransferProgress GetAggregatedProgress(
  const FFileTransferProgress& Progress,
  const int32 PlatformIndex,
  const int32 TotalPlatforms) {

  FFileTransferProgress AggregatedProgress = Progress;
  if (TotalPlatforms <= 0) {
    return AggregatedProgress;
  }

  const float CurrentUploadProgress = FMath::Clamp((float)Progress.progress / 100.0f, 0.0f, 1.0f);
  const float OverallProgress = (PlatformIndex + CurrentUploadProgress) / (float)TotalPlatforms;
  AggregatedProgress.progress = FMath::Clamp(FMath::RoundToInt(OverallProgress * 100.0f), 0, 100);
  return AggregatedProgress;
}

// -----------------------------------------------------------------------------
void LogCookedArchiveState(const TCHAR* Context, const FString& ArchivePath) {
  const FString FullArchivePath = FPaths::ConvertRelativePathToFull(ArchivePath);
  const FString DirectoryPath = FPaths::GetPath(FullArchivePath);
  const bool bExists = FPaths::FileExists(FullArchivePath);
  const int64 SizeBytes = IFileManager::Get().FileSize(*FullArchivePath);

  UE_LOG(
    LogTemp,
    Log,
    TEXT("CookedArchiveState[%s] Path='%s' Exists=%s SizeBytes=%lld DirectoryExists=%s"),
    Context,
    *FullArchivePath,
    bExists ? TEXT("true") : TEXT("false"),
    SizeBytes,
    FPaths::DirectoryExists(DirectoryPath) ? TEXT("true") : TEXT("false"));
}

// -----------------------------------------------------------------------------
void CleanupCookedArchives(const TSharedRef<TArray<FString>>& ArchiveFiles) {
  IFileManager& FileManager = IFileManager::Get();
  for (const FString& ArchiveFile : *ArchiveFiles) {
    const FString FullArchiveFile = FPaths::ConvertRelativePathToFull(ArchiveFile);
    LogCookedArchiveState(TEXT("CleanupBeforeDelete"), FullArchiveFile);

    if (FPaths::FileExists(FullArchiveFile)) {
      const bool bDeleted = FileManager.Delete(*FullArchiveFile);
      UE_LOG(
        LogTemp,
        Log,
        TEXT("CookedArchiveCleanup Delete Path='%s' Deleted=%s"),
        *FullArchiveFile,
        bDeleted ? TEXT("true") : TEXT("false"));
    } else {
      UE_LOG(
        LogTemp,
        Warning,
        TEXT("CookedArchiveCleanup SkipMissing Path='%s'"),
        *FullArchiveFile);
    }

    LogCookedArchiveState(TEXT("CleanupAfterDelete"), FullArchiveFile);
  }
  ArchiveFiles->Reset();
}

// -----------------------------------------------------------------------------
void CleanupCookedArchivesOnFailure(const TSharedRef<TArray<FString>>& ArchiveFiles) {
  constexpr bool bCleanupCookedArchivesOnFailure = false;
  if (!bCleanupCookedArchivesOnFailure) {
    UE_LOG(
      LogTemp,
      Warning,
      TEXT("Cooked upload failed. Preserving cooked archives on disk for debugging."));
    return;
  }

  CleanupCookedArchives(ArchiveFiles);
}

// -----------------------------------------------------------------------------
ECFCorePlatform MapBuildPlatformToCFCorePlatform(EBuildPlatforms BuildPlatform,
                                                 bool bIsServer) {
  switch (BuildPlatform) {
  case EBuildPlatforms::WIN:
    return bIsServer ? ECFCorePlatform::WindowsServer
                     : ECFCorePlatform::Windows;
  case EBuildPlatforms::LINUX:
    return bIsServer ? ECFCorePlatform::LinuxServer
                     : ECFCorePlatform::Linux;
  case EBuildPlatforms::PS4:  return ECFCorePlatform::PS4;
  case EBuildPlatforms::PS5:  return ECFCorePlatform::PS5;
  case EBuildPlatforms::XB1:  return ECFCorePlatform::XboxOne;
  case EBuildPlatforms::XSX:  return ECFCorePlatform::XboxXS;
  case EBuildPlatforms::MAC:  return ECFCorePlatform::Mac;
  default:                    return ECFCorePlatform::None;
  }
}

// -----------------------------------------------------------------------------
bool BuildCookedUploadTasks(
  const TArray<FCModPlatformData>& BuildPlatforms,
  const FString& BasePath,
  const FString& SafeModName,
  const FString& SafeVersionForFilename,
  const UEnum* CFCorePlatformEnum,
  TArray<FCookedUploadTask>& OutTasks,
  FText& OutError) {

  OutTasks.Reserve(BuildPlatforms.Num());

  for (const FCModPlatformData& BuildPlatform : BuildPlatforms) {
    const ECFCorePlatform CFCorePlatform =
      MapBuildPlatformToCFCorePlatform(BuildPlatform.BuildPlatform,
                                       BuildPlatform.bIsServer);

    if (CFCorePlatform == ECFCorePlatform::None) {
      OutError = FText::Format(
        LOCTEXT("CookedUploadUnsupportedPlatform",
                "Unsupported build platform for cooked "
                "upload: {0}."),
        BuildPlatform.PlatformName);
      return false;
    }

    const FString PlatformFolderName =
      CFCorePlatformEnum->GetNameStringByValue(
        static_cast<int64>(CFCorePlatform));

    const FString PlatformPath =
      FPaths::Combine(BasePath, PlatformFolderName);

    if (!FPaths::DirectoryExists(PlatformPath)) {
      OutError = FText::Format(
        LOCTEXT("CookedUploadMissingPlatformFolder",
                "Missing cooked output folder:\n{0}"),
        FText::FromString(PlatformPath));
      return false;
    }

    FString PackagedModDirectory;
    FString ResolveError;
    if (!TryResolvePackagedModDirectory(
          PlatformPath, SafeModName,
          PackagedModDirectory, ResolveError)) {
      OutError = FText::FromString(ResolveError);
      return false;
    }

    FCookedUploadTask Task;
    Task.BuildPlatform = BuildPlatform;
    Task.Request.platform = CFCorePlatform;
    Task.PackagedModDirectory = PackagedModDirectory;
    Task.PlatformFolderName = PlatformFolderName;
    Task.ArchiveFilename =
      FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BasePath,
        FString::Printf(TEXT("%s-%s-%s.zip"),
                         *SafeModName,
                         *SafeVersionForFilename,
                         *PlatformFolderName)));
    OutTasks.Add(MoveTemp(Task));
  }

  return true;
}

// -----------------------------------------------------------------------------
void LogCookedUploadTasks(const TArray<FCookedUploadTask>& Tasks) {
  UE_LOG(LogTemp, Log,
    TEXT("CookedUpload QueuedPlatforms Count=%d"),
    Tasks.Num());

  for (int32 i = 0; i < Tasks.Num(); ++i) {
    const FCookedUploadTask& Task = Tasks[i];
    UE_LOG(LogTemp, Log,
      TEXT("CookedUpload Task[%d] Platform='%s' "
           "SourceDir='%s' Archive='%s'"),
      i,
      *Task.PlatformFolderName,
      *FPaths::ConvertRelativePathToFull(
        Task.PackagedModDirectory),
      *FPaths::ConvertRelativePathToFull(
        Task.ArchiveFilename));
  }
}

#undef LOCTEXT_NAMESPACE
