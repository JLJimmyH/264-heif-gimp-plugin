# 264-heif-gimp-plugin

This is a GIMP plugin for loading H.264 bitstream HEIF images (High Efficiency Image File Format).

## Features

Loader:
* Supports still image mode with HEIF files using the H.264 bitstream.

## Installation

This code depends on [heif](https://github.com/nokiatech/heif)
and [openh264](https://github.com/cisco/openh264).  
Please make sure to install these libraries and place them in the 'lib' folder before compiling this plugin.
And you can modify the include and library paths in the `CMakeLists.txt` file according to your specific setup.

## Building source:
Prerequisites: **[cmake](https://cmake.org/)** and compiler supporting C++11 in PATH.
```
mkdir build
cd build
cmake ..
cmake --build .
```

After compilation, copy the file `heif-264-gimp-plugin` into your GIMP plugin directory
(for example, `$HOME/.gimp-2.8/plug-ins`).
When starting GIMP, the HEIF file-format should now show in the list of supported formats when you open a file.

## License

The 264-heif-gimp-plugin is distributed under the terms of the GNU General Public License.

See COPYING for more details.