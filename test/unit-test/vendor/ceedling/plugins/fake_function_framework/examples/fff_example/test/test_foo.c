#include "unity.h"
#include "foo.h"
#include "mock_bar.h"
#include "mock_zzz.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_foo(void)
{
	//When
	foo_turn_on();

	//Then
	TEST_ASSERT_EQUAL(1, bar_turn_on_fake.call_count);
	TEST_ASSERT_EQUAL(1, zzz_sleep_fake.call_count);
	TEST_ASSERT_EQUAL_STRING("sleepy", zzz_sleep_fake.arg1_val);
}

void test_foo_again(void)
{
	//When
	foo_turn_on();

	//Then
	TEST_ASSERT_EQUAL(1, bar_turn_on_fake.call_count);
}

void test_foo_mock_with_const(void)
{
	foo_print_message("123");

	TEST_ASSERT_EQUAL(1, bar_print_message_fake.call_count);
	TEST_ASSERT_EQUAL_STRING("123", bar_print_message_fake.arg0_val);
}

void test_foo_mock_with_variable_args(void)
{
	foo_print_special_message();
	TEST_ASSERT_EQUAL(1, bar_print_message_formatted_fake.call_count);
	TEST_ASSERT_EQUAL_STRING("The numbers are %d, %d and %d", bar_print_message_formatted_fake.arg0_val);
}
