#include "1608.h"

#define TEMP1_HIGH_INDEX 2U
#define TEMP1_LOW_INDEX  3U
#define TEMP2_HIGH_INDEX 4U
#define TEMP2_LOW_INDEX  5U

static u8 is_valid_index(u8 index)
{
	return (index < (u8)sizeof(tem)) ? 1U : 0U;
}

static int read_temp_word(u8 high_index, u8 low_index)
{
	u16 temp_high;
	u16 temp_low;
	u16 temp_word;

	if ((is_valid_index(high_index) == 0U) || (is_valid_index(low_index) == 0U))
	{
		return 0;
	}

	__disable_irq();
	temp_high = (u16)tem[high_index];
	temp_low = (u16)tem[low_index];
	__enable_irq();

	temp_word = (u16)((temp_high << 8) | temp_low);
	return (int)temp_word;
}

int tem_trans(void)
{
	return read_temp_word(TEMP1_HIGH_INDEX, TEMP1_LOW_INDEX);
}

int tem2_trans(void)
{
	return read_temp_word(TEMP2_HIGH_INDEX, TEMP2_LOW_INDEX);
}
