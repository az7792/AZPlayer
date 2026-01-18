// SPDX-FileCopyrightText: 2025 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMPAT_H
#define COMPAT_H

#ifdef __cplusplus
#define AZ_EXTERN_C_BEGIN extern "C" {
#define AZ_EXTERN_C_END }
#else
#define AZ_EXTERN_C_BEGIN
#define AZ_EXTERN_C_END
#endif

#endif /* COMPAT_H */
