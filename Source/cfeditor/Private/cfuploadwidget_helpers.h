// Copyright 2023 Blue Isle Studios Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "api/models/creation/create_cooked_mod_file_request.h"
#include "api/models/file_transfer_progress.h"
#include "cfuploadwidget.h"

struct FCookedUploadTask {
  FCModPlatformData BuildPlatform;
  FCreateCookedModFileRequest Request;
  FString PlatformFolderName;
  FString PackagedModDirectory;
  FString ArchiveFilename;
};

bool TryResolvePackagedModDirectory(const FString& PlatformPath,
                                    const FString& ExpectedModName,
                                    FString& OutPackagedModDirectory,
                                    FString& OutError);

FFileTransferProgress GetAggregatedProgress(
  const FFileTransferProgress& Progress,
  const int32 PlatformIndex,
  const int32 TotalPlatforms);

void LogCookedArchiveState(const TCHAR* Context, const FString& ArchivePath);

void CleanupCookedArchives(const TSharedRef<TArray<FString>>& ArchiveFiles);

void CleanupCookedArchivesOnFailure(const TSharedRef<TArray<FString>>& ArchiveFiles);

ECFCorePlatform MapBuildPlatformToCFCorePlatform(EBuildPlatforms BuildPlatform,
                                                 bool bIsServer);

bool BuildCookedUploadTasks(
  const TArray<FCModPlatformData>& BuildPlatforms,
  const FString& BasePath,
  const FString& SafeModName,
  const FString& SafeVersionForFilename,
  const UEnum* CFCorePlatformEnum,
  TArray<FCookedUploadTask>& OutTasks,
  FText& OutError);

void LogCookedUploadTasks(const TArray<FCookedUploadTask>& Tasks);
