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
using UnrealBuildTool;

public class cfeditor : ModuleRules {
  public cfeditor(ReadOnlyTargetRules Target) : base(Target) {
    PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

#if UE_4_24_OR_LATER
		DefaultBuildSettings = BuildSettingsVersion.V2;
#endif

    PublicIncludePaths.AddRange(
      new string[] {
				// ... add public include paths required here ...
			}
      );


    PrivateIncludePaths.AddRange(
      new string[] {
				// ... add other private include paths required here ...
			}
      );


    PublicDependencyModuleNames.AddRange(
      new string[]
      {
        "Core",
        "cfcore",
				// ... add other public dependencies that you statically link with here ...
			}
      );


    PrivateDependencyModuleNames.AddRange(
      new string[] {
        "Projects",
        "InputCore",
        "UnrealEd",
        "LevelEditor",
        "CoreUObject",
        "Engine",
        "Slate",
        "SlateCore",
        "OnlineSubsystem",
        "OnlineSubsystemUtils",
        "OnlineSubsystemSteam",
        "UMGEditor",
        "Blutility",
        "UMG",
        "DesktopPlatform",
        "UATHelper",
        "Json",
        "JsonUtilities",
				// ... add private dependencies that you statically link with here ...
			});


    DynamicallyLoadedModuleNames.AddRange(
      new string[] {
				// ... add any modules that your module loads dynamically here ...
			});
  }
}
