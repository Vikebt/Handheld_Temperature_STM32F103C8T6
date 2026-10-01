#include "fusion_policy.h"

uint8_t FusionPolicy_IsMeasurementReady(int16_t iTemperature,
                                        uint8_t ucCardDetected,
                                        const uint8_t aucCardUID[4])
{
    uint8_t ucUIDNonZero;

    if ((iTemperature == 0) || (ucCardDetected == 0U) || (aucCardUID == 0))
    {
        return 0U;
    }

    ucUIDNonZero = (uint8_t)(aucCardUID[0] | aucCardUID[1] |
                             aucCardUID[2] | aucCardUID[3]);
    return (ucUIDNonZero != 0U) ? 1U : 0U;
}

uint16_t FusionPolicy_CRC16(const uint8_t *pucData, uint16_t usLength)
{
    uint16_t usCRC = 0U;
    uint16_t usIndex;
    uint8_t ucBit;

    if (pucData == 0)
    {
        return 0U;
    }

    for (usIndex = 0U; usIndex < usLength; usIndex++)
    {
        usCRC ^= (uint16_t)pucData[usIndex] << 8;
        for (ucBit = 0U; ucBit < 8U; ucBit++)
        {
            usCRC = (usCRC & 0x8000U) ?
                    (uint16_t)((usCRC << 1) ^ 0x8005U) :
                    (uint16_t)(usCRC << 1);
        }
    }

    return usCRC;
}
