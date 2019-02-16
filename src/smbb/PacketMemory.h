
/**
Copyright (c) 2019 Nick Little

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
SOFTWARE.
*/

#ifndef SMBB_PACKETMEMORY_H
#define SMBB_PACKETMEMORY_H

#include "utilities/Atomic.h"
#include "utilities/IntegerTypes.h"

#include "Time.h"

namespace smbb {

class PacketMemory {
public:
	// Version Information
	typedef uint16_t Version;

	static const Version CURRENT_MAJOR_VERSION = 0x0000; // Increase if a new change is made that breaks backwards compatibility with old versions
	static const Version CURRENT_MINOR_VERSION = 0x0000; // Increase if new backwards compatible feature is added

private:
	Version _majorVersion;
	Version _minorVersion;

	bool HasMinimumVersion(Version majorVersion, Version minorVersion) {
		return _majorVersion > majorVersion || (_majorVersion == majorVersion && _minorVersion >= minorVersion);
	}

	size_t _blockSize; // The size of a block (data block size + HEADER_SIZE)
	DataSize _dataOffset; // The offset to the data from the start of the object
	DataSize _dataSize; // The size of the data

	volatile uint32_t _updateIndex; // The update index (incremented every minor update)
	volatile Time _updateTime; // The time of the last major update

public:
	PacketMemory(DataSize dataOffset, DataSize dataSize) : _majorVersion(CURRENT_MAJOR_VERSION), _minorVersion(CURRENT_MINOR_VERSION),
		_dataOffset(dataOffset), _dataSize(dataSize), _updateIndex(), _updateTime()
	{ }

	// A gap in the packet memory
	class Gap {
		Time _nakTime; // The time the last NAK was sent
		DataSize _start; // The start of the gap data (in words)
		DataSize _size; // The size of the gap data (in words)

	public:
		// Constructs a gap structure at the specified data offset
		Gap() : _nakTime(), _start(), _size() { }
	};
};

}

#endif
