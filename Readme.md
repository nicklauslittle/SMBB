## Note: This is a pre-release version, use at your own risk

# Synchronized Memory Building Blocks (SMBB)
SMBB is a small set of cross-platform building blocks for easily constructing scalable and reliable shared data applications using IP sockets and shared memory. It is released under the MIT license and can be used in both open source and closed source applications.

## Design Goals
* Minimalistic
 * Allow header-only compilation
 * Allow compilation of individual object files
 * Allow linking with static library
* Easy to use
 * Simple C++ classes that wrap C structs / handles
* Support for safety-critical systems
 * No dynamic memory allocations
 * No C++ exceptions
* Wide platform compatibility
 * Support for Win32 API and POSIX-like APIs (LSB, SUS, etc.)
 * Support for C++98 and newer compilers
 * Identical functionality on different platforms
