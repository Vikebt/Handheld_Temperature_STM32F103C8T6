#ifndef FUSION_POLICY_H
#define FUSION_POLICY_H

#include <stdint.h>

uint8_t FusionPolicy_IsMeasurementReady(int16_t iTemperature,
                                        uint8_t ucCardDetected,
                                        const uint8_t aucCardUID[4]);
uint16_t FusionPolicy_CRC16(const uint8_t *pucData, uint16_t usLength);

#endif
