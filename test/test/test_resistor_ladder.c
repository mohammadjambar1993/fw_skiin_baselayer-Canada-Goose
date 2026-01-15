#ifdef TEST

#include "unity.h"
#include "resistor_ladder.h"
#include "mock_hal_gpio.h"
#include "apptypes.h"

static const IOPin outpins[] = {REF_R1,REF_R2,REF_R3,REF_R4,REF_R5,REF_R6,REF_R7};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_resistor_ladder_initialization(void)
{
	hal_gpio_write_Ignore();
	//fail if null pointer is passed
    TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, rladder_init(NULL, 1));
    //fail if # of outputs is 0
    TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, rladder_init(outpins, 0));
    //success
    uint8_t nchannels = sizeof(outpins)/sizeof(outpins[0]);
    TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, rladder_init(outpins, nchannels));
}

void test_resistor_ladder_outputs_initialization(void)
{
	uint8_t nchannels = sizeof(outpins)/sizeof(outpins[0]);
	for(uint8_t i = 0; i < nchannels; i++)
	{
		hal_gpio_write_Expect(outpins[i], 0);
	}
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, rladder_init(outpins, nchannels));
}

#endif // TEST
