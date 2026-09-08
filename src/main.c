
/**
 * @file main.c
 * @brief Hauptprogramm der STM32-Anwendung.
 *
 * Dieses Modul initialisiert die Systemuhr, den SysTick-Timer sowie
 * den GPIO-Port C und lässt anschließend eine LED an GPIOC Pin 13
 * periodisch blinken.
 *
 * Die Anwendung verwendet libopencm3 für den direkten Zugriff auf
 * die STM32-Hardware.
 *
 * @author
 * @date 2026-09-04
 *
 * @details
 * Programmablauf:
 * - Initialisierung der Systemtakt-Konfiguration über clock_setup().
 * - Initialisierung des SysTick-Timers über systick_setup().
 * - Aktivierung des Peripherietakts für GPIOC.
 * - Konfiguration von GPIOC Pin 13 als Push-Pull-Ausgang ohne Pull-Up
 *   bzw. Pull-Down.
 * - Setzen des Ausgangs auf HIGH.
 * - Periodisches Umschalten des Ausgangszustands im Abstand von 500 ms.
 *
 * Dadurch entsteht ein Blinksignal mit einer Periodendauer von
 * ungefähr 1 Sekunde, sofern delay_ms() die angegebene Verzögerung
 * von 500 ms korrekt einhält.
 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/systick.h>

#include "clock.h"


/**
 * @brief Einstiegspunkt des STM32-Programms.
 *
 * Initialisiert zunächst die für die Anwendung benötigten
 * Systemkomponenten und konfiguriert anschließend GPIOC Pin 13
 * als digitalen Ausgang.
 *
 * Nach der Initialisierung wird der GPIO-Pin in einer Endlosschleife
 * alle 500 ms zwischen HIGH und LOW umgeschaltet. Ist an diesem Pin
 * eine LED angeschlossen, führt dies zu einem periodischen Blinken.
 *
 * @return int
 *         Wird bei einer Embedded-Anwendung normalerweise nicht
 *         erreicht, da das Programm dauerhaft in der Endlosschleife
 *         verbleibt.
 *
 * @note
 * GPIOC muss vor der Konfiguration des Pins durch
 * rcc_periph_clock_enable() aktiviert werden.
 *
 * @note
 * Die konkrete Funktion von GPIOC Pin 13 hängt von der verwendeten
 * STM32-Hardware und der Beschaltung der LED ab. Bei vielen
 * STM32-Entwicklungsboards ist an diesem Pin eine integrierte LED
 * angeschlossen.
 */
int main(void)
{
    /**
     * @brief Initialisierung der Systemtakt-Konfiguration.
     *
     * clock_setup() konfiguriert den System- bzw. Peripherietakt
     * entsprechend der projektspezifischen Einstellungen.
     */
    clock_setup();

    /**
     * @brief Initialisierung des SysTick-Timers.
     *
     * Der SysTick-Timer wird für zeitbasierte Funktionen wie
     * delay_ms() verwendet.
     */
    systick_setup();

    /**
     * @brief Aktiviert den Takt für GPIOC.
     *
     * STM32-GPIO-Peripherie muss zunächst mit einem Peripherietakt
     * versorgt werden, bevor die Register des GPIO-Ports verwendet
     * werden können.
     */
    rcc_periph_clock_enable(RCC_GPIOC);

    /**
     * @brief Konfiguriert GPIOC Pin 13 als Ausgang.
     *
     * GPIOC Pin 13 wird als digitaler Ausgang ohne internen Pull-Up
     * oder Pull-Down-Widerstand konfiguriert.
     *
     * @param GPIOC       GPIO-Port C
     * @param GPIO_MODE_OUTPUT
     *                    Betriebsart als digitaler Ausgang
     * @param GPIO_PUPD_NONE
     *                    Keine internen Pull-Up-/Pull-Down-Widerstände
     * @param GPIO13      Zu konfigurierender Pin 13
     */
    gpio_mode_setup(
        GPIOC,
        GPIO_MODE_OUTPUT,
        GPIO_PUPD_NONE,
        GPIO13
    );

    /**
     * @brief Setzt GPIOC Pin 13 auf HIGH.
     *
     * Der Ausgang wird initial auf HIGH gesetzt, bevor die
     * Blinkschleife beginnt.
     */
    gpio_set(GPIOC, GPIO13);

    /**
     * @brief Endlosschleife zum Erzeugen des Blinksignals.
     *
     * Der Zustand von GPIOC Pin 13 wird kontinuierlich umgeschaltet.
     * Nach jedem Umschalten wartet das Programm 500 ms.
     *
     * Dadurch wird der Ausgang alle 500 ms zwischen HIGH und LOW
     * gewechselt. Ein vollständiger Blinkzyklus dauert damit
     * ungefähr 1000 ms bzw. 1 Sekunde.
     */
    while (1)
    {
        /**
         * @brief Schaltet GPIOC Pin 13 in den jeweils anderen Zustand.
         *
         * HIGH wird zu LOW und LOW wird zu HIGH.
         */
        gpio_toggle(GPIOC, GPIO13);

        /**
         * @brief Wartet 500 Millisekunden.
         *
         * Die Verzögerung basiert auf der zuvor initialisierten
         * SysTick-Konfiguration.
         */
        delay_ms(500);
    }
}
