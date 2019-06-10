
CException
==========

CException is a basic exception framework for C, suitable for use in
embedded applications.  It provides an exception framework similar in
use to C++, but with much less overhead.


CException uses C standard library functions `setjmp` and `longjmp` to
operate.  As long as the target system has these two functions defined,
this library should be useable with very little configuration.  It
even supports environments where multiple program flows are in use,
such as real-time operating systems.


There are about a gabillion exception frameworks using a similar
setjmp/longjmp method out there... and there will probably be more
in the future. Unfortunately, when we started our last embedded
project, all those that existed either (a) did not support multiple
tasks (therefore multiple stacks) or (b) were way more complex than
we really wanted.  CException was born.


*Why use CException?*


0. It's ANSI C, and it beats passing error codes around.
1. You want something simple... CException throws a single id. You can
   define those ID's to be whatever you like. You might even choose which
   type that number is for your project. But that's as far as it goes.
   We weren't interested in passing objects or structs or strings...
   just simple error codes.
2. Performance... CException can be configured for single tasking or
   multitasking. In single tasking, there is very little overhead past
   the setjmp/longjmp calls (which are already fast). In multitasking,
   your only additional overhead is the time it takes you to determine
   a unique task id 0 - num_tasks.


For the latest version, go to [ThrowTheSwitch.org](http://throwtheswitch.org)


CONTENTS OF THIS DOCUMENT
=========================

* Usage
* Limitations
*API
* Configuration
* Testing
* License


Usage
-----

Code that is to be protected are wrapped in `Try { } Catch { }` blocks.
The code directly following the Try call is "protected", meaning that
if any Throws occur, program control is directly transferred to the
start of the Catch block.


A numerical exception ID is included with Throw, and is made accessible
from the Catch block.


Throws can occur from within function calls (nested as deeply as you
like) or directly from within the function itself.



Limitations
-----------


This library was made to be as fast as possible, and provide basic
exception handling.  It is not a full-blown exception library.  Because
of this, there are a few limitations that should be observed in order
to successfully utilize this library:

1. Do not directly "return" from within a `Try` block, nor `goto`
   into or out of a `Try` block.

   *Why?*

   The `Try` macro allocates some local memory and alters a global
   pointer.  These are cleaned up at the top of the `Catch` macro.
   Gotos and returns would bypass some of these steps, resulting in
   memory leaks or unpredictable behavior.


2. If (a) you change local (stack) variables within your `Try` block,
   AND (b) wish to make use of the updated values after an exception
   is thrown, those variables should be made `volatile`. Note that this
   is ONLY for locals and ONLY when you need access to them after a
   `Throw`.

   *Why?*

   Compilers optimize.  There is no way to guarantee that the actual
   memory location was updated and not just a register unless the
   variable is marked volatile.


3. Memory which is `malloc`'d or `new`'d is not automatically released
   when an error is thrown.  This will sometimes be desirable, and
   othertimes may not.  It will be the responsibility of the `Catch`
   block to perform this kind of cleanup.

   *Why?*

   There's just no easy way to track `malloc`'d memory, etc., without
   replacing or wrapping malloc calls or something like that.  This
   is a light framework, so these options were not desirable.



API
---

###Try

`Try` is a macro which starts a protected block.  It MUST be followed by
a pair of braces or a single protected line (similar to an 'if'),
enclosing the data that is to be protected.  It **must** be followed by a
`Catch` block (don't worry, you'll get compiler errors to let you know if
you mess any of that up).


###Catch(e)

`Catch` is a macro which ends the `Try` block and starts the error handling
block. The `Catch` block is called if and only if an exception was thrown
while within the `Try` block.  This error was thrown by a `Throw` call
somewhere within `Try` (or within a function called within `Try`, or a function
called by a function called within `Try`, etc).

The single parameter `e` is filled with the error code which was thrown.
This can be used for reporting, conditional cleanup, etc. (or you can just
ignore it if you really want... people ignore return codes all the time,
right?).  `e` should be of type `EXCEPTION_T`


###Throw(e)

This is the method of throwing an error. A `Throw` should only occur from within a
protected (`Try` ... `Catch`) block, though it may easily be nested many function
calls deep without an impact on performance or functionality. `Throw` takes
a single argument, which is an exception id which will be passed to `Catch`
as the reason for the error.

If you wish to rethrow an error, this can be done by calling `Throw(e)` with
the error code you just caught.  It **is** valid to throw from a catch block.


###ExitTry()

On rare occasion, you might want to immediately exit your current `Try` block
but **not** treat this as an error. Don't run the `Catch`. Just start executing
from after the `Catch` as if nothing had happened... That's what `ExitTry` is
for.


CONFIGURATION
-------------

CException is a mostly portable library.  It has one universal
dependency, and some macros which are required if working in a
multi-tasking environment.

1. The standard C library setjmp must be available.  Since this is part
   of the standard library, chances are good that you'll be fine.

2. If working in a multitasking environment, methods for obtaining an
   index into an array of frames and to get the overall number of
   id's are required.  If the OS supports a method to retrieve Task
   ID's, and those Tasks are number 0, 1, 2... you are in an ideal
   situation.  Otherwise, a more creative mapping function may be
   required.  Note that this function is likely to be called twice
   for each protected block and once during a throw.  This is the
   only overhead in the system.


Exception.h
-----------

By convention, most projects include `Exception.h` which defines any
further requirements, then calls `CException.h` to do the gruntwork.  All
of these are optional.  You could directly include `CException.h` if
you wanted and just use the defaults provided.

* `EXCEPTION_T`
  * Set this to the type you want your exception id's to be.  Defaults to 'unsigned int'.

* `EXCEPTION_NONE`
  * Set this to a number which will never be an exception id in your system.  Defaults to `0x5a5a5a5a`.

* `EXCEPTION_GET_ID`
  * If in a multi-tasking environment, this should be
    set to be a call to the function described in #2 above.
    Defaults to just return `0` all the time (good for
    single tasking environments)

* `EXCEPTION_NUM_ID`
  * If in a multi-tasking environment, this should be set
    to the number of ID's required (usually the number of
    tasks in the system).  Defaults to `1` (for single
    tasking environments).

* `CEXCEPTION_NO_CATCH_HANDLER(id)`
  * This macro can be optionally specified.
    It allows you to specify code to be called when a Throw
    is made outside of `Try` ... `Catch` protection.  Consider
    this the emergency fallback plan for when something has
    gone terribly wrong.


You may also want to include any header files which will commonly be
needed by the rest of your application where it uses exception handling
here.  For example, OS header files or exception codes would be useful.


Finally, there are some hook macros which you can implement to inject
your own target-specific code in particular places. It is a rare instance
where you will need these, but they are here if you need them:


* `CEXCEPTION_HOOK_START_TRY`
  * called immediately before the Try block

* `CEXCEPTION_HOOK_HAPPY_TRY`
  * called immediately after the Try block if no exception was thrown

* `CEXCEPTION_HOOK_AFTER_TRY`
  * called immediately after the Try block OR before an exception is caught

* `CEXCEPTION_HOOK_START_CATCH`
  * called immediately before the catch



TESTING
-------


If you want to validate that CException works with your tools or that
it works with your custom configuration, you may want to run the test
suite.


The test suite included makes use of the `Unity` Test Framework.  It will
require a native C compiler. The example makefile uses MinGW's gcc.
Modify the makefile to include the proper paths to tools, then run `make`
to compile and run the test application.

* `C_COMPILER`
  * The C compiler to use to perform the tests

* `C_LIBS`
  * The path to the C libraries (including setjmp)

* `UNITY_DIR`
  * The path to the Unity framework (required to run tests)
    (get it at [ThrowTheSwitch.org](http://throwtheswitch.org))



LICENSE
-------

This software is licensed under the MIT License

Copyright (c) 2007-2017 Mark VanderVoord

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
