
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

#ifndef SMBB_SHAREDMEMORY_H
#define SMBB_SHAREDMEMORY_H

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifndef MAX_SHARED_MEMORY_FILENAME_SIZE
#define MAX_SHARED_MEMORY_FILENAME_SIZE 1024U
#endif

namespace smbb {

class SharedMemorySection;

class SharedMemory {
	friend class SharedMemorySection;

public:
#ifdef _WIN32
	typedef LONGLONG Size;
#else
	typedef off_t Size;
#endif

	enum LoadResult {
		LOAD_SUCCESS = 0,
		LOAD_FAILED_UNSUPPORTED,
		LOAD_FAILED_BAD_SIZE,
		LOAD_FAILED_BAD_NAME,
		LOAD_FAILED_TO_OPEN_FILE,
		LOAD_FAILED_TO_RESIZE_FILE
	};

private:
#ifdef _WIN32
	typedef HANDLE Handle;

	Handle _handle;
	Handle _mapHandle;
	bool _readOnly;
#else
	bool _readOnly;
	bool _usingFile;
	int _handle;
	char _name[MAX_SHARED_MEMORY_FILENAME_SIZE];
#endif

	// Disable copying
	SharedMemory(const SharedMemory &) { }
	SharedMemory &operator=(const SharedMemory &) { return *this; }

	// Loads shared memory by name or by filename
	LoadResult Load(const char *name, const char *filename, bool readOnly, Size size = 0, bool deleteOnClose = false);

public:
	// Gets the recommended directory for putting temporary, shared memory files
	static bool GetRecommendedDirectory(char *directory, size_t directorySize);

	// Deletes a shared memory file
	static bool DeleteFileBacked(const char *filename);

	// Deletes a named shared memory entity
	static bool DeleteNamed(const char *);

#ifdef _WIN32
	SharedMemory() : _handle(INVALID_HANDLE_VALUE), _mapHandle(), _readOnly() { }
#else
	SharedMemory() : _readOnly(), _usingFile(), _handle(-1) { _name[0] = (char)0; }
#endif

	~SharedMemory() { Close(); }

	// Creates a new shared memory file (useful for large files)
	LoadResult CreateFileBacked(const char *filename, Size size, bool deleteOnClose = false) {
		return Load(NULL, filename, false, size, deleteOnClose);
	}

	// Opens an existing shared memory file
	LoadResult OpenFileBacked(const char *filename, bool readOnly = true) {
		return Load(NULL, filename, readOnly);
	}

	// Creates a new named shared memory entity (useful for small files)
	LoadResult CreateNamed(const char *name, Size size, bool deleteOnClose = false) {
		return Load(name, NULL, false, size, deleteOnClose);
	}

	// Opens an existing named shared memory entity
	LoadResult OpenNamed(const char *name, bool readOnly = true) {
		return Load(name, NULL, readOnly);
	}

	// Closes the shared memory, so that it can no longer be mapped (existing mappings will continue to operate properly)
	void Close();
};

}

#endif
