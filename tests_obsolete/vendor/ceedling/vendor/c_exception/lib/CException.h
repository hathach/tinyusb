#ifndef _CEXCEPTION_H
#define _CEXCEPTION_H

#include <setjmp.h>

#ifdef __cplusplus
extern "C"
{
#endif


//To Use CException, you have a number of options:
//1. Just include it and run with the defaults
//2. Define any of the following symbols at the command line to override them
//3. Include a header file before CException.h everywhere which defines any of these
//4. Create an Exception.h in your path, and just define EXCEPTION_USE_CONFIG_FILE first

#ifdef CEXCEPTION_USE_CONFIG_FILE
#include "CExceptionConfig.h"
#endif

//This is the value to assign when there isn't an exception
#ifndef CEXCEPTION_NONE
#define CEXCEPTION_NONE      (0x5A5A5A5A)
#endif

//This is number of exception stacks to keep track of (one per task)
#ifndef CEXCEPTION_NUM_ID
#define CEXCEPTION_NUM_ID    (1) //there is only the one stack by default
#endif

//This is the method of getting the current exception stack index (0 if only one stack)
#ifndef CEXCEPTION_GET_ID
#define CEXCEPTION_GET_ID    (0) //use the first index always because there is only one anyway
#endif

//The type to use to store the exception values.
#ifndef CEXCEPTION_T
#define CEXCEPTION_T         unsigned int
#endif

//This is an optional special handler for when there is no global Catch
#ifndef CEXCEPTION_NO_CATCH_HANDLER
#define CEXCEPTION_NO_CATCH_HANDLER(id)
#endif

//exception frame structures
typedef struct {
  jmp_buf* pFrame;
  CEXCEPTION_T volatile Exception;
} CEXCEPTION_FRAME_T;

//actual root frame storage (only one if single-tasking)
extern volatile CEXCEPTION_FRAME_T CExceptionFrames[];

//Try (see C file for explanation)
#define Try                                                         \
    {                                                               \
        jmp_buf *PrevFrame, NewFrame;                               \
        unsigned int MY_ID = CEXCEPTION_GET_ID;                     \
        PrevFrame = CExceptionFrames[CEXCEPTION_GET_ID].pFrame;     \
        CExceptionFrames[MY_ID].pFrame = (jmp_buf*)(&NewFrame);     \
        CExceptionFrames[MY_ID].Exception = CEXCEPTION_NONE;        \
        if (setjmp(NewFrame) == 0) {                                \
            if (&PrevFrame) 

//Catch (see C file for explanation)
#define Catch(e)                                                    \
            else { }                                                \
            CExceptionFrames[MY_ID].Exception = CEXCEPTION_NONE;    \
        }                                                           \
        else                                                        \
        { e = CExceptionFrames[MY_ID].Exception; e=e; }             \
        CExceptionFrames[MY_ID].pFrame = PrevFrame;                 \
    }                                                               \
    if (CExceptionFrames[CEXCEPTION_GET_ID].Exception != CEXCEPTION_NONE)

//Throw an Error
void Throw(CEXCEPTION_T ExceptionID);

#ifdef __cplusplus
}   // extern "C"
#endif


#endif // _CEXCEPTION_H
