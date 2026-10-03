#define _GNU_SOURCE
#include "flash_store.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static int fail_erase;

void FLASH_Unlock(void) {}
void FLASH_Lock(void) {}
void FLASH_ClearFlag(uint32_t flags) { (void)flags; }
FLASH_Status FLASH_ErasePage(uint32_t address)
{
    if (fail_erase) return FLASH_ERROR_PG;
    memset((void *)(uintptr_t)address, 0xFF, 1024);
    return FLASH_COMPLETE;
}
FLASH_Status FLASH_ProgramHalfWord(uint32_t address, uint16_t value)
{
    memcpy((void *)(uintptr_t)address, &value, sizeof(value));
    return FLASH_COMPLETE;
}

int main(void)
{
    const uintptr_t mapped_page = (uintptr_t)FLASH_STORE_PAGE_START & ~(uintptr_t)4095;
    void *memory = mmap((void *)mapped_page, 4096, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(memory == (void *)mapped_page);
    SystemConfig_t *flash = (SystemConfig_t *)(uintptr_t)FLASH_STORE_PAGE_START;

    FlashStore_SetDefaults();
    CHECK(FlashStore_CRC_Verify() == 1U);
    CHECK(FlashStore_Save() == 1U);
    memset(&g_tSystemConfig, 0, sizeof(g_tSystemConfig));
    CHECK(FlashStore_Load() == 1U);
    CHECK(g_tSystemConfig.ulMagic == FLASH_STORE_MAGIC);

    /* A valid header and a damaged payload must not publish partial state. */
    ((uint8_t *)flash)[offsetof(SystemConfig_t, cWiFiSSID)] ^= 1U;
    SystemConfig_t before = g_tSystemConfig;
    CHECK(FlashStore_Load() == 0U);
    CHECK(memcmp(&g_tSystemConfig, &before, sizeof(before)) == 0);

    fail_erase = 1;
    CHECK(FlashStore_Init() == 0U);
    CHECK(g_tSystemConfig.ulMagic == FLASH_STORE_MAGIC);
    fail_erase = 0;
    CHECK(FlashStore_Init() == 1U);
    CHECK(FlashStore_Load() == 1U);
    CHECK(munmap(memory, 4096) == 0);
    return 0;
}
