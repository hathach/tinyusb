ceedling-module-generator
=========================

## Overview

The module_generator plugin adds a pair of new commands to Ceedling, allowing
you to make or remove modules according to predefined templates. WIth a single call,
Ceedling can generate a source, header, and test file for a new module. If given a
pattern, it can even create a series of submodules to support specific design patterns.
Finally, it can just as easily remove related modules, avoiding the need to delete
each individually.

Let's say, for example, that you want to create a single module named `MadScience`.

```
ceedling module:create[MadScience]
```

It says we're speaking to the module plugin, and we want to create a new module. The
name of that module is between the brackets. It will keep this case, unless you have
specified a different default (see configuration). It will create three files:
`MadScience.c`, `MadScience.h`, and `TestMadScience.c`. *NOTE* that it is important that
there are no spaces between the brackets. We know, it's annoying... but it's the rules.

You can also create an entire pattern of files. To do that, just add a second argument
to the pattern ID. Something like this:

```
ceedling module:create[SecretLair,mch]
```

In this example, we'd create 9 files total: 3 headers, 3 source files, and 3 test files. These
files would be named `SecretLairModel`, `SecretLairConductor`, and `SecretLairHardware`. Isn't
that nice?

Similarly, you can create stubs for all functions in a header file just by making a single call
to your handy `stub` feature, like this:

```
ceedling module:stub[SecretLair]
```

This call will look in SecretLair.h and will generate a file SecretLair.c that contains a stub
for each function declared in the header! Even better, if SecretLair.c already exists, it will
add only new functions, leaving your existing calls alone so that it doesn't cause any problems.

## Configuration

Enable the plugin in your project.yml by adding `module_generator`
to the list of enabled plugins.

Then, like much of Ceedling, you can just run as-is with the defaults, or you can override those
defaults for your own needs. For example, new source and header files will be automatically
placed in the `src/` folder while tests will go in the `test/` folder. That's great if your project
follows the default ceedling structure... but what if you have a different structure?

```
:module_generator:
  :project_root: ./
  :source_root: source/
  :inc_root: includes/
  :test_root: tests/
```

Now I've redirected the location where modules are going to be generated.

### Includes

You can make it so that all of your files are generated with a standard include list. This is done
by adding to the `:includes` array. For example:

```
:module_generator:
  :includes:
    :tst:
      - defs.h
      - board.h
    :src:
      - board.h
```

### Boilerplates

You can specify the actual boilerplate used for each of your files. This is the handy place to
put that corporate copyright notice (or maybe a copyleft notice, if that's your perference?)

```
:module_generator:
  :boilerplates: |
    /***************************
    * This file is Awesome.    *
    * That is All.             *
    ***************************/
```

### Test Defines

You can specify the "#ifdef TEST" at the top of the test files with a custom define.
This example will put a "#ifdef CEEDLING_TEST" at the top of the test files.  

```
:module_generator:
  :test_define: CEEDLING_TEST
```

### Naming Convention

Finally, you can force a particular naming convention. Even if someone calls the generator
with something like `MyNewModule`, if they have the naming convention set to `:caps`, it will
generate files like `MY_NEW_MODULE.c`. This keeps everyone on your team behaving the same way.

Your options are as follows:

  - `:bumpy` - BumpyFilesLooksLikeSo
  - `:camel` - camelFilesAreSimilarButStartLow
  - `:snake` - snake_case_is_all_lower_and_uses_underscores
  - `:caps`  - CAPS_FEELS_LIKE_YOU_ARE_SCREAMING


