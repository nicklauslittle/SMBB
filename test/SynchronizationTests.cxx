
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

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <iostream>
#include <string>

#include "smbb/IPSocket.h"
#include "smbb/PacketMemory.h"
#include "smbb/SharedMemory.h"
#include "smbb/SharedMemorySection.h"

using namespace smbb;

SCENARIO ("Unordered", "[PacketMemory], [SharedMemory]") {
	IPSocket::Initialize();

	IPSocket recvSock(IPAddress("localhost", NULL, true, IPV4), UDP, IPSocket::OPEN_AND_BIND);
	IPSocket sendSock(recvSock.GetAddress(), UDP, IPSocket::OPEN_AND_CONNECT);

	const char *testData[] = { "This is a ", "test.", "misordered data " };
	IPSocket::Buffer buffers[] = { IPSocket::Buffer::Make(testData[0], strlen(testData[0])), IPSocket::Buffer::Make(testData[2], strlen(testData[2])), IPSocket::Buffer::Make(testData[1], strlen(testData[1]) + 1) };
	IPSocket::Message message = IPSocket::Message::Make(buffers, sizeof(buffers) / sizeof(buffers[0]));
	REQUIRE(sendSock.Send(message).GetResult() > 0);

	IPSocket::PollItem pollItem = IPSocket::PollItem::Make(recvSock, IPSocket::POLL_CAN_READ);
	IPSocket::Poll(&pollItem, 1, 1000);
	REQUIRE(pollItem.HasResult(IPSocket::POLL_CAN_READ));

	char buf[1024];
	REQUIRE(recvSock.Receive(&buf, sizeof(buf)).GetResult() > 0);
	std::cout << buf << std::endl;

	/*
	GIVEN ("Some file-backed shared memory") {
		static const size_t packetSize = 2048;
		static const size_t blockCount = 64;
		SharedMemory testFile;

		REQUIRE(testFile.CreateFileBacked("Memory", SharedMemorySection::GetOffsetSize() + packetSize * blockCount, true) == SharedMemory::LOAD_SUCCESS);

		SharedMemorySection metadataSection(testFile, SharedMemorySection::GetOffsetSize());
		PacketMemory *pMem = new(metadataSection.Data()) PacketMemory(SharedMemorySection::GetOffsetSize(), packetSize * blockCount);

		WHEN ("The shared memory is opened and the data modified") {
			SharedMemory testFile2;

			REQUIRE(testFile2.OpenFileBacked("Test 1") == SharedMemory::LOAD_SUCCESS);

			THEN ("The memory in the created shared memory section matches the memory in the opened shared memory section") {
				SharedMemorySection section2(testFile2, 4096, SharedMemorySection::GetOffsetSize());
				SharedMemorySection section21(testFile2, 4096);

				REQUIRE(section2.ReadOnly());
				REQUIRE(section21.ReadOnly());

				REQUIRE(section21.Size() == 4096);
				REQUIRE(section21.Valid());

				strcpy((char *)section1.Data(), "Test String 1");
				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 1");

				REQUIRE(std::string((const char *)section2.Data()) == "Test String 1");

				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 2");

				REQUIRE(std::string((const char *)section21.Data()) == "Test String 1");
				REQUIRE(std::string((const char *)section2.Data()) == "Test String 2");
			}
		}

		WHEN ("A shared memory section is created with no size") {
			SharedMemorySection section2(testFile, 0);

			THEN ("The shared memory section is invalid") {
				REQUIRE(section2.Size() == 0);
				REQUIRE(!section2.Valid());
			}
		}
	} */

	IPSocket::Finish();
}
