# CHANGELOG
---
## Only For Major Versions and Releases

---

## v5.0.0
### PR: #11
### issue: #6 

### Some dependency management

- files: `CMakeLists.txt`

+ Removed unused and hard-coded dependencies

### New Config options & Tray tooltip.
- files: `traymanager.cpp, configmanager.cpp`

+ `Titlebar Menu >> Customize App >> Background Check`
+ `Titlebar Menu >> Customize App >> Show Tooltip`

+ Checking in background every `30` sec...

+ Indicator works when `Use Less Memory` is off or on

+ Show tray indicator toggle in menu bar.

+ Tray tooltip updates when new nessages are detected.


### Delete Log file when disabling the option
+ files: `logger.cpp`

+ Disabling the `Debug: File Logging` option will delete the log file.


### Other
+ Refactoring some stuff in `webenginehelper.cpp, mainwindow.cpp`

### Release Available.

----------------------------------------------------------------------------
## Can't describe changelog for versions before v5.0.0
### But for all later versions changelog must be updated.
+ Just copy-pasting previous release notes for lower versions.
----------------------------------------------------------------------------

## v4.0.0

### MAJOR REGRESSION
+ **Regression: ALL (Including Dark Mode) JS injection is removed.**
+ Titlebar menu option and Config file key value are also removed as well.

+ The app is more secure now (Not that it wasn't before...), offering a more vanilla experience.

+ Dark Mode can be directly enabled from whatsapp settings, and that setting is persistent anyway.

+ If any problem occurs, use `Reload Config and Cache` option. If problem persists, start an issue.

+ If you want to examine the changes, go through the corresponding commit. In particular, check `webenginehelper.cpp` for changes regarding JS.

### Qt Licensing
+ Added 3rd party licenses. check the LICENSES folder or README.

### Icons
+ Added icons for titlebar menu buttons.

### Release Available.

---

## v 3.0.1
### Title bar menu:
### System
+ Mute sounds (including notifications and media)

### Advanced
+ Use Less Memory - When hidden to tray, app consumes less memory by unloading the webpage.
+ Memory Kill switch - Checks its memory every 30 seconds and quits if memory usage is above the threshold.
+ Customize App Icon, Tray Icon and App URL.

### Others
+ Tray icon toggles show/hide window on click.
+ `Ctrl+Shift+Q` keybinding quits the app completely.

### Release Available.


### If the features of this release are not to your liking, use the previous versions.

---

## v2.0.0
### Title bar menu:
### General
- [X] Better dark mode toggle.
- [X] Remember Downloads path.

### View
- [X] Zoom controls.

### Window
- [X] Minimize to Tray on Close


### System
- [X] Autostart on login.
- [X] Start minimized in tray.
- [X] Notifications for new messages when app is out of focus or hidden.


### Advanced
- [X] Debugging to log file.
- [X] Reload cache and config.
- [X] Remove profile and restart.

### Others

- [X] Single instance behaviour.
- [X] Tray icon toggles show/hide window on click.
- [X] `Ctrl+q` keybinding quits/closes the app. 
- [X] hyperlinks open in external browser.


### Notes
- For `Prefer Dark Mode` toggle to work, you need to enable dark theme in whatsapp first.
- Go to `Whatsapp Settings -> Chats - > Theme` and choose `Dark`.
- If enabling the menu option is still breaking the view of the webpage, restart the app completely.

- Notifications will only show when the app is minimized, hidden, or out of view (in a different virtual desktop). 

### Release Available. 
+ However, It is not recommended to use. Very unripe.

---

## v1.0.1

### 1st Release.
+ Very Bare bones. Not recommended to use.