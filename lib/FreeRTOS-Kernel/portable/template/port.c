/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * license and copyright intentionally withheld to promote copying into user code.
 */

#include "FreeRTOS.h"
#include "task.h"

BaseType_t xPortStartScheduler( void )
{
    return pdTRUE;
}

void vPortEndScheduler( void )
{
}

StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    ( void ) pxTopOfStack;
    ( void ) pvParameters;
    ( void ) * pxCode;

    return NULL;
}

void vPortYield( void )
{
    /* Save the current Context */

    /* Switch to the highest priority task that is ready to run. */
    #if ( configNUMBER_OF_CORES == 1 )
    {
        vTaskSwitchContext();
    }
    #else
    {
        vTaskSwitchContext( portGET_CORE_ID() );
    }
    #endif

    /* Start executing the task we have just switched to. */
}

static void prvTickISR( void )
{
    /* Interrupts must have been enabled for the ISR to fire, so we have to
     * save the context with interrupts enabled. */

    #if ( configNUMBER_OF_CORES == 1 )
    {
        /* Maintain the tick count. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            /* Switch to the highest priority task that is ready to run. */
            vTaskSwitchContext();
        }
    }
    #else
    {
        UBaseType_t ulPreviousMask;

        /* Tasks or ISRs running on other cores may still in critical section in
         * multiple cores environment. Incrementing tick needs to performed in
         * critical section. */
        ulPreviousMask = taskENTER_CRITICAL_FROM_ISR();

        /* Maintain the tick count. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            /* Switch to the highest priority task that is ready to run. */
            vTaskSwitchContext( portGET_CORE_ID() );
        }

        taskEXIT_CRITICAL_FROM_ISR( ulPreviousMask );
    }
    #endif /* if ( configNUMBER_OF_CORES == 1 ) */

    /* start executing the new task */
}
