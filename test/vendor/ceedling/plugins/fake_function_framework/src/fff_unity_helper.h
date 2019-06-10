#ifndef fff_unity_helper_H
#define fff_unity_helper_H

/*
    FFF helper macros for Unity.
*/

/*
    Fail if the function was not called the expected number of times.
*/
#define TEST_ASSERT_CALLED_TIMES(times_, function_)                         \
    TEST_ASSERT_EQUAL_MESSAGE(times_,                                       \
        function_ ## _fake.call_count,                                      \
        "Function " #function_ " called the incorrect number of times.")
/*
    Fail if the function was not called exactly once.
*/
#define TEST_ASSERT_CALLED(function_) TEST_ASSERT_CALLED_TIMES(1, function_)

/*
    Fail if the function was called 1 or more times.
*/
#define TEST_ASSERT_NOT_CALLED(function_) TEST_ASSERT_CALLED_TIMES(0, function_)

/*
    Fail if the function was not called in this particular order.
*/
#define TEST_ASSERT_CALLED_IN_ORDER(order_, function_)                      \
    TEST_ASSERT_EQUAL_PTR_MESSAGE((void *) function_,                       \
        fff.call_history[order_],                                           \
        "Function " #function_ " not called in order " #order_ )

#endif