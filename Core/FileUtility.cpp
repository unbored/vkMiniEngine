//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

//#include "pch.h"
#include "FileUtility.h"
#include "Utility.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <zlib.h> // From NuGet package

using namespace std;
using namespace Utility;

namespace Utility
{
ByteArray NullFile = make_shared<vector<unsigned char>>(vector<unsigned char>());
}

ByteArray DecompressZippedFile(string &fileName);

ByteArray ReadFileHelper(const string &fileName)
{
    std::filesystem::path path(fileName);
    if (!std::filesystem::exists(path))
        // struct _stat64 fileStat;
        // int fileExists = _wstat64(fileName.c_str(), &fileStat);
        // if (fileExists == -1)
        return NullFile;

    ifstream file(fileName, ios::in | ios::binary);
    if (!file)
        return NullFile;

    auto file_size = std::filesystem::file_size(path);
    Utility::ByteArray byteArray = make_shared<vector<unsigned char>>(file_size);
    file.read((char *)byteArray->data(), byteArray->size());
    file.close();

    return byteArray;
}

ByteArray ReadFileHelperEx(shared_ptr<string> fileName)
{
    std::string zippedFileName = *fileName + ".gz";
    ByteArray firstTry = DecompressZippedFile(zippedFileName);
    if (firstTry != NullFile)
        return firstTry;

    return ReadFileHelper(*fileName);
}

ByteArray Inflate(ByteArray CompressedSource, int &err, uint32_t ChunkSize = 0x100000)
{
    // Create a dynamic buffer to hold compressed blocks
    vector<unique_ptr<unsigned char>> blocks;

    z_stream strm = {};
    strm.data_type = Z_BINARY;
    strm.total_in = strm.avail_in = (uInt)CompressedSource->size();
    strm.next_in = CompressedSource->data();

    err = inflateInit2(&strm, (15 + 32)); // 15 window bits, and the +32 tells zlib to to detect if using gzip or zlib

    while (err == Z_OK || err == Z_BUF_ERROR)
    {
        strm.avail_out = ChunkSize;
        strm.next_out = (unsigned char *)malloc(ChunkSize);
        blocks.emplace_back(strm.next_out);
        err = inflate(&strm, Z_NO_FLUSH);
    }

    if (err != Z_STREAM_END)
    {
        inflateEnd(&strm);
        return NullFile;
    }

    ASSERT(strm.total_out > 0, "Nothing to decompress");

    Utility::ByteArray byteArray = make_shared<vector<unsigned char>>(strm.total_out);

    // Allocate actual memory for this.
    // copy the bits into that RAM.
    // Free everything else up!!
    void *curDest = byteArray->data();
    size_t remaining = byteArray->size();

    for (size_t i = 0; i < blocks.size(); ++i)
    {
        ASSERT(remaining > 0);

        size_t CopySize = remaining < ChunkSize ? remaining : ChunkSize;

        memcpy(curDest, blocks[i].get(), CopySize);
        curDest = (unsigned char *)curDest + CopySize;
        remaining -= CopySize;
    }

    inflateEnd(&strm);

    return byteArray;
}

ByteArray DecompressZippedFile(string &fileName)
{
    ByteArray CompressedFile = ReadFileHelper(fileName);
    if (CompressedFile == NullFile)
        return NullFile;

    int error;
    ByteArray DecompressedFile = Inflate(CompressedFile, error);
    if (DecompressedFile->size() == 0)
    {
        Utility::Printf("Couldn't unzip file %s:  Error = %d\n", fileName.c_str(), error);
        return NullFile;
    }

    return DecompressedFile;
}

ByteArray Utility::ReadFileSync(const string &fileName) { return ReadFileHelperEx(make_shared<string>(fileName)); }

// task<ByteArray> Utility::ReadFileAsync(const string& fileName)
//{
//     shared_ptr<string> SharedPtr = make_shared<string>(fileName);
//     return create_task([=] { return ReadFileHelperEx(SharedPtr); });
// }
