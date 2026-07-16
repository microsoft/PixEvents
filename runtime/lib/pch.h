// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

// Optimize Windows headers with exclusion macros
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define NODRAWTEXT
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <windows.h>

#include <memory>
#include <vector>
#include <atomic>

#include <assert.h>

#include <d3d12.h>
#include <d3d12video.h> // must be before pix3.h

#include "IncludePixEtw.h"

#include <pix3.h>
