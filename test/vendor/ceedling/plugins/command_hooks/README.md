ceedling-command-hooks
======================

Plugin for easily calling command line tools at various points in the build process

Define any of these sections in :tools: to provide additional hooks to be called on demand:

```
    :pre_mock_generate
    :post_mock_generate
    :pre_runner_generate
    :post_runner_generate
    :pre_compile_execute
    :post_compile_execute
    :pre_link_execute
    :post_link_execute
    :pre_test_fixture_execute
    :pre_test_fixture_execute
    :pre_test
    :post_test
    :pre_release
    :post_release
    :pre_build
    :post_build
```

Each of these tools can support an :executable string and an :args list, like so:

```
:tools:
  :post_link_execute:
    :executable: objcopy.exe
    :args:
      - ${1} #This is replaced with the executable name
      - output.srec
      - --strip-all
```

You may also specify an array of executables to be called in a particular place, like so:

```
:tools:
  :post_test:
    -  :executable: echo
       :args: "${1} was glorious!"
    -  :executable: echo
       :args:
         - it kinda made me cry a little.
         - you?
```

Happy Tweaking!
