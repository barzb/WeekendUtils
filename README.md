## WeekendUtils

**Relevant documentation can be found here:**
- [Game Service Framework](https://github.com/barzb/UnrealWeekendUtils/wiki/Game-Service-Framework)
- [Save Game Framework](https://github.com/barzb/UnrealWeekendUtils/wiki/Save-Game-Framework)

### **Update notes: December 2025**
**Breaking Changes to the GameService framework!**
- ```UGameServiceManager``` is not a ```UEngineSubsystem``` anymore!
  - Instead, it is now tied to a ```UGameInstance```, so local service environments in multiplayer PIE sessions are supported. This is in preparation for a bigger multiplayer support of the framework (TBD).
  - ```FGameServiceUser``` derived classes must now implement ```ConfigureGameServiceUser()``` to configure their dependencies, instead of doing so in their constructor.
    ```
    virtual FGameServiceUserConfig UExample::ConfigureGameServiceUser() const override
    {
      return FGameServiceUserConfig(this)
        .AddServiceDependency<USaveGameService>();
    }
    ```
  - ```UseGameService()``` and similar methods do not need to pass ```this``` anymore.
    ```
    void UExample::DoSomething()
    {
      UseGameService<USaveGameService>().RequestAutosave("example");
    }
    ```
- Used dependencies in ```FGameServiceUser``` derived classes are now cached after first look-up.
- Renamed some static getters in ```UGameServiceManager``` and ```UModularSaveGame```.
- Fixed and improved some broken unit tests and adjusted ```FScopedAutomationTestWorld``` to accommodate for the changes in the GameService framework.
- ```UGameServiceLocator``` now needs a ```WorldContext``` passed for all static locator methods.