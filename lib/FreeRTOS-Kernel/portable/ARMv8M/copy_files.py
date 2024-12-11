#/*
# * FreeRTOS Kernel <DEVELOPMENT BRANCH>
# * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# *
# * SPDX-License-Identifier: MIT
# *
# * Permission is hereby granted, free of charge, to any person obtaining a copy of
# * this software and associated documentation files (the "Software"), to deal in
# * the Software without restriction, including without limitation the rights to
# * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# * the Software, and to permit persons to whom the Software is furnished to do so,
# * subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included in all
# * copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# *
# * https://www.FreeRTOS.org
# * https://github.com/FreeRTOS
# *
# */

import os
import shutil

_THIS_FILE_DIRECTORY_ = os.path.dirname(os.path.realpath(__file__))
_FREERTOS_PORTABLE_DIRECTORY_ = os.path.dirname(_THIS_FILE_DIRECTORY_)

_COMPILERS_ = ['GCC', 'IAR']
_ARCH_NS_ = ['ARM_CM85', 'ARM_CM85_NTZ', 'ARM_CM55', 'ARM_CM55_NTZ', 'ARM_CM35P', 'ARM_CM35P_NTZ', 'ARM_CM33', 'ARM_CM33_NTZ', 'ARM_CM23', 'ARM_CM23_NTZ']
_ARCH_S_ = ['ARM_CM85', 'ARM_CM55', 'ARM_CM35P', 'ARM_CM33', 'ARM_CM23']

# Files to be compiled in the Secure Project
_SECURE_COMMON_FILE_PATHS_ = [
    os.path.join('secure', 'context'),
    os.path.join('secure', 'heap'),
    os.path.join('secure', 'init'),
    os.path.join('secure', 'macros')
]

_SECURE_PORTABLE_FILE_PATHS_ = {
    'GCC':{
        'ARM_CM23' :[os.path.join('secure', 'context', 'portable', 'GCC', 'ARM_CM23')],
        'ARM_CM33' :[os.path.join('secure', 'context', 'portable', 'GCC', 'ARM_CM33')],
        'ARM_CM35P':[os.path.join('secure', 'context', 'portable', 'GCC', 'ARM_CM33')],
        'ARM_CM55' :[os.path.join('secure', 'context', 'portable', 'GCC', 'ARM_CM33')],
        'ARM_CM85' :[os.path.join('secure', 'context', 'portable', 'GCC', 'ARM_CM33')]
    },
    'IAR':{
        'ARM_CM23' :[os.path.join('secure', 'context', 'portable', 'IAR', 'ARM_CM23')],
        'ARM_CM33' :[os.path.join('secure', 'context', 'portable', 'IAR', 'ARM_CM33')],
        'ARM_CM35P':[os.path.join('secure', 'context', 'portable', 'IAR', 'ARM_CM33')],
        'ARM_CM55' :[os.path.join('secure', 'context', 'portable', 'IAR', 'ARM_CM33')],
        'ARM_CM85' :[os.path.join('secure', 'context', 'portable', 'IAR', 'ARM_CM33')]
    }
}

# Files to be compiled in the Non-Secure Project
_NONSECURE_COMMON_FILE_PATHS_ = [
    'non_secure'
]

_NONSECURE_PORTABLE_FILE_PATHS_ = {
    'GCC':{
        'ARM_CM23'      : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM23')],
        'ARM_CM23_NTZ'  : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM23_NTZ')],
        'ARM_CM33'      : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33')],
        'ARM_CM33_NTZ'  : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ')],
        'ARM_CM35P'     : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM35P', 'portmacro.h')],
        'ARM_CM35P_NTZ' : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM35P', 'portmacro.h')],
        'ARM_CM55'      : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM55', 'portmacro.h')],
        'ARM_CM55_NTZ'  : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM55', 'portmacro.h')],
        'ARM_CM85'      : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM85', 'portmacro.h')],
        'ARM_CM85_NTZ'  : [os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'portasm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.c'),
                           os.path.join('non_secure', 'portable', 'GCC', 'ARM_CM85', 'portmacro.h')]
    },
    'IAR':{
        'ARM_CM23'      : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM23')],
        'ARM_CM23_NTZ'  : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM23_NTZ')],
        'ARM_CM33'      : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33')],
        'ARM_CM33_NTZ'  : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ')],
        'ARM_CM35P'     : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM35P', 'portmacro.h')],
        'ARM_CM35P_NTZ' : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM35P', 'portmacro.h')],
        'ARM_CM55'      : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM55', 'portmacro.h')],
        'ARM_CM55_NTZ'  : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM55', 'portmacro.h')],
        'ARM_CM85'      : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM85', 'portmacro.h')],
        'ARM_CM85_NTZ'  : [os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'portasm.s'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM33_NTZ', 'mpu_wrappers_v2_asm.S'),
                           os.path.join('non_secure', 'portable', 'IAR', 'ARM_CM85', 'portmacro.h')]
    },
}


def copy_files_in_dir(src_abs_path, dst_abs_path):
    if os.path.isfile(src_abs_path):
        print('Src: {}'.format(src_abs_path))
        print('Dst: {}\n'.format(dst_abs_path))
        shutil.copy2(src_abs_path, dst_abs_path)
    else:
        for src_file in os.listdir(src_abs_path):
            src_file_abs_path = os.path.join(src_abs_path, src_file)
            if os.path.isfile(src_file_abs_path) and src_file != 'ReadMe.txt':
                if not os.path.exists(dst_abs_path):
                    os.makedirs(dst_abs_path)
                print('Src: {}'.format(src_file_abs_path))
                print('Dst: {}\n'.format(dst_abs_path))
                shutil.copy2(src_file_abs_path, dst_abs_path)


def copy_common_files_for_compiler_and_arch(compiler, arch, src_paths, dst_path):
    for src_path in src_paths:

        src_abs_path = os.path.join(_THIS_FILE_DIRECTORY_, src_path)
        dst_abs_path = os.path.join(_FREERTOS_PORTABLE_DIRECTORY_, compiler, arch, dst_path)

        copy_files_in_dir(src_abs_path, dst_abs_path)


def copy_portable_files_for_compiler_and_arch(compiler, arch, src_paths, dst_path):
    for src_path in src_paths[compiler][arch]:

        src_abs_path = os.path.join(_THIS_FILE_DIRECTORY_, src_path)
        dst_abs_path = os.path.join(_FREERTOS_PORTABLE_DIRECTORY_, compiler, arch, dst_path)

        copy_files_in_dir(src_abs_path, dst_abs_path)


def copy_files():
    # Copy Secure Files
    for compiler in _COMPILERS_:
        for arch in _ARCH_S_:
            copy_common_files_for_compiler_and_arch(compiler, arch, _SECURE_COMMON_FILE_PATHS_, 'secure')
            copy_portable_files_for_compiler_and_arch(compiler, arch, _SECURE_PORTABLE_FILE_PATHS_, 'secure')

    # Copy Non-Secure Files
    for compiler in _COMPILERS_:
        for arch in _ARCH_NS_:
            copy_common_files_for_compiler_and_arch(compiler, arch, _NONSECURE_COMMON_FILE_PATHS_, 'non_secure')
            copy_portable_files_for_compiler_and_arch(compiler, arch, _NONSECURE_PORTABLE_FILE_PATHS_, 'non_secure')


def main():
    copy_files()


if __name__ == '__main__':
    main()
