// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PTRS_H
#define PTRS_H
#include "types.h"
#include "utils/utils.h"
#include <memory>

using sharedPktQueue = std::shared_ptr<AVPktQueue>;
using weakPktQueue = std::weak_ptr<AVPktQueue>;
using sharedFrmQueue = std::shared_ptr<SPSCQueue<AVFrmItem>>;
using weakFrmQueue = std::weak_ptr<SPSCQueue<AVFrmItem>>;

#endif // PTRS_H
