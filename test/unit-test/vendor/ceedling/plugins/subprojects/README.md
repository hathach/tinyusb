ceedling-subprojects
====================

Plugin for supporting subprojects that are built as static libraries. It continues to support
dependency tracking, without getting confused between your main project files and your
subproject files. It accepts different compiler flags and linker flags, allowing you to
optimize for your situation.

First, you're going to want to add the extension to your list of known extensions:

```
:extension:
  :subprojects: '.a'
```

Define a new section called :subprojects. There, you can list as many subprojects
as you may need under the :paths key. For each, you specify a unique place to build
and a unique name.

```
:subprojects:
  :paths:
   - :name: libprojectA
     :source:
       - ./subprojectA/first/dir
       - ./subprojectA/second/dir
     :include:
       - ./subprojectA/include/dir
     :build_root: ./subprojectA/build/dir
     :defines:
       - DEFINE_JUST_FOR_THIS_FILE
       - AND_ANOTHER
   - :name: libprojectB
     :source:
       - ./subprojectB/only/dir
     :include:
       - ./subprojectB/first/include/dir
       - ./subprojectB/second/include/dir
     :build_root: ./subprojectB/build/dir
     :defines: [] #none for this one
```

You can specify the compiler and linker, just as you would a release build:

```
:tools:
  :subprojects_compiler:
    :executable: gcc
    :arguments:
      - -g
      - -I"$": COLLECTION_PATHS_SUBPROJECTS
      - -D$: COLLECTION_DEFINES_SUBPROJECTS
      - -c "${1}"
      - -o "${2}"
  :subprojects_linker:
    :executable: ar
    :arguments:
      - rcs
      - ${2}
      - ${1}
```

That's all there is to it! Happy Hacking!
