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
    :pre_test
    :post_test
    :pre_release
    :post_release
    :pre_build
    :post_build
```

Each of these tools can support an :executable string and an :arguments list, like so:

```
:tools:
  :post_link_execute:
    :executable: objcopy.exe
    :arguments:
      - ${1} #This is replaced with the executable name
      - output.srec
      - --strip-all
```

You may also specify an array of executables to be called in a particular place, like so:

```
:tools:
  :post_test:
    -  :executable: echo
       :arguments: "${1} was glorious!"
    -  :executable: echo
       :arguments:
         - it kinda made me cry a little.
         - you?
```

Please note that it varies which arguments are being parsed down to the
hooks. For now see `command_hooks.rb` to figure out which suits you best.
Happy Tweaking!
