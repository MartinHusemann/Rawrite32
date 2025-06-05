# Rawrite32
## The NetBSD image writing tool
(not restricted to NetBSD images)

## What is this?

Rawrite32 is a Windows tool to prepare disks or other removeable media,
especially USB memory sticks, from files called file system images.

## Documentation

For all details please check the homepage:
https://NetBSD.org/~martin/rawrite32/

## Building
To make this source compile you need

 - MS Visual Studio 2017 or newer (2022 used for latest release binaries)
 - some additional files (see below)

Get the complete zlib distribution http://www.zlib.net/ and unpack it in the zlib subdirectory (1.3.1 used for latest release binaries).

Get the complete bz2lib distritibution https://gitlab.com/bzip2/bzip2/ and unpack it in the bz2lib subdirectory (1.0.8 used for latest release binaries).

Get the complete xz distribution http://tukaani.org/xz/ and unpack it in the xz directoy (5.8.1 used for latest release binaries).

Be carefull to not overwrite the vxproj files provided with the Rawrite32 distribution.

To create the installer, you need the nullsoft install system.
