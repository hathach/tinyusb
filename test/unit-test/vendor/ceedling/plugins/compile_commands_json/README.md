compile_commands_json
=====================

## Overview

Syntax highlighting and code completion are hard. Historically each editor or IDE has implemented their own and then competed amongst themselves to offer the best experience for developers. Often developers would still to an IDE that felt cumbersome and slow just because it had the best syntax highlighting on the market. If doing it for one language is hard (and it is) imagine doing it for dozens of them. Imagine a full stack developer who has to work with CSS, HTML, JavaScript and some Ruby - they need excellent support in all those languages which just made things even harder.

In June of 2016, Microsoft with Red Hat and Codenvy got together to create a standard called the Language Server Protocol (LSP). The idea was simple, by standardising on one protocol, all the IDEs and editors out there would only have to support LSP, and not have custom plugins for each language. In turn, the backend code that actually does the highlighting can be written once and used by any IDE that supports LSP. Many editors already support it such as Sublime Text, vim and emacs. This means that if you're using a crufty old IDE or worse, you're using a shiny new editor without code completion, then this could be just the upgrade you're looking for!

For C and C++ projects, many people use the `clangd` backend. So that it can do things like "go to definition", `clangd` needs to know how to build the project so that it can figure out all the pieces to the puzzle. There are manual tools such as `bear` which can be run with `gcc` or `clang` to extract this information it has a big limitation in that if run with `ceedling release` you won't get any auto completion for Unity and you'll also get error messages reported by your IDE because of what it perceives as missing headers. If you do the same with `ceedling test` now you get Unity but you might miss things that are only seen in the release build.

This plugin resolves that issue. As it is run by Ceedling, it has access to all the build information it needs to create the perfect `compile_commands.json`. Once enabled, this plugin will generate that file and place it in `./build/artifacts/compile_commands.json`. `clangd` will search your project for this file, but it is easier to symlink it into the root directory (for example `ln -s ./build/artifacts/compile_commands.json`.

For more information on LSP and to find out if your editor supports it, check out https://langserver.org/

## Setup

Enable the plugin in your project.yml by adding `compile_commands_json` to the list
of enabled plugins.

``` YAML
:plugins:
  :enabled:
    - compile_commands_json
```

## Configuration

There is no additional configuration necessary to run this plugin.
