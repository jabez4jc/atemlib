# Blackmagic ATEM C# Library

## Introduction

This library is a collection of tools for working with Blackmagic ATEM video switchers. It is intended to be used as part of automation solutions.

`MediaUpload` allows you to upload images to specific slots in a BlackMagic ATEM switcher's media pool.
`MediaPool` lists all the media in the switcher's media pool.

## Usage

### Media Upload

```
    mediaupload [options] <hostname> <slot> <filename>

Arguments:

 hostname            - The hostname or IP address of the switcher
 slot                - The slot to upload to
 filename            - The filename of the image to upload

Options:

 -h, --help          - Help information
 -d, --debug         - Enable debug output
 -v, --version       - View version information
 -n, --name          - Set the name of the image in the media pool
```

Example:

To upload myfile.png to Slot 1 on a switcher at 192.168.0.254:

    mediaupload 192.168.0.254 1 myfile.png

### Media Pool

```
    mediapool [options] <hostname>

Arguments:

 hostname        - The hostname or IP of the ATEM switcher

Options:

 -h, --help      - This help message
 -d, --debug     - Debug output
 -v, --version   - Version information
 -f, --format    - The output format. Either xml, csv, json or text
```

Example:

To see what's in the media pool for a switcher at 192.168.0.254:

    mediapool 192.168.0.254

To view the output in JSON format:

    mediapool -f json 192.168.0.254

## Requirements

 - [.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0)
 - [Blackmagic ATEM Software Control + SDK](https://www.blackmagicdesign.com/support/family/atem-live-production-switchers)
 - C++ toolchain with CMake 3.20+
 - Access to `BMDSwitcherAPI.h` from the ATEM SDK include folder

## Library Versions

 - `Newtonsoft.Json` 13.0.4
 - `SixLabors.ImageSharp` 3.1.12

## Building

Build the native bridge (`native/atem_bridge`) first:

```
cmake -S native/atem_bridge -B native/atem_bridge/build -DBMDSWITCHER_SDK_INCLUDE_DIR="/path/to/ATEM SDK include"
cmake --build native/atem_bridge/build --config Release
```

For your local SDK install, the include path is:

```
/Users/jnt/Downloads/Blackmagic_ATEM_Switchers_SDK_10.2.1/Blackmagic ATEM Switchers SDK 10.2.1/Mac OS X/include
```

You can also use the preconfigured helper script:

```
./scripts/build-bridge-macos.sh
```

Then build all managed projects:

```
dotnet build "ATEM Switcher Library.sln"
```

Make sure `atem_bridge` is on your native library search path when running:

 - macOS: `DYLD_LIBRARY_PATH` includes `native/atem_bridge/build`
 - Linux: `LD_LIBRARY_PATH` includes `native/atem_bridge/build`
 - Windows: `PATH` includes the bridge DLL directory

## Platform Support

Current codebase support:

 - macOS: Supported through native bridge (`atem_bridge`)
 - Windows: In progress (requires validating bridge build/runtime loading on Windows)
 - Linux: In progress (ATEM 10.2.1 SDK package does not include Linux headers/samples)

## Supported Image Formats

ImageSharp is used for image manipulation. This currently supports:

  - PNG
  - BMP
  - JPEG
  - GIF
  - TIFF

Alpha channels are supported and will be included in the images sent to the switcher.

Images will need to be the same resolution as the switcher. Running in debug mode you can see the detected resolution on the switcher.

## Notes

This has been tested with a Blackmagic Design ATEM Production Studio 4K. I do not have access to any other switchers to test with, but if they use version 6.2 or greater of the SDK, then they should work.


## Contact Details

If you're using this for anything interesting, I'd love to hear about it.

 - Web: http://www.mintopia.net
 - Email: jess@mintopia.net
 - Twitter: @MintopiaUK

 - Bitcoin: 1FhMKKabMSJx4M4Trm73JTTrALg7DmxbbP
 - Ethereum: 0x8063501c3944846579fb62aaAe3965d933638f35

## ChangeLog

### Version 2.0.2 - 2018-02-02:
 - Add support for NTSC SD

### Version 2.0.1 - 2018-02-01:
 - Built against Blackmagic Switcher SDK 7.3

### Version 2.0.0 - 2014-12-24:
 - Rebuilt from decompiled source of original binary
 - Added enumerating of the media pool

### Version 1.0.1 - 2014-09-22:
 - Moved switcher functions into a separate library to allow development of more tools
 - Slight change to arguments
 - Add support for specifying the name of the image when uploading it

### Version 1.0.0 - 2014-09-21:
 - Initial version

## MIT License

Copyright (C) 2016 by Jessica Smith

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
