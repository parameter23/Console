/**

* @file clock.h
* @brief Schnittstelle für Systemtakt- und Zeitbasis-Funktionen.
*
* Dieses Header-File stellt die öffentliche Schnittstelle des
* Clock-/SysTick-Moduls zur Verfügung.
*
* Das Modul übernimmt zwei wesentliche Aufgaben:
*
* * Initialisierung des Systemtakts des STM32-Mikrocontrollers.
* * Bereitstellung einer Millisekunden-Zeitbasis über den ARM-Cortex-M
* SysTick-Timer.
*
* Der SysTick-Timer erzeugt periodische Interrupts. Bei jedem Interrupt
* wird ein interner 32-Bit-Zähler um 1 erhöht. Dadurch entsteht eine
* Zeitbasis mit einer Auflösung von 1 Millisekunde.
*
* Die Funktion systick() ermöglicht das Auslesen dieses Zählerstands.
* Mit delay_ms() kann auf einfache Weise eine blockierende Verzögerung
* erzeugt werden.
*
* @note
* Die konkrete SysTick-Frequenz wird in systick_setup() festgelegt.
* Die Implementierung dieses Projekts verwendet eine Tick-Frequenz
* von 1 kHz, sodass ein Tick einer Millisekunde entspricht.
*
* @note
* Die Funktionen dieses Moduls basieren auf der libopencm3-Bibliothek.
  */

#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/**

* @brief Initialisiert den Systemtakt des Mikrocontrollers.
*
* Konfiguriert die Taktversorgung des STM32 über die in der
* Implementierung definierte PLL-Konfiguration.
*
* Nach erfolgreicher Initialisierung steht der für das Projekt
* vorgesehene Systemtakt zur Verfügung.
*
* Die Funktion sollte während der Systeminitialisierung möglichst
* früh aufgerufen werden, bevor Peripherie initialisiert wird, deren
* Funktion von einer definierten Taktfrequenz abhängt.
*
* @note
* Die genaue Taktfrequenz und PLL-Konfiguration sind in der
* Implementierung von clock_setup() festgelegt.
*
* @see systick_setup()
  */
  void clock_setup(void);

/**

* @brief Initialisiert den SysTick-Timer.
*
* Konfiguriert den ARM-Cortex-M SysTick-Timer als periodische
* Zeitbasis für das System.
*
* Der SysTick-Timer wird so eingerichtet, dass regelmäßig ein
* Interrupt ausgelöst wird. Der zugehörige Interrupt-Handler
* sys_tick_handler() erhöht bei jedem Interrupt den internen
* Millisekunden-Zähler.
*
* Im aktuellen Projekt entspricht ein SysTick-Interrupt einer
* Zeitspanne von 1 Millisekunde.
*
* Die Funktion muss aufgerufen werden, bevor die Funktionen
* systick() oder delay_ms() sinnvoll verwendet werden können.
*
* @note
* Die SysTick-Konfiguration ist abhängig von der eingestellten
* System- bzw. AHB-Taktfrequenz.
*
* @see sys_tick_handler()
* @see systick()
* @see delay_ms()
  */
  void systick_setup(void);

/**

* @brief Behandelt einen SysTick-Interrupt.
*
* Diese Funktion ist der Interrupt-Service-Routine (ISR) für den
* ARM-Cortex-M SysTick-Timer.
*
* Bei jedem ausgelösten SysTick-Interrupt wird der interne
* Millisekunden-Zähler um eins erhöht.
*
* Bei einer Tick-Frequenz von 1 kHz entspricht jeder Aufruf
* einem Zeitfortschritt von 1 Millisekunde.
*
* Die Funktion wird normalerweise nicht direkt aus dem
* Anwendungsprogramm aufgerufen, sondern automatisch durch die
* Interrupt-Vektor-Tabelle aufgerufen.
*
* @note
* Der Funktionsname entspricht der von libopencm3 verwendeten
* Bezeichnung für den SysTick-Interrupt-Handler.
*
* @warning
* Die Funktion sollte nicht aus dem normalen Programmablauf
* aufgerufen werden.
*
* @see systick_setup()
* @see systick()
  */
  void sys_tick_handler(void);

/**

* @brief Liefert den aktuellen Wert der Millisekunden-Zeitbasis.
*
* Gibt den aktuellen Wert des internen 32-Bit-Zählers zurück.
*
* Der Zähler wird durch sys_tick_handler() bei jedem
* SysTick-Interrupt erhöht.
*
* Bei einer SysTick-Frequenz von 1 kHz entspricht der Rückgabewert
* ungefähr der Anzahl der seit der Initialisierung vergangenen
* Millisekunden.
*
* Die Funktion kann beispielsweise verwendet werden, um nicht
* blockierende Zeitsteuerungen zu implementieren:
*
* @code
* uint32_t start = systick();
*
* if ((systick() - start) >= 1000) {
* ```
  // 1000 ms bzw. ungefähr 1 Sekunde vergangen
  ```
* }
* @endcode
*
* Die Differenzbildung mit unsigned Integern berücksichtigt dabei
* auch einen Überlauf des 32-Bit-Zählers.
*
* @return
* Aktueller Wert des internen Millisekunden-Zählers.
*
* @note
* Bei einer Auflösung von 1 ms läuft ein 32-Bit-Zähler nach ungefähr
* 49,7 Tagen über und beginnt wieder bei 0.
*
* @see sys_tick_handler()
* @see delay_ms()
  */
  uint32_t systick(void);

/**

* @brief Erzeugt eine blockierende Verzögerung in Millisekunden.
*
* Wartet die angegebene Anzahl von Millisekunden, bevor die Funktion
* zurückkehrt.
*
* Intern wird der aktuelle Wert von systick() beim Funktionsaufruf
* gespeichert. Anschließend wird so lange gewartet, bis die
* gewünschte Zeitspanne verstrichen ist.
*
* Beispiel:
*
* @code
* delay_ms(500);
* @endcode
*
* wartet ungefähr 500 Millisekunden.
*
* Ein weiteres Beispiel für eine Sekunde:
*
* @code
* delay_ms(1000);
* @endcode
*
* @param ms
* Anzahl der zu wartenden Millisekunden.
*
* @note
* Die Funktion arbeitet blockierend. Während der Wartezeit wird
* die aufrufende Programmausführung angehalten.
*
* @note
* Der SysTick-Interrupt läuft während der Verzögerung weiter,
* sodass der Millisekunden-Zähler weiterhin aktualisiert wird.
*
* @warning
* Diese Funktion verwendet eine Busy-Wait-Schleife und verbraucht
* während der Wartezeit CPU-Rechenzeit. Für komplexere Anwendungen
* oder längere Wartezeiten sollte eine nicht-blockierende
* Zeitsteuerung oder ein geeigneter Sleep-Modus verwendet werden.
*
* @warning
* Die Genauigkeit der Verzögerung hängt von der korrekten
* Konfiguration des Systemtakts und des SysTick-Timers ab.
*
* @see systick_setup()
* @see systick()
  */
  void delay_ms(uint32_t ms);

#endif /* CLOCK_H */
