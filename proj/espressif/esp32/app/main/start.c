#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "soc/dport_reg.h"
#include "esp32/rom/cache.h"



/* For cpu_start*/
#include "esp_attr.h"
#include "esp_err.h"

#include "esp_log.h"
#include "esp_system.h"

#include "esp_rom_uart.h"

#include "esp_clk_internal.h"
#include "esp_rom_efuse.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"


/* esp32 specific */
#include "soc/dport_reg.h"
#include "esp32/rtc.h"
#include "esp32/cache_err_int.h"
#include "esp32/rom/cache.h"
#include "esp32/rom/rtc.h"
#include "esp32/rom/spi_flash.h"
#include "esp32/spiram.h"


#include "bootloader_flash_config.h"
#include "esp_private/crosscore_int.h"
#include "esp_flash_encrypt.h"

#include "hal/rtc_io_hal.h"
#include "hal/gpio_hal.h"
#include "hal/wdt_hal.h"
#include "soc/rtc.h"
#include "soc/efuse_reg.h"
#include "soc/periph_defs.h"
#include "soc/cpu.h"
#include "soc/rtc.h"
#include "soc/spinlock.h"

#include "bootloader_mem.h"


#include "esp_private/startup_internal.h"
#include "esp_private/system_internal.h"

#include "port/boot_wdt_port.h"
#include "port/boot_log_port.h"


extern int _bss_start;
extern int _bss_end;
extern int _rtc_bss_start;
extern int _rtc_bss_end;

extern int _vector_table;

static const char *TAG = "cpu_start";

#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
extern int _ext_ram_bss_start;
extern int _ext_ram_bss_end;
#endif
#ifdef CONFIG_ESP32_IRAM_AS_8BIT_ACCESSIBLE_MEMORY
extern int _iram_bss_start;
extern int _iram_bss_end;
#endif
#endif // CONFIG_IDF_TARGET_ESP32

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
static volatile bool s_cpu_up[SOC_CPU_CORES_NUM] = { false };
static volatile bool s_cpu_inited[SOC_CPU_CORES_NUM] = { false };

static volatile bool s_resume_cores;
#endif

// If CONFIG_SPIRAM_IGNORE_NOTFOUND is set and external RAM is not found or errors out on testing, this is set to false.
bool g_spiram_ok = true;


#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
void startup_resume_other_cores(void)
{
    s_resume_cores = true;
}

void IRAM_ATTR call_start_cpu1(void)
{
    cpu_hal_set_vecbase(&_vector_table);

    ets_set_appcpu_boot_addr(0);

    bootloader_init_mem();

#if CONFIG_ESP_CONSOLE_UART_NONE
    esp_rom_install_channel_putc(1, NULL);
    esp_rom_install_channel_putc(2, NULL);
#else // CONFIG_ESP_CONSOLE_UART_NONE
    esp_rom_install_uart_printf();
    esp_rom_uart_set_as_console(CONFIG_ESP_CONSOLE_UART_NUM);
#endif

#if CONFIG_IDF_TARGET_ESP32
    DPORT_REG_SET_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_PDEBUG_ENABLE | DPORT_APP_CPU_RECORD_ENABLE);
    DPORT_REG_CLR_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_RECORD_ENABLE);
#else
    REG_WRITE(ASSIST_DEBUG_CORE_1_RCD_PDEBUGENABLE_REG, 1);
    REG_WRITE(ASSIST_DEBUG_CORE_1_RCD_RECORDING_REG, 1);
#endif

    s_cpu_up[1] = true;
    ESP_EARLY_LOGI(TAG, "App cpu up.");

    //Take care putting stuff here: if asked, FreeRTOS will happily tell you the scheduler
    //has started, but it isn't active *on this CPU* yet.
    esp_cache_err_int_init();

#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_ESP32_TRAX_TWOBANKS
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif
#endif

    s_cpu_inited[1] = true;

    while (!s_resume_cores) {
        esp_rom_delay_us(100);
    }

    SYS_STARTUP_FN();
}

static void start_other_core(void)
{
    // If not the single core variant of ESP32 - check this since there is
    // no separate soc_caps.h for the single core variant.
    bool is_single_core = false;
#if CONFIG_IDF_TARGET_ESP32
    is_single_core = REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_DIS_APP_CPU);
#endif
    if (!is_single_core) {
        ESP_EARLY_LOGI(TAG, "Starting app cpu, entry point is %p", call_start_cpu1);

#if CONFIG_IDF_TARGET_ESP32
        Cache_Flush(1);
        Cache_Read_Enable(1);
#endif
        esp_cpu_unstall(1);

        // Enable clock and reset APP CPU. Note that OpenOCD may have already
        // enabled clock and taken APP CPU out of reset. In this case don't reset
        // APP CPU again, as that will clear the breakpoints which may have already
        // been set.
#if CONFIG_IDF_TARGET_ESP32
        if (!DPORT_GET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN)) {
            DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
            DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_C_REG, DPORT_APPCPU_RUNSTALL);
            DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
            DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
        }
#elif CONFIG_IDF_TARGET_ESP32S3
        if (!REG_GET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_CLKGATE_EN)) {
            REG_SET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_CLKGATE_EN);
            REG_CLR_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RUNSTALL);
            REG_SET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RESETING);
            REG_CLR_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RESETING);
        }
#endif
        ets_set_appcpu_boot_addr((uint32_t)call_start_cpu1);

        volatile bool cpus_up = false;

        while (!cpus_up) {
            cpus_up = true;
            for (int i = 0; i < SOC_CPU_CORES_NUM; i++) {
                cpus_up &= s_cpu_up[i];
            }
            esp_rom_delay_us(100);
        }
    }
}
#endif // !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE


static void intr_matrix_clear(void)
{
    for (int i = 0; i < ETS_MAX_INTR_SOURCE; i++) {
        intr_matrix_set(0, i, ETS_INVALID_INUM);
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        intr_matrix_set(1, i, ETS_INVALID_INUM);
#endif
    }
}




/*------------------------------------------------------------------------------------------------------------*/






/* Cache MMU block size */
#define MMU_BLOCK_SIZE    0x00010000

/* Cache MMU address mask (MMU tables ignore bits which are zero) */
#define MMU_FLASH_MASK    (~(MMU_BLOCK_SIZE - 1))

/* TODO: Get app primary slot from MCUBoot code */
#ifndef CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS
    #define CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS 0x00020000
#endif

extern uint32_t _image_irom_vma;
extern uint32_t _image_irom_lma;
extern uint32_t _image_irom_size;

extern uint32_t _image_drom_vma;
extern uint32_t _image_drom_lma;
extern uint32_t _image_drom_size;





static inline uint32_t count_mmu_pages( uint32_t size, uint32_t vaddr )
{

    return (size + (vaddr - (vaddr & MMU_FLASH_MASK)) + MMU_BLOCK_SIZE - 1) / MMU_BLOCK_SIZE;
}

IRAM_ATTR static int map_rom_segments( void )
{
    uint32_t rc = ESP_OK;

    /* Cache ROM segments into cache */
    volatile uint32_t irom_lma = (uint32_t)&_image_irom_lma;
    volatile uint32_t irom_vma = (uint32_t)&_image_irom_vma;
    volatile uint32_t irom_size = (uint32_t)&_image_irom_size;

    volatile uint32_t drom_lma = (uint32_t)&_image_drom_lma;
    volatile uint32_t drom_vma = (uint32_t)&_image_drom_vma;
    volatile uint32_t drom_size = (uint32_t)&_image_drom_size;

    volatile uint32_t irom_abs_lma = CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS + irom_lma;
    volatile uint32_t irom_abs_vma = irom_vma;
    
    volatile uint32_t drom_abs_lma = CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS + drom_lma;
    volatile uint32_t drom_abs_vma = drom_vma;

    /* Disable and flush PRO CPU cache */
    Cache_Read_Disable(0);
    Cache_Flush(0);

    /* Invalidate MMU entries, for fresh start with app */
    for(int i=0; i<DPORT_FLASH_MMU_TABLE_SIZE; i++)
    {
        DPORT_PRO_FLASH_MMU_TABLE[i] = DPORT_FLASH_MMU_TABLE_INVALID_VAL;
    }

    /* Verify alignment reqired for MMU */
    uint32_t drom_aligned_lma = drom_abs_lma & MMU_FLASH_MASK;
    uint32_t drom_aligned_vma = drom_abs_vma & MMU_FLASH_MASK;
    uint32_t drom_page_count = count_mmu_pages( drom_size, drom_aligned_vma );
    rc |= cache_flash_mmu_set(0, 0, drom_aligned_vma, drom_aligned_lma, 64, drom_page_count);
    rc |= cache_flash_mmu_set(1, 0, drom_aligned_vma, drom_aligned_lma, 64, drom_page_count);

    uint32_t irom_aligned_lma = irom_abs_lma & MMU_FLASH_MASK;
    uint32_t irom_aligned_vma = irom_abs_vma & MMU_FLASH_MASK;
    uint32_t irom_page_count = count_mmu_pages( irom_size, irom_aligned_vma );
    rc |= cache_flash_mmu_set(0, 0, irom_aligned_vma, irom_aligned_lma, 64, irom_page_count);
    rc |= cache_flash_mmu_set(1, 0, irom_aligned_vma, irom_aligned_lma, 64, irom_page_count);

    DPORT_REG_CLR_BIT( DPORT_PRO_CACHE_CTRL1_REG, ( DPORT_PRO_CACHE_MASK_IRAM0 | 
                                                    DPORT_PRO_CACHE_MASK_DROM0 | 
                                                    DPORT_PRO_CACHE_MASK_DRAM1 ));

    DPORT_REG_CLR_BIT( DPORT_APP_CACHE_CTRL1_REG, ( DPORT_APP_CACHE_MASK_IRAM0 | 
                                                    DPORT_APP_CACHE_MASK_DROM0 | 
                                                    DPORT_APP_CACHE_MASK_DRAM1 ));


    Cache_Read_Enable(0);

    return rc;
}

IRAM_ATTR static void start_cpu( void )
{
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    RESET_REASON rst_reas[SOC_CPU_CORES_NUM];
#else
    RESET_REASON rst_reas[1];
#endif

    // Move exception vectors to IRAM
    cpu_hal_set_vecbase(&_vector_table);

    rst_reas[0] = rtc_get_reset_reason(0);
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    rst_reas[1] = rtc_get_reset_reason(1);
#endif

#ifndef CONFIG_BOOTLOADER_WDT_ENABLE
    // from panic handler we can be reset by RWDT or TG0WDT
    if (rst_reas[0] == RTCWDT_SYS_RESET || rst_reas[0] == TG0WDT_SYS_RESET
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
            || rst_reas[1] == RTCWDT_SYS_RESET || rst_reas[1] == TG0WDT_SYS_RESET
#endif
       ) {
        wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_disable(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    }
#endif

    //Clear BSS. Please do not attempt to do any complex stuff (like early logging) before this.
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_ESP32_IRAM_AS_8BIT_ACCESSIBLE_MEMORY)
    // Clear IRAM BSS
    memset(&_iram_bss_start, 0, (&_iram_bss_end - &_iram_bss_start) * sizeof(_iram_bss_start));
#endif

    /* Unless waking from deep sleep (implying RTC memory is intact), clear RTC bss */
    if (rst_reas[0] != DEEPSLEEP_RESET) {
        memset(&_rtc_bss_start, 0, (&_rtc_bss_end - &_rtc_bss_start) * sizeof(_rtc_bss_start));
    }



    bootloader_init_mem();
#if CONFIG_SPIRAM_BOOT_INIT
    if (esp_spiram_init() != ESP_OK) {
#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
        ESP_EARLY_LOGE(TAG, "Failed to init external RAM, needed for external .bss segment");
        abort();
#endif
#endif

#if CONFIG_SPIRAM_IGNORE_NOTFOUND
        ESP_EARLY_LOGI(TAG, "Failed to init external RAM; continuing without it.");
        g_spiram_ok = false;
#else
        ESP_EARLY_LOGE(TAG, "Failed to init external RAM!");
        abort();
#endif
    }
    if (g_spiram_ok) {
        esp_spiram_init_cache();
    }
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    s_cpu_up[0] = true;
#endif

    ESP_EARLY_LOGI(TAG, "Pro cpu up.");

#if SOC_CPU_CORES_NUM > 1 // there is no 'single-core mode' for natively single-core processors
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    start_other_core();
#else
    ESP_EARLY_LOGI(TAG, "Single core mode");
#if CONFIG_IDF_TARGET_ESP32
    DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN); // stop the other core
#elif CONFIG_IDF_TARGET_ESP32S3
    REG_CLR_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_CLKGATE_EN);
#endif
#endif // !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
#endif // SOC_CPU_CORES_NUM > 1

#if CONFIG_SPIRAM_MEMTEST
    if (g_spiram_ok) {
        bool ext_ram_ok = esp_spiram_test();
        if (!ext_ram_ok) {
            ESP_EARLY_LOGE(TAG, "External RAM failed memory test!");
            abort();
        }
    }
#endif

#if CONFIG_SPIRAM_FETCH_INSTRUCTIONS
    extern void instruction_flash_page_info_init(void);
    instruction_flash_page_info_init();
#endif
#if CONFIG_SPIRAM_RODATA
    extern void rodata_flash_page_info_init(void);
    rodata_flash_page_info_init();
#endif

#if CONFIG_SPIRAM_FETCH_INSTRUCTIONS
    extern void esp_spiram_enable_instruction_access(void);
    esp_spiram_enable_instruction_access();
#endif
#if CONFIG_SPIRAM_RODATA
    extern void esp_spiram_enable_rodata_access(void);
    esp_spiram_enable_rodata_access();
#endif

#if CONFIG_ESP32S2_INSTRUCTION_CACHE_WRAP || CONFIG_ESP32S2_DATA_CACHE_WRAP
    uint32_t icache_wrap_enable = 0, dcache_wrap_enable = 0;
#if CONFIG_ESP32S2_INSTRUCTION_CACHE_WRAP
    icache_wrap_enable = 1;
#endif
#if CONFIG_ESP32S2_DATA_CACHE_WRAP
    dcache_wrap_enable = 1;
#endif
    extern void esp_enable_cache_wrap(uint32_t icache_wrap_enable, uint32_t dcache_wrap_enable);
    esp_enable_cache_wrap(icache_wrap_enable, dcache_wrap_enable);
#endif

#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
    memset(&_ext_ram_bss_start, 0, (&_ext_ram_bss_end - &_ext_ram_bss_start) * sizeof(_ext_ram_bss_start));
#endif

//Enable trace memory and immediately start trace.
#if CONFIG_ESP32_TRAX || CONFIG_ESP32S2_TRAX
#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_ESP32_TRAX_TWOBANKS
    trax_enable(TRAX_ENA_PRO_APP);
#else
    trax_enable(TRAX_ENA_PRO);
#endif
#elif CONFIG_IDF_TARGET_ESP32S2
    trax_enable(TRAX_ENA_PRO);
#endif
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif // CONFIG_ESP32_TRAX || CONFIG_ESP32S2_TRAX

    esp_clk_init();
    esp_perip_clk_init();

    // Now that the clocks have been set-up, set the startup time from RTC
    // and default RTC-backed system time provider.
    g_startup_time = esp_rtc_get_time_us();

    intr_matrix_clear();

#ifdef CONFIG_ESP_CONSOLE_UART
    uint32_t clock_hz = rtc_clk_apb_freq_get();
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
    clock_hz = UART_CLK_FREQ_ROM; // From esp32-s3 on, UART clock source is selected to XTAL in ROM
#endif
    esp_rom_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_rom_uart_set_clock_baudrate(CONFIG_ESP_CONSOLE_UART_NUM, clock_hz, CONFIG_ESP_CONSOLE_UART_BAUDRATE);
#endif

#if SOC_RTCIO_HOLD_SUPPORTED
    rtcio_hal_unhold_all();
#else
    gpio_hal_force_unhold_all();
#endif

    esp_cache_err_int_init();

#if CONFIG_ESP_SYSTEM_MEMPROT_FEATURE
#if CONFIG_ESP_SYSTEM_MEMPROT_FEATURE_LOCK
    esp_memprot_set_prot(true, true, NULL);
#else
    esp_memprot_set_prot(true, false, NULL);
#endif
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    s_cpu_inited[0] = true;

    volatile bool cpus_inited = false;

    while (!cpus_inited) {
        cpus_inited = true;
        for (int i = 0; i < SOC_CPU_CORES_NUM; i++) {
            cpus_inited &= s_cpu_inited[i];
        }
        esp_rom_delay_us(100);
    }
#endif

    SYS_STARTUP_FN();
}

void vLog( const char *pcFormat, ... )
{
    va_list args;
    va_start(args, pcFormat);
    vprintf( pcFormat, args );
    va_end(args);
}


/* Initialize prerequisite hardware then run app */
IRAM_ATTR void __start( void )
{
    map_rom_segments();
    boot_port_wdt_feed();
    boot_port_wdt_disable();

    /* Call typical esp32 entry */
    start_cpu();

    while(1);
}

__attribute__((section(".entry"), used)) static void (*__entry_point)(void) = &__start;
