#include "fusion_policy.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        return 1; \
    } \
} while (0)

int main(void)
{
    static const uint8_t aucUID[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    static const uint8_t aucEmptyUID[4] = {0U, 0U, 0U, 0U};
    static const uint8_t aucKnownVector[] = "123456789";

    CHECK(FusionPolicy_IsMeasurementReady(3650, 1U, aucUID) == 1U);
    CHECK(FusionPolicy_IsMeasurementReady(0, 1U, aucUID) == 0U);
    CHECK(FusionPolicy_IsMeasurementReady(3650, 0U, aucUID) == 0U);
    CHECK(FusionPolicy_IsMeasurementReady(3650, 1U, aucEmptyUID) == 0U);
    CHECK(FusionPolicy_IsMeasurementReady(3650, 1U, 0) == 0U);

    CHECK(FusionPolicy_CRC16(aucKnownVector,
                              (uint16_t)(sizeof(aucKnownVector) - 1U)) == 0xFEE8U);
    CHECK(FusionPolicy_CRC16(0, 9U) == 0U);
    return 0;
}
