# ps3EyeSyphon_2

`ps3EyeSyphon_2` captures one or more PlayStation 3 Eye cameras and publishes each
camera as a Syphon server on macOS.

This fork has been updated for openFrameworks 0.12.x and Apple Silicon builds.

## Features

- PS3 Eye capture through `ofxPS3EyeGrabber`
- one Syphon output per connected camera
- GUI controls for draw enable, flip, exposure, gain, brightness, contrast,
  sharpness, hue, and white balance
- OSC control for common camera settings
- default capture mode: `640x480 @ 60 fps`
- custom app icon included as `icon.icns`

## Requirements

- macOS
- Xcode
- openFrameworks `0.12.1` for macOS
- PS3 Eye camera
- Syphon-compatible receiver app

Required addons:

- `ofxGui`
- `ofxOsc`
- `ofxKinect`
- `ofxOpenCv`
- `ofxPS3EyeGrabber`
- `ofxSyphon`
- `ofxXmlSettings`

Use the matching `ofxPS3EyeGrabber` fork:

```sh
git clone https://github.com/Elc3r/ofxPS3EyeGrabber.git addons/ofxPS3EyeGrabber
```

## Project Layout

The app is expected to live inside an openFrameworks tree:

```text
of_v0.12.1_osx_release/
  apps/
    myApps/
      ps3EyeSyphon_2/
```

The project uses relative openFrameworks paths (`../../..`) in `Makefile` and
`config.make`.

## Building

Open the Xcode project:

```sh
open ps3EyeSyphon_2.xcodeproj
```

Build one of the included schemes:

- `ps3EyeSyphon_2 Debug`
- `ps3EyeSyphon_2 Release`

Command-line build:

```sh
xcodebuild -project ps3EyeSyphon_2.xcodeproj \
  -scheme "ps3EyeSyphon_2 Debug" \
  -configuration Debug build
```

Build output is written to `bin/`.

## Settings

Camera startup settings are stored in:

```text
bin/data/cameraSettings.xml
```

Current defaults:

```xml
<CAMWIDTH>640</CAMWIDTH>
<CAMHEIGHT>480</CAMHEIGHT>
<FRAMERATE>60</FRAMERATE>
```

GUI state is stored in:

```text
bin/data/settings.xml
```

Inside the app:

- press `g` to show/hide the GUI
- press `s` to save GUI settings
- press `r` to reload GUI settings
- press `m` to toggle minimized mode
