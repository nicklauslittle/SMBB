
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
#include <sys/types.h>
#include <windows.h>
#else
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#ifdef _SC_SHARED_MEMORY_OBJECTS
#include <sys/mman.h>
#endif
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
#elif defined(_SC_SHARED_MEMORY_OBJECTS)
	static bool CopyFilename(char *newFilename, const char *filename) {
		size_t len = strlen(filename);

		if (len >= MAX_SHARED_MEMORY_FILENAME_SIZE)
			return false;

		memcpy(newFilename, filename, len);
		newFilename[len] = (char)0;
		return true;
	}

	static bool CopyName(char *newName, const char *name) {
		size_t len = strlen(name);

		if (len >= MAX_SHARED_MEMORY_FILENAME_SIZE - 1)
			return false;

		newName[0] = '/';
		memcpy(&newName[1], name, len);
		newName[len + 1] = (char)0;
		return true;
	}

	char _name[MAX_SHARED_MEMORY_FILENAME_SIZE];
	bool _usingFile;
	int _handle;
#endif
	bool _readOnly;

	// Disable copying
	SharedMemory(const SharedMemory &) { }
	SharedMemory &operator=(const SharedMemory &) { return *this; }

	// Loads shared memory by name or by filename
	LoadResult Load(const char *name, const char *filename, bool readOnly, Size size = 0, bool deleteOnClose = false) {
		Close();

		if (size < 0)
			return LOAD_FAILED_BAD_SIZE;

#ifdef _WIN32
		if (filename) { // Use file-backed memory
			_handle = CreateFile(filename, GENERIC_READ | (readOnly ? 0 : GENERIC_WRITE), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, size ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | (deleteOnClose ? FILE_FLAG_DELETE_ON_CLOSE : 0), NULL);

			if (_handle == INVALID_HANDLE_VALUE)
				return LOAD_FAILED_TO_OPEN_FILE;

			if (size) {
				LARGE_INTEGER newSize;
				newSize.QuadPart = size;

				if (!SetFilePointerEx(_handle, newSize, NULL, FILE_BEGIN) || !SetEndOfFile(_handle)) {
					Close();
					return LOAD_FAILED_TO_RESIZE_FILE;
				}
			}

			_mapHandle = CreateFileMapping(_handle, NULL, readOnly ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);
		}
		else if (!name)
			return LOAD_FAILED_BAD_NAME;
		else if (size) // Don't use file-backed memory (size is set atomically, and memory is zeroized)
			_mapHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, (readOnly ? PAGE_READONLY : PAGE_READWRITE) | SEC_COMMIT, (DWORD)(size >> 32), (DWORD)size, name);
		else
			_mapHandle = OpenFileMapping(FILE_MAP_READ | (readOnly ? 0 : FILE_MAP_WRITE), FALSE, name);

		_readOnly = readOnly;
		return LOAD_SUCCESS;
#elif defined(_SC_SHARED_MEMORY_OBJECTS)
		if (filename) { // Use file-backed memory
			if (!CopyFilename(_name, filename))
				return LOAD_FAILED_BAD_NAME;

			_usingFile = true;
			_handle = open(_name, (readOnly ? O_RDONLY : O_RDWR) | (size ? O_CREAT | O_EXCL : 0), 0600);
		}
		else if (!name)
			return LOAD_FAILED_BAD_NAME;
		else { // Don't use file-backed memory
			if (!CopyName(_name, name))
				return LOAD_FAILED_BAD_NAME;

			_usingFile = false;
			_handle = shm_open(_name, (readOnly ? O_RDONLY : O_RDWR) | (size ? O_CREAT | O_EXCL : 0), 0600);
		}

		if (!deleteOnClose)
			_name[0] = (char)0;

		// Check for error
		if (_handle == -1) {
			_name[0] = (char)0;
			Close();
			return LOAD_FAILED_TO_OPEN_FILE;
		}
		else if (size && ftruncate(_handle, size) == -1) { //< Resize (zeroizes memory)
			Close();
			return LOAD_FAILED_TO_RESIZE_FILE;
		}

		_readOnly = readOnly;
		return LOAD_SUCCESS;
#else
		return LOAD_FAILED_UNSUPPORTED;
#endif
	}

public:
#ifdef _WIN32
	static const bool IS_SUPPORTED = true;

	// Gets the recommended directory for putting temporary, shared memory files
	static const char *GetRecommendedDirectory() {
		static char directory[MAX_SHARED_MEMORY_FILENAME_SIZE] = { 0 };

		if (!directory[0]) {
			DWORD len = GetTempPath(MAX_SHARED_MEMORY_FILENAME_SIZE, directory);

			if (len == 0 || len > MAX_SHARED_MEMORY_FILENAME_SIZE)
				return "C:\\";
		}

		return directory;
	}

	// Deletes a shared memory file
	static bool DeleteFileBacked(const char *filename) {
		return DeleteFile(filename) == TRUE;
	}

	// Deletes a named shared memory entity
	static bool DeleteNamed(const char *) {
		return true;
	}

	SharedMemory() : _handle(INVALID_HANDLE_VALUE), _mapHandle(), _readOnly() { }
#elif defined(_SC_SHARED_MEMORY_OBJECTS)
	static const bool IS_SUPPORTED = true;

	// Gets the recommended directory for putting temporary, shared memory files
	static const char *GetRecommendedDirectory() {
		const char *directory = getenv("TMPDIR");
		return directory ? directory : "/tmp";
	}

	// Deletes a shared memory file
	static bool DeleteFileBacked(const char *filename) {
		return unlink(filename) == 0;
	}

	// Deletes a named shared memory entity
	static bool DeleteNamed(const char *name) {
		char realName[MAX_SHARED_MEMORY_FILENAME_SIZE];
		return name != NULL && CopyName(realName, name) && shm_unlink(realName) == 0;
	}

	SharedMemory() : _usingFile(), _handle(-1), _readOnly() { _name[0] = (char)0; }
#else
	static const bool IS_SUPPORTED = false;

	SharedMemory() : _readOnly() { }
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
	void Close() {
#ifdef _WIN32
		if (_mapHandle) {
			(void)CloseHandle(_mapHandle);
			_mapHandle = NULL;
		}

		if (_handle != INVALID_HANDLE_VALUE) {
			(void)CloseHandle(_handle);
			_handle = INVALID_HANDLE_VALUE;
		}
#elif defined(_SC_SHARED_MEMORY_OBJECTS)
		if (_name[0]) {
			if (_usingFile)
				(void)unlink(_name);
			else
				(void)shm_unlink(_name);

			_name[0] = (char)0;
		}

		if (_handle != -1) {
			(void)close(_handle);
			_handle = -1;
		}
#endif
	}
};

}

#endif
