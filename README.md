# cf-ue-editor-plugin

An Unreal Engine Editor Plugin for creating and uploading mods to CurseForge

## Credits

This plugin was contributed by our friends at Blue Isle

## Getting started

> **NOTE:** In order to be able to cook mods locally, you need a sourc ebuild of
> UE to recompile the AutomationTool project and support the PackageUGC command.
> See https://forums.unrealengine.com/t/inside-unreal-adding-mod-support-with-the-simple-ugc-plugin/147657
> for more information.

- This plugin depends on the **cfcore** plugin - make sure the cfcore plugin resides
in the plugins directory. Contact us to get access.
- Download the latest release of this repo and place it into your Plugins directory (name the folder **cfeditor**)
- Edit **consts.cpp** and set your Game ID, Api Key and Game friendly name
- Review **cfcore_sdk_service.cpp** - |InitializeAsync| and make sure the settings
are appropriate for your game editor
- Edit **cfeditor_module.cpp** - |StartupModule| and make sure you start an
appropriate AuthenticationProvider (the plugin currently supports Email, Steam
and a hard-coded test Steam provider)


## The User Interface

#### UE4 toolbar button:

![ue4 toolbar button](/docs/assets/images/ue4-toolbar-button.jpg)

#### UE5 menu entry:

![ue5 menu entry](/docs/assets/images/ue5-menu-entry.jpg)

#### Create mod on CurseForge

The first time an author uploads a mod file it will also create a new mod project on CurseForge

![create mod](/docs/assets/images/create-mod.jpg)

## Packaging

When a mod file is to be uploaded to the CurseForge servers, there are a few
possible flows:

- Local cooking
- Using the CurseForge Cloud cooking servers

### Local cooking

Right now, when the mod author clicks to upload their mod - the cfeditor plugin
will first package the mod and then upload it to the CurseForge servers.

Packaging locally is done by running the PackageUGC command (utilizing UAT):

- PackageUGC -Prepare="true" - to perform pre-cooking checks
- PackageUGC for each platform that is supported (e.g. Windows, Linux, Server...)
- PackageUGC -Archive="true" to zip the output package before uploading it

You may customize either the PackageUGC command, based on the Epic's UGCExample,
or change the code so that it runs other commands.

> **NOTE:** We can also provide you with a sample PackageUGC command that
> support archiving, please contact us for this sample.

After the the mod has been packaged, both the source and the cooked package have
to be uploaded to the CF servers. This is because a mod can be cooked to many
platforms and we use the source as the logical parent file for the cooked mods.

This is done by:

- Zipping the mod's source file (Utilizing PackageUGC -Archive="true")
- Uploading it via the cfcore SDK's CreateModFile API
- After uploading the source file, record the file's CF File ID and then
upload the cooked file(s) via the cfcore SDK's CreateCookedModFile API -
associating the cooked files with the source file (as their SourceFileId)

### Using the CurseForge Cloud Cooking servers

For cloud cooking, only the source file should be uploaded, this is done by:

- Zipping the mod's source file (Utilizing PackageUGC -Archive="true")
- Uploading it via the cfcore SDK's CreateModFile API with the
cookingOptions.isSourceFile parameter set to true

The cloud cooker will handle the cooking for the different platforms.

> **NOTE:** To support cloud cooking you must first contact us for setup
