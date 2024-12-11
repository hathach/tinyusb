[![CMock Unit Tests](https://github.com/FreeRTOS/FreeRTOS-Kernel/actions/workflows/unit-tests.yml/badge.svg?branch=main&event=push)](https://github.com/FreeRTOS/FreeRTOS-Kernel/actions/workflows/unit-tests.yml?query=branch%3Amain+event%3Apush+workflow%3A%22CMock+Unit+Tests%22++)
[![codecov](https://codecov.io/gh/FreeRTOS/FreeRTOS-Kernel/badge.svg?branch=main)](https://codecov.io/gh/FreeRTOS/FreeRTOS-Kernel)

## Getting started

This repository contains FreeRTOS kernel source/header files and kernel
ports only. This repository is referenced as a submodule in
[FreeRTOS/FreeRTOS](https://github.com/FreeRTOS/FreeRTOS)
repository, which contains pre-configured demo application projects under
```FreeRTOS/Demo``` directory.

The easiest way to use FreeRTOS is to start with one of the pre-configured demo
application projects.  That way you will have the correct FreeRTOS source files
included, and the correct include paths configured. Once a demo application is
building and executing you can remove the demo application files, and start to
add in your own application source files.  See the
[FreeRTOS Kernel Quick Start Guide](https://www.freertos.org/Documentation/01-FreeRTOS-quick-start/01-Beginners-guide/02-Quick-start-guide)
for detailed instructions and other useful links.

Additionally, for FreeRTOS kernel feature information refer to the
[Developer Documentation](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/00-Developer-docs),
and [API Reference](https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/00-TaskHandle).

Also for contributing and creating a Pull Request please refer to
[the instructions here](.github/CONTRIBUTING.md#contributing-via-pull-request).

**FreeRTOS-Kernel V11.1.0
[source code](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0) is part
of the
[FreeRTOS 202406.00 LTS](https://github.com/FreeRTOS/FreeRTOS-LTS/tree/202406-LTS)
release.**

### Getting help

If you have any questions or need assistance troubleshooting your FreeRTOS project,
we have an active community that can help on the
[FreeRTOS Community Support Forum](https://forums.freertos.org).

## To consume FreeRTOS-Kernel

### Consume with CMake

If using CMake, it is recommended to use this repository using FetchContent.
Add the following into your project's main or a subdirectory's `CMakeLists.txt`:

- Define the source and version/tag you want to use:

```cmake
FetchContent_Declare( freertos_kernel
  GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
  GIT_TAG        main #Note: Best practice to use specific git-hash or tagged version
)
```

In case you prefer to add it as a git submodule, do:

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git <path of the submodule>
git submodule update --init
```

- Add a freertos_config library (typically an INTERFACE library) The following assumes the directory structure:
  - `include/FreeRTOSConfig.h`

```cmake
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM
INTERFACE
    include
)

target_compile_definitions(freertos_config
  INTERFACE
    projCOVERAGE_TEST=0
)
```

In case you installed FreeRTOS-Kernel as a submodule, you will have to add it as a subdirectory:

```cmake
add_subdirectory(${FREERTOS_PATH})
```

- Configure the FreeRTOS-Kernel and make it available
  - this particular example supports a native and cross-compiled build option.

```cmake
set( FREERTOS_HEAP "4" CACHE STRING "" FORCE)
# Select the native compile PORT
set( FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)
# Select the cross-compile PORT
if (CMAKE_CROSSCOMPILING)
  set(FREERTOS_PORT "GCC_ARM_CA9" CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(freertos_kernel)
```

- In case of cross compilation, you should also add the following to `freertos_config`:

```cmake
target_compile_definitions(freertos_config INTERFACE ${definitions})
target_compile_options(freertos_config INTERFACE ${options})
```

### Consuming stand-alone - Cloning this repository

To clone using HTTPS:

```
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

Using SSH:

```
git clone git@github.com:FreeRTOS/FreeRTOS-Kernel.git
```

## Repository structure

- The root of this repository contains the three files that are common to
every port - list.c, queue.c and tasks.c.  The kernel is contained within these
three files.  croutine.c implements the optional co-routine functionality - which
is normally only used on very memory limited systems.

- The ```./portable``` directory contains the files that are specific to a particular microcontroller and/or compiler.
See the readme file in the ```./portable``` directory for more information.

- The ```./include``` directory contains the real time kernel header files.

- The ```./template_configuration``` directory contains a sample `FreeRTOSConfig.h` to help jumpstart a new project.
See the [FreeRTOSConfig.h](examples/template_configuration/FreeRTOSConfig.h) file for instructions.

### Code Formatting

FreeRTOS files are formatted using the
"[uncrustify](https://github.com/uncrustify/uncrustify)" tool.
The configuration file used by uncrustify can be found in the
[FreeRTOS/CI-CD-GitHub-Actions's](https://github.com/FreeRTOS/CI-CD-Github-Actions)
[uncrustify.cfg](https://github.com/FreeRTOS/CI-CD-Github-Actions/tree/main/formatting)
file.

### Line Endings

File checked into the FreeRTOS-Kernel repository use unix-style LF line endings
for the best compatibility with git.

For optimal compatibility with Microsoft Windows tools, it is best to enable
the git autocrlf feature. You can enable this setting for the current
repository using the following command:

```
git config core.autocrlf true
```

### Git History Optimizations

Some commits in this repository perform large refactors which touch many lines
and lead to unwanted behavior when using the `git blame` command. You can
configure git to ignore the list of large refactor commits in this repository
with the following command:

```
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

### Spelling and Formatting

We recommend using [Visual Studio Code](https://code.visualstudio.com),
commonly referred to as VSCode, when working on the FreeRTOS-Kernel.
The FreeRTOS-Kernel also uses [cSpell](https://cspell.org/) as part of its
spelling check. The config file for which can be found at [cspell.config.yaml](cspell.config.yaml)
There is additionally a
[cSpell plugin for VSCode](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker)
that can be used as well.
*[.cSpellWords.txt](.github/.cSpellWords.txt)* contains words that are not
traditionally found in an English dictionary. It is used by the spellchecker
to verify the various jargon, variable names, and other odd words used in the
FreeRTOS code base are correct. If your pull request fails to pass the spelling
and you believe this is a mistake, then add the word to
*[.cSpellWords.txt](.github/.cSpellWords.txt)*. When adding a word please
then sort the list, which can be done by running the bash command:
`sort -u .cSpellWords.txt -o .cSpellWords.txt`
Note that only the FreeRTOS-Kernel Source Files, [include](include),
[portable/MemMang](portable/MemMang), and [portable/Common](portable/Common)
files are checked for proper spelling, and formatting at this time.

## Third Party Tools
Visit [this link](.github/third_party_tools.md) for detailed information about
third-party tools with FreeRTOS support.
