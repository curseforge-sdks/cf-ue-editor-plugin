# cf-ue-editor-plugin

An Unreal Engine Editor Plugin for uploading mods to CurseForge

## Credits

This plugin was contributed by our friends at Blue Isle

## Getting started

- This plugin depends on the cfcore plugin - make sure the cfcore plugin resides
in the plugins directory. Contact us to get access.
- Download the latest release of this repo and place it into your Plugins directory
- Edit *consts.cpp* and set your Game ID, Api Key and Game friendly name
- Review *cfcore_sdk_service.cpp* - |InitializeAsync| and make sure the settings
are appropriate for your game editor
- Edit *cfeditor_module.cpp* - |StartupModule| and make sure you start an
appropriate AuthenticationProvider (we currently support Steam)
