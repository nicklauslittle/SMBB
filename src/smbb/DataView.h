
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

#ifndef SMBB_DATAVIEW_H
#define SMBB_DATAVIEW_H

#include <cassert>
#include <cstdlib>

#include "utilities/IntegerTypes.h"

#if defined(_MSC_VER)
#define SWAP_2(V) _byteswap_ushort(static_cast<unsigned short>(V))
#define SWAP_4(V) _byteswap_ulong(static_cast<unsigned long>(V))
#define SWAP_8(V) _byteswap_uint64(static_cast<unsigned long long>(V))
#elif defined(__GNUC__) || defined(__clang__)
#define SWAP_2(V) __builtin_bswap16(static_cast<uint16_t>(V))
#define SWAP_4(V) __builtin_bswap32(static_cast<uint32_t>(V))
#define SWAP_8(V) __builtin_bswap64(static_cast<uint64_t>(V))
#else
#define SWAP_2(V) static_cast<uint16_t>((static_cast<uint16_t>(V) << 8) | (static_cast<uint16_t>(V) >> 8))
#define SWAP_4(V) static_cast<uint32_t>((static_cast<uint32_t>(V) << 24) | ((static_cast<uint32_t>(V) & 0xFF00U) << 8) | ((static_cast<uint32_t>(V) & 0xFF0000U) >> 8) | (static_cast<uint32_t>(V) >> 24))
#define SWAP_8(V) static_cast<uint64_t>((static_cast<uint64_t>(V) << 56) | ((static_cast<uint64_t>(V) & 0xFF00U) << 40) | ((static_cast<uint64_t>(V) & 0xFF0000U) << 24) | ((static_cast<uint64_t>(V) & 0xFF000000U) << 8) | ((static_cast<uint64_t>(V) >> 8) & 0xFF000000U) | ((static_cast<uint64_t>(V) >> 24) & 0xFF0000U) | ((static_cast<uint64_t>(V) >> 40) & 0xFF00U) | (static_cast<uint64_t>(V) >> 56))
#endif

namespace smbb {

static const unsigned int LITTLE_ENDIANNESS = 1;
static const float FLOAT_LITTLE_ENDIANNESS = 1.0f;

struct DataView {
	typedef uint8_t Byte;

	// Endianness checks
	static bool IsLittleEndian() { return *reinterpret_cast<const unsigned char *>(&LITTLE_ENDIANNESS) != 0; }
	static bool IsBigEndian() { return *reinterpret_cast<const unsigned char *>(&LITTLE_ENDIANNESS) == 0; }
	static bool IsFloatingLittleEndian() { return *reinterpret_cast<const unsigned char *>(&FLOAT_LITTLE_ENDIANNESS) == 0; }
	static bool IsFloatingBigEndian() { return *reinterpret_cast<const unsigned char *>(&FLOAT_LITTLE_ENDIANNESS) != 0; }

	// Load and store functions
	template <size_t S> struct LoadStoreStruct { };

	// Gets a value of the specified type out of the specified buffer
	template <typename T> static T Get(const Byte buffer[]) { return LoadStoreStruct<sizeof(T)>::template Load<T>(buffer); }
	template <size_t BIT> static bool GetBool(const Byte buffer[]) { return (buffer[0] & (1 << BIT)) != 0; }

	// Sets a value of the specified type in the buffer
	template <typename T> static void Set(Byte buffer[], T value) { LoadStoreStruct<sizeof(T)>::Store(buffer, value); }
	template <size_t BIT> static void SetBool(Byte buffer[], bool value) { buffer[0] = Byte((buffer[0] & Byte(~(1 << BIT))) | Byte(value << BIT)); }
	template <size_t BIT> static void SetTrue(Byte buffer[]) { buffer[0] |= Byte(1 << BIT); }
	template <size_t BIT> static void SetFalse(Byte buffer[]) { buffer[0] &= Byte(~(1 << BIT)); }
};

// Load and store functions
template <> struct DataView::LoadStoreStruct<1> {
	template <typename T> static T Load(const Byte buffer[]) { return static_cast<T>(buffer[0]); }
	template <typename T> static void Store(Byte buffer[], T value) { return buffer[0] = static_cast<Byte>(value); }
};

template <> struct DataView::LoadStoreStruct<2> {
	template <typename T> static T Load(const Byte buffer[]) { return IsLittleEndian() ? *reinterpret_cast<const T *>(buffer) : static_cast<T>(SWAP_2(*reinterpret_cast<const T *>(buffer))); }
	template <typename T> static void Store(Byte buffer[], T value) { *reinterpret_cast<T *>(buffer) = IsLittleEndian() ? value : static_cast<T>(SWAP_2(value)); }
};

template <> struct DataView::LoadStoreStruct<4> {
	template <typename T> static T Load(const Byte buffer[]) { return IsLittleEndian() ? *reinterpret_cast<const T *>(buffer) : static_cast<T>(SWAP_4(*reinterpret_cast<const T *>(buffer))); }
	template <typename T> static void Store(Byte buffer[], T value) { *reinterpret_cast<T *>(buffer) = IsLittleEndian() ? value : static_cast<T>(SWAP_4(value)); }
};

template <> struct DataView::LoadStoreStruct<8> {
	template <typename T> static T Load(const Byte buffer[]) { return IsLittleEndian() ? *reinterpret_cast<const T *>(buffer) : static_cast<T>(SWAP_8(*reinterpret_cast<const T *>(buffer))); }
	template <typename T> static void Store(Byte buffer[], T value) { *reinterpret_cast<T *>(buffer) = IsLittleEndian() ? value : static_cast<T>(SWAP_8(value)); }
};

// Get and set functions
template <> float  DataView::Get(const Byte buffer[]) { uint32_t rawValue = IsFloatingLittleEndian() ? *reinterpret_cast<const uint32_t *>(buffer) : static_cast<uint32_t>(SWAP_4(*reinterpret_cast<const uint32_t *>(buffer))); return *reinterpret_cast<float *>(&rawValue); }
template <> double DataView::Get(const Byte buffer[]) { uint64_t rawValue = IsFloatingLittleEndian() ? *reinterpret_cast<const uint64_t *>(buffer) : static_cast<uint64_t>(SWAP_8(*reinterpret_cast<const uint64_t *>(buffer))); return *reinterpret_cast<double *>(&rawValue); }

template <> void DataView::Set(Byte buffer[], float value)  { uint32_t rawValue = *reinterpret_cast<uint32_t *>(&value); *reinterpret_cast<uint32_t *>(buffer) = IsFloatingLittleEndian() ? rawValue : static_cast<uint32_t>(SWAP_4(rawValue)); }
template <> void DataView::Set(Byte buffer[], double value) { uint64_t rawValue = *reinterpret_cast<uint64_t *>(&value); *reinterpret_cast<uint64_t *>(buffer) = IsFloatingLittleEndian() ? rawValue : static_cast<uint64_t>(SWAP_8(rawValue)); }

}

#undef SWAP_2
#undef SWAP_4
#undef SWAP_8

#endif
