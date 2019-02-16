
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

#ifndef SMBB_DATABLOCKS_H
#define SMBB_DATABLOCKS_H

#include "utilities/IntegerTypes.h"

namespace smbb {

// Block Reference
class BlockReference {
	uint32_t _index; // The 1-based index of the block (0 indicates invalid)

public:
	BlockReference(uint32_t index = 0) : _index(index) { }

	bool IsValid() const { return _index != 0; }

	friend class BlockArraySection;
};

// Block Header
struct BlockHeader {
	BlockReference _nextReceived;
	BlockReference _nextInStream;

	BlockHeader(BlockReference nextReceived = 0, BlockReference nextInStream = 0) : _nextReceived(nextReceived), _nextInStream(nextInStream) { }

	BlockReference GetNextReceived() const { return _nextReceived; }
	BlockReference GetNextInStream() const { return _nextInStream; }
	void *GetData() const;

	BlockHeader &SetNextReceived(BlockReference nextReceived) { _nextReceived = nextReceived; return *this; }
	BlockHeader &SetNextInStream(BlockReference nextInStream) { _nextInStream = nextInStream; return *this; }
};

static const size_t HEADER_SIZE = (sizeof(uintmax_t) + sizeof(BlockHeader) - 1) / sizeof(uintmax_t) * sizeof(uintmax_t);

inline void *BlockHeader::GetData() const { return const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(this)) + HEADER_SIZE; }

// Represents an array of memory blocks
class BlockArray {
	size_t _blockSize; ///< The size of a block (data block size + HEADER_SIZE)
	uint32_t _count; ///< The number of blocks in the array

public:
	// Gets the data size from the given block size
	static size_t GetDataSizeFromBlockSize(size_t blockSize) { return blockSize - HEADER_SIZE; }

	// Gets the block size from the given data size
	static size_t GetBlockSizeFromDataSize(size_t dataSize) { return dataSize + HEADER_SIZE; }

	BlockArray(size_t dataBlockSize, void *memory, DataSize totalSize) :
		_blockSize(GetBlockSizeFromDataSize(dataBlockSize)), _count(static_cast<uint32_t>(totalSize / _blockSize)) { }

	// Gets the data size for a single block
	size_t GetBlockSize() const { return _blockSize; }

	// Gets the number of blocks
	uint32_t GetCount() const { return _count; }
};

// Represents a section of an array of memory blocks
class BlockArraySection {
	size_t _blockSize; ///< The size of a block (data block size + HEADER_SIZE)
	uint8_t *_baseMemory; ///< The base memory for the array (original memory - the size of a block)
	uint32_t _start; ///< The starting index for the array section
	uint32_t _end; ///< The ending index for the array section

public:
	BlockArraySection() : _baseMemory(), _start(), _end() { }

	BlockArraySection(const BlockArray &blocks, void *memory, uint32_t start, uint32_t end) : _blockSize(blocks.GetBlockSize()),
		_baseMemory(reinterpret_cast<uint8_t *>(memory) - _blockSize * start), _start(start), _end(end > blocks.GetCount() ? blocks.GetCount() : end) { }

	// Checks if the array section contains the specified index
	bool Contains(BlockReference index) const { index._index > _start && index._index <= _end; }

	// Gets a block header from a contained index
	BlockHeader &GetHeaderFromIndex(BlockReference index) const {
		return *reinterpret_cast<BlockHeader *>(_baseMemory + index._index * _blockSize);
	}
};

}

#endif
