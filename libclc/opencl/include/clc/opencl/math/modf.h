//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __CLC_OPENCL_MATH_MODF_H__
#define __CLC_OPENCL_MATH_MODF_H__

#define FUNCTION modf
#define __CLC_BODY <clc/math/unary_decl_with_ptr.inc>
#include <clc/math/gentype.inc>

#undef FUNCTION

#endif // __CLC_OPENCL_MATH_MODF_H__
