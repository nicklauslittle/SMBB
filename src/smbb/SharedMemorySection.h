
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

#ifndef SMBB_SHAREDMEMORYSECTION_H
#define SMBB_SHAREDMEMORYSECTION_H

#ifdef _WIN32
#include <sys/types.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#ifdef _SC_SHARED_MEMORY_OBJECTS
#include <sys/mman.h>
#endif
#endif

#include "utilities/IntegerTypes.h"

#include "SharedMemory.h"

namespace smbb {

class SharedMemorySection {
public:
	// Gets the value that all offsets must be a multiple of
	static unsigned long GetOffsetSize() {
		static unsigned long offsetSize = 0;

#ifdef _WIN32
		if (offsetSize == 0) {
			SYSTEM_INFO sinfo;

			GetSystemInfo(&sinfo);
			offsetSize = sinfo.dwAllocationGranularity;
		}
#elif defined(_SC_PAGE_SIZE) || !defined(_SC_PAGESIZE)
		if (offsetSize == 0)
			offsetSize = static_cast<unsigned long>(sysconf(_SC_PAGE_SIZE));
#else
		if (offsetSize == 0)
			offsetSize = static_cast<unsigned long>(sysconf(_SC_PAGESIZE));
#endif

		return offsetSize;
	}

	// Gets the closest previous offset that can be used to map a memory section
	static SharedMemory::Size GetMapOffset(SharedMemory::Size offset) {
		static SharedMemory::Size mask = GetOffsetSize() - 1;
		return offset & ~mask;
	}

private:
	uint8_t *_data;
	size_t _size;
	SharedMemory::Size _offset;
	bool _readOnly;

	// Disable copying
	SharedMemorySection(const SharedMemorySection &) { }
	SharedMemorySection &operator=(const SharedMemorySection &) { return *this; }

public:
	// Maps a new section from shared memory (Note that the section is still valid even if the shared memory is closed)
	SharedMemorySection(const SharedMemory &sharedMemory, size_t size, SharedMemory::Size offset = 0) :
			_data(), _size(size), _offset(offset), _readOnly(sharedMemory._readOnly) {
		if (!_size)
			return;

#ifdef _WIN32
		_data = (uint8_t *)MapViewOfFile(sharedMemory._mapHandle, _readOnly ? FILE_MAP_READ : FILE_MAP_WRITE, (DWORD)(_offset >> 32), (DWORD)_offset, (SIZE_T)_size);
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
		_data = (uint8_t *)mmap(NULL, _size, PROT_READ | (_readOnly ? 0 : PROT_WRITE), MAP_SHARED, sharedMemory._handle, _offset);

		if (_data == MAP_FAILED)
			_data = NULL;
#endif
	}

	~SharedMemorySection() {
#ifdef _WIN32
		if (_data)
			(void)UnmapViewOfFile(_data);
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
		if (_data)
			(void)munmap(_data, _size);
#endif
	}

	// Returns true if the section is valid
	bool Valid() const { return _data != NULL; }

	// Gets the offset of the section into the shared memory
	SharedMemory::Size Offset() const { return _offset; }

	// Gets the data pointer of the mapped section
	uint8_t *Data() const { return _data; }

	// Gets the size of the mapped data
	size_t Size() const { return _size; }

	// Returns true if the mapped data is read only
	bool ReadOnly() const { return _readOnly; }
};

}

#endif
