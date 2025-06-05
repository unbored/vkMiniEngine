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

#pragma once

//#include "pch.h"
#include <memory>
#include <string>
#include <vector>
//#include <ppl.h>

namespace Utility
{
using namespace std;
// using namespace concurrency;

typedef shared_ptr<vector<uint8_t>> ByteArray;
extern ByteArray NullFile;

// Reads the entire contents of a binary file.  If the file with the same name except with an additional
// ".gz" suffix exists, it will be loaded and decompressed instead.
// This operation blocks until the entire file is read.
ByteArray ReadFileSync(const string &fileName);

// Same as previous except that it does not block but instead returns a task.
// task<ByteArray> ReadFileAsync(const string& fileName);

} // namespace Utility
