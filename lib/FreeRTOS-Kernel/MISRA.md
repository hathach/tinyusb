# MISRA Compliance

FreeRTOS-Kernel conforms to [MISRA C:2012](https://www.misra.org.uk/misra-c)
guidelines, with the deviations listed below. Compliance is checked with
Coverity static analysis version 2023.6.1. Since the FreeRTOS kernel is
designed for small-embedded devices, it needs to have a very small memory
footprint and has to be efficient. To achieve that and to increase the
performance, it deviates from some MISRA rules. The specific deviations,
suppressed inline, are listed below.

Additionally, [MISRA configuration file](examples/coverity/coverity_misra.config)
contains project wide deviations.

### Suppressed with Coverity Comments
To find the violation references in the source files run grep on the source code
with ( Assuming rule 8.4 violation; with justification in point 1 ):
```
grep 'MISRA Ref 8.4.1' . -rI
```

#### Dir 4.7
MISRA C:2012 Dir 4.7: If a function returns error information, then that error
information shall be tested.

_Ref 4.7.1_
 - `taskENTER_CRITICAL_FROM_ISR` returns the interrupt mask and not any error
    information. Therefore, there is no need test the return value.

#### Rule 8.4

MISRA C:2012 Rule 8.4: A compatible declaration shall be visible when an
object or function with external linkage is defined.

_Ref 8.4.1_
 - pxCurrentTCB(s) is defined with external linkage but it is only referenced
   from the assembly code in the port files. Therefore, adding a declaration in
   header file is not useful as the assembly code will still need to declare it
   separately.

_Ref 8.4.2_
 - xQueueRegistry is defined with external linkage because it is accessed by the
   kernel unit tests. It is not meant to be directly accessed by the application
   and therefore, not declared in a header file.

#### Rule 8.6

MISRA C:2012 Rule 8.6: An identifier with external linkage shall have exactly
one external definition.

_Ref 8.6.1_
 - This rule prohibits an identifier with external linkage to have multiple
   definitions or no definition. FreeRTOS hook functions are implemented in
   the application and therefore, have no definition in the Kernel code.

#### Rule 11.1
MISRA C:2012 Rule 11.1: Conversions shall not be performed between a pointer to
function and any other type.

_Ref 11.1.1_
 - The pointer to function is casted into void to avoid unused parameter
   compiler warning when Stream Buffer's Tx and Rx Completed callback feature is
   not used.

#### Rule 11.3

MISRA C:2012 Rule 11.3: A cast shall not be performed between a pointer to
object type and a pointer to a different object type.

_Ref 11.3.1_
 - This rule prohibits casting a pointer to object into a pointer to a
   different object because it may result in an incorrectly aligned pointer,
   leading to undefined behavior. Even if the casting produces a correctly
   aligned pointer, the behavior may be still undefined if the pointer is
   used to access an object. FreeRTOS deliberately creates external aliases
   for all the kernel object types (StaticEventGroup_t, StaticQueue_t,
   StaticStreamBuffer_t, StaticTimer_t and StaticTask_t) for data hiding
   purposes. The internal object types and the corresponding external
   aliases are guaranteed to have the same size and alignment which is
   checked using configASSERT.


#### Rule 11.5

MISRA C:2012 Rule 11.5: A conversion should not be performed from pointer to
void into pointer to object.
This rule prohibits conversion of a pointer to void into a pointer to
object because it may result in an incorrectly aligned pointer leading
to undefined behavior.

_Ref 11.5.1_
 - The memory blocks returned by pvPortMalloc() are guaranteed to meet the
   architecture alignment requirements specified by portBYTE_ALIGNMENT.
   The casting of the pointer to void returned by pvPortMalloc() is,
   therefore, safe because it is guaranteed to be aligned.

_Ref 11.5.2_
 - The conversion from a pointer to void into a pointer to EventGroup_t is
   safe because it is a pointer to EventGroup_t, which is returned to the
   application at the time of event group creation for data hiding
   purposes.

_Ref 11.5.3_
 - The conversion from a pointer to void in list macros for list item owner
   is safe because the type of the pointer stored and retrieved is the
   same.

_Ref 11.5.4_
 - The conversion from a pointer to void into a pointer to EventGroup_t is
   safe because it is a pointer to EventGroup_t, which is passed as a
   parameter to the xTimerPendFunctionCallFromISR API when the callback is
   pended.

_Ref 11.5.5_
 - The conversion from a pointer to void into a pointer to uint8_t is safe
   because data storage buffers are implemented as uint8_t arrays for the
   ease of sizing, alignment and access.

#### Rule 21.6

MISRA C-2012 Rule 21.6: The Standard Library input/output functions shall not
be used.

_Ref 21.6.1_
 - The Standard Library function snprintf is used in vTaskListTasks and
   vTaskGetRunTimeStatistics APIs, both of which are utility functions only and
   are not considered part of core kernel implementation.
