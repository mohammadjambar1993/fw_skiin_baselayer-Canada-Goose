#include "hal_config.h"
#include "hal_clock.h"

#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)

#define FREQ_HFCLK      64000000
#define FREQ_PCLK32     32000000
#define FREQ_PCLK16     16000000
#define FREQ_PCLK1      1000000
#define FREQ_PCLK32K    32768

ClkState hal_clk_init(void)
{
    return CLKST_OK;
}

uint32_t hal_clk_getFrequency(ClkSrc src)
{
    uint32_t hz;
    switch (src)
    {
        case CLKSRC_CORE:
            hz = FREQ_HFCLK;
        break;
        case CLKSRC_BUS1:
            hz = FREQ_PCLK32;
        break;
        case CLKSRC_BUS2:
            hz = FREQ_PCLK16;
        break;
        case CLKSRC_BUS3:
            hz = FREQ_PCLK1;
        break;
        case CLKSRC_BUS4:
            hz = FREQ_PCLK32K;
        break;
        default:
            hz = 0;
    }
    return hz;
}

#endif //UC == MK22FX512
