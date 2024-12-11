# README for FreeRTOS-Kernel/examples

The easiest way to use FreeRTOS is to start with one of the pre-configured demo application projects.
See [FreeRTOS/FreeRTOS/Demo](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo) to find a list of pre-configured demos on multiple platforms which demonstrate the working of the FreeRTOS-Kernel.
This directory aims to further facilitate the beginners in building their first FreeRTOS project.


## Directory Structure:

* The [cmake_example](./cmake_example) directory contains a minimal FreeRTOS example project, which uses the configuration file in the template_configuration directory listed below. This will provide you with a starting point for building your applications using FreeRTOS-Kernel.
* The [coverity](./coverity) directory contains a project to run [Synopsys Coverity](https://www.synopsys.com/software-integrity/static-analysis-tools-sast/coverity.html) for checking MISRA compliance. This directory contains further readme files and links to documentation.
* The [template_configuration](./template_configuration) directory contains a sample configuration file FreeRTOSConfig.h which helps you in preparing your application configuration


## Additional examples

Additional examples of the kernel being used in real life applications in tandem with many other libraries (i.e. FreeRTOS+TCP, coreMQTT, coreHTTP etc.) can be found [here](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS-Plus/Demo).
