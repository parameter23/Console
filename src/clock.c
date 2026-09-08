/**
 * @file clock.c
 * @brief Konfiguration des Systemtakts und Implementierung einer Millisekunden-Zeitbasis.
 *
 * Dieses Modul übernimmt zwei wesentliche Aufgaben:
 *
 * 1. Konfiguration des STM32-Systemtakts auf 100 MHz.
 * 2. Bereitstellung einer einfachen Zeitbasis in Millisekunden über
 *    den ARM Cortex-M SysTick-Timer.
 *
 * Die Systemtaktkonfiguration verwendet eine externe Taktquelle (HSE)
 * und eine PLL, um eine Systemfrequenz von 100 MHz zu erzeugen.
 *
 * Der SysTick-Timer erzeugt anschließend einen Interrupt mit einer
 * Frequenz von 1 kHz. Bei jedem SysTick-Interrupt wird der globale
 * Millisekunden-Zähler `ms_ticks` um eins erhöht.
 *
 * Die Funktion delay_ms() verwendet diesen Zähler, um eine blockierende
 * Verzögerung in Millisekunden bereitzustellen.
 *
 * @warning
 * Die tatsächliche PLL-Konfiguration ist abhängig von der auf dem
 * verwendeten STM32 vorhandenen externen HSE-Taktfrequenz.
 * Die hier verwendeten PLL-Parameter sind für eine HSE-Frequenz von
 * 25 MHz ausgelegt:
 *
 *     PLL Eingang = 25 MHz / 25 = 1 MHz
 *     VCO         = 1 MHz * 200 = 200 MHz
 *     SYSCLK      = 200 MHz / 2 = 100 MHz
 *
 * @note
 * APB1 wird mit 50 MHz betrieben, während APB2 mit 100 MHz betrieben
 * wird. Die genaue Zulässigkeit dieser Werte muss anhand des verwendeten
 * STM32-Datenblatts geprüft werden.
 *
 * @note
 * Für die korrekte Funktion von delay_ms() muss der SysTick-Interrupt
 * tatsächlich mit einer Frequenz von 1 kHz ausgelöst werden.
 */

#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/pwr.h>


/**
 * @brief PLL- und Taktkonfiguration für einen Systemtakt von 100 MHz.
 *
 * Diese Struktur beschreibt die gewünschte Taktkonfiguration für
 * rcc_clock_setup_pll().
 *
 * Die verwendete PLL-Konfiguration basiert auf einer externen
 * HSE-Taktquelle von 25 MHz.
 *
 * Die Frequenzberechnung lautet:
 *
 * @verbatim
 * HSE      = 25 MHz
 * PLLM     = 25
 * PLLN     = 200
 * PLLP     = 2
 *
 * PLL input = 25 MHz / 25
 *           = 1 MHz
 *
 * VCO       = 1 MHz * 200
 *           = 200 MHz
 *
 * SYSCLK    = 200 MHz / 2
 *           = 100 MHz
 * @endverbatim
 *
 * Zusätzlich werden die Frequenzen der AHB-, APB1- und APB2-Busse
 * festgelegt.
 *
 * Bus-Takt:
 *
 * @verbatim
 * AHB   = 100 MHz
 * APB1  =  50 MHz
 * APB2  = 100 MHz
 * @endverbatim
 *
 * APB1 wird dabei durch einen Prescaler von 2 aus dem AHB-Takt
 * abgeleitet. APB2 und AHB verwenden keinen zusätzlichen Prescaler.
 *
 * Für den Flash-Speicher werden Instruction Cache und Data Cache
 * aktiviert und eine Flash-Latenz von 3 Wait States eingestellt.
 *
 * Die Spannungsskalierung wird auf PWR_SCALE1 gesetzt, um den
 * Betrieb bei der vorgesehenen Systemfrequenz zu ermöglichen.
 *
 * @note
 * Die Einstellung `RCC_CFGR_PLLSRC_HSE_CLK` setzt voraus, dass eine
 * geeignete externe HSE-Taktquelle vorhanden und korrekt konfiguriert
 * ist.
 *
 * @warning
 * Die angegebene HSE-Frequenz von 25 MHz muss mit der tatsächlichen
 * Hardware übereinstimmen. Eine abweichende HSE-Frequenz führt zu einer
 * abweichenden PLL-Ausgangsfrequenz.
 */
static const struct rcc_clock_scale clock_100mhz = {
    .pllm = 25,
    .plln = 200,
    .pllp = 2,
    .pllq = 4,

    .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,

    .ppre1 = RCC_CFGR_PPRE_DIV2,
    .ppre2 = RCC_CFGR_PPRE_NODIV,
    .hpre  = RCC_CFGR_HPRE_NODIV,

    .ahb_frequency  = 100000000,
    .apb1_frequency = 50000000,
    .apb2_frequency = 100000000,

    .flash_config =
        FLASH_ACR_ICEN |
        FLASH_ACR_DCEN |
        FLASH_ACR_LATENCY_3WS,

    .voltage_scale = PWR_SCALE1,
};


/**
 * @brief Initialisiert die Systemtakt-Konfiguration.
 *
 * Konfiguriert den STM32-Systemtakt mithilfe der zuvor definierten
 * PLL-Konfiguration `clock_100mhz`.
 *
 * Nach erfolgreicher Ausführung arbeitet der Mikrocontroller mit einer
 * vorgesehenen Systemfrequenz von 100 MHz.
 *
 * Die Funktion übernimmt unter anderem:
 *
 * - Auswahl der PLL als Taktquelle.
 * - Konfiguration der PLL-Teiler und -Multiplikatoren.
 * - Einstellung des AHB-Prescalers.
 * - Einstellung der APB1- und APB2-Prescaler.
 * - Konfiguration der Flash-Wait-States.
 * - Einstellung der Spannungsskalierung.
 *
 * @note
 * Diese Funktion sollte möglichst früh während der Systeminitialisierung
 * aufgerufen werden, bevor zeitkritische Peripherie initialisiert wird.
 *
 * @see clock_100mhz
 */
void clock_setup(void)
{
    rcc_clock_setup_pll(&clock_100mhz);
}


/**
 * @brief Globale Millisekunden-Zeitbasis.
 *
 * Diese Variable enthält die Anzahl der seit der Initialisierung des
 * SysTick-Timers vergangenen Millisekunden.
 *
 * Der Wert wird im Interrupt-Handler sys_tick_handler() bei jedem
 * SysTick-Interrupt inkrementiert.
 *
 * `volatile` ist erforderlich, da die Variable sowohl im normalen
 * Programmablauf als auch innerhalb einer Interrupt-Service-Routine
 * verändert bzw. gelesen wird. Dadurch wird verhindert, dass der
 * Compiler Zugriffe auf die Variable unerwartet optimiert.
 *
 * @note
 * Der Zähler läuft bei Erreichen des maximal darstellbaren Wertes
 * von `uint32_t` über und beginnt anschließend wieder bei 0.
 * Bei einer Auflösung von 1 ms geschieht dies nach ungefähr
 * 49,7 Tagen.
 */
static volatile uint32_t ms_ticks = 0;


/**
 * @brief SysTick-Interrupt-Handler.
 *
 * Diese Interrupt-Service-Routine wird bei jedem Ablauf des
 * SysTick-Timers aufgerufen.
 *
 * Der globale Millisekunden-Zähler `ms_ticks` wird dabei um eins
 * erhöht.
 *
 * Bei einer SysTick-Frequenz von 1 kHz entspricht ein Interrupt
 * einer Zeitspanne von 1 ms.
 *
 * @note
 * Der Funktionsname `sys_tick_handler` entspricht der von libopencm3
 * erwarteten Interrupt-Handler-Bezeichnung.
 *
 * @see systick_setup()
 */
void sys_tick_handler(void)
{
    ms_ticks++;
}


/**
 * @brief Liefert den aktuellen Millisekunden-Zählerstand.
 *
 * Gibt die Anzahl der seit der Initialisierung des SysTick-Timers
 * vergangenen Millisekunden zurück.
 *
 * Die Funktion ermöglicht es, Zeitmessungen durch Differenzbildung
 * zwischen zwei Zählerständen durchzuführen.
 *
 * Beispiel:
 *
 * @code
 * uint32_t start = systick();
 *
 * // ... Programmcode ...
 *
 * if ((systick() - start) >= 1000) {
 *     // Eine Sekunde ist vergangen.
 * }
 * @endcode
 *
 * Die Differenzbildung funktioniert auch bei einem Überlauf des
 * 32-Bit-Zählers, solange Zeitintervalle kleiner als der vollständige
 * Wertebereich des Zählers verwendet werden.
 *
 * @return
 * Aktueller Wert des globalen Millisekunden-Zählers.
 */
uint32_t systick(void)
{
    return ms_ticks;
}


/**
 * @brief Erzeugt eine blockierende Verzögerung in Millisekunden.
 *
 * Die Funktion wartet so lange, bis die angegebene Anzahl von
 * Millisekunden seit dem Aufruf vergangen ist.
 *
 * Zur Zeitmessung wird der aktuelle Wert des SysTick-Zählers
 * gespeichert. Anschließend wird dieser Wert kontinuierlich mit
 * dem aktuellen Zählerstand verglichen.
 *
 * Die Implementierung verwendet bewusst eine Differenz:
 *
 * @code
 * systick() - start
 * @endcode
 *
 * Dadurch funktioniert die Zeitmessung auch dann korrekt, wenn
 * der 32-Bit-Millisekundenzähler während der Wartezeit überläuft.
 *
 * @param ms
 * Anzahl der zu wartenden Millisekunden.
 *
 * @note
 * Diese Funktion ist eine Busy-Wait- bzw. Polling-Verzögerung.
 * Während der Wartezeit wird die CPU nicht angehalten und führt
 * kontinuierlich die Schleifenbedingung aus.
 *
 * @warning
 * Die Funktion blockiert die weitere Programmausführung für die
 * angegebene Zeit. Für komplexere Anwendungen oder längere
 * Wartezeiten sollte stattdessen eine nicht-blockierende
 * Zeitsteuerung verwendet werden.
 *
 * @code
 * delay_ms(500);
 * @endcode
 *
 * wartet ungefähr 500 ms.
 */
void delay_ms(uint32_t ms)
{
    uint32_t start = systick();

    while ((systick() - start) < ms)
    {
        /* Busy wait */
    }
}


/**
 * @brief Initialisiert den ARM Cortex-M SysTick-Timer.
 *
 * Konfiguriert den SysTick-Timer so, dass regelmäßig ein Interrupt
 * erzeugt wird. Dieser Interrupt dient als Zeitbasis für den
 * Millisekunden-Zähler `ms_ticks`.
 *
 * Bei einem AHB-Takt von 100 MHz wird der Reload-Wert auf
 * 100000 - 1 gesetzt.
 *
 * Daraus ergibt sich:
 *
 * @verbatim
 * AHB-Frequenz = 100.000.000 Hz
 *
 * Reload-Wert  = 100.000 - 1
 *              = 99.999
 *
 * Tick-Frequenz = 100.000.000 / 100.000
 *               = 1.000 Hz
 *
 * Periodendauer = 1 / 1.000
 *               = 1 ms
 * @endverbatim
 *
 * Damit wird alle 1 ms ein SysTick-Interrupt ausgelöst.
 * Der Interrupt-Handler sys_tick_handler() erhöht daraufhin
 * `ms_ticks` um eins.
 *
 * Die Initialisierung erfolgt in folgenden Schritten:
 *
 * 1. Setzen des Reload-Wertes.
 * 2. Auswahl des AHB-Taktes als SysTick-Taktquelle.
 * 3. Zurücksetzen des aktuellen SysTick-Zählerstandes.
 * 4. Aktivieren des SysTick-Zählers.
 * 5. Aktivieren des SysTick-Interrupts.
 *
 * @note
 * Diese Funktion setzt voraus, dass `clock_setup()` zuvor aufgerufen
 * wurde und der AHB-Takt tatsächlich 100 MHz beträgt.
 *
 * @warning
 * Wird der Systemtakt nach Aufruf dieser Funktion verändert, stimmt
 * die SysTick-Frequenz möglicherweise nicht mehr mit der angenommenen
 * Frequenz von 1 kHz überein. In diesem Fall muss der Reload-Wert
 * entsprechend angepasst werden.
 *
 * @see clock_setup()
 * @see sys_tick_handler()
 * @see delay_ms()
 */
void systick_setup(void)
{
    /**
     * @brief Setzt den Reload-Wert auf 99.999.
     *
     * Bei einem AHB-Takt von 100 MHz ergibt sich damit eine
     * Interrupt-Frequenz von 1 kHz bzw. ein Zeitintervall von 1 ms.
     */
    systick_set_reload(100000 - 1);

    /**
     * @brief Verwendet den AHB-Takt als SysTick-Taktquelle.
     *
     * Da der AHB-Takt auf 100 MHz eingestellt ist, wird der
     * SysTick-Timer ebenfalls mit 100 MHz betrieben.
     */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);

    /**
     * @brief Setzt den aktuellen SysTick-Zählerstand zurück.
     *
     * Dadurch beginnt die Zeitbasis definiert mit der neuen
     * Konfiguration.
     */
    systick_clear();

    /**
     * @brief Aktiviert den SysTick-Timer.
     */
    systick_counter_enable();

    /**
     * @brief Aktiviert den SysTick-Interrupt.
     *
     * Nach Ablauf des Reload-Zählers wird die
     * `sys_tick_handler()`-Routine aufgerufen.
     */
    systick_interrupt_enable();
}
