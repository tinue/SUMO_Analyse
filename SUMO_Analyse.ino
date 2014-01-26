/**
 * Analyse eines RC Summensignales. Ein Summensignal sollte so aufgebaut sein:
 * - Kanal 1 beginnt mit einem kurzen Puls auf +5 Volt. Der Puls selber hat eine gewisse Länge, die aber nicht gemessen wird.
 * - Nach dem Puls folgt eine Pause, bis Kanal 2 mit dem nächsten Puls eingeleitet wird. Je länger diese Pause ist, desto höher ist der Servowert des Kanales
 * - Puls plus Pause dauern zwischen 1000 und 2000 Mikrosekunden. Mittelstellung des Servos is 1500 Mikrosekunden
 * - Bei 8 Kanälen ist der Zyklus nach spätestens 8*2000 = 16 Millisekunden fertig
 * - Das ganze dauert 20 Millisekunden. Kanal 1 kann daher erkannt werden, wenn die Pause vor dem Puls mehr als ca. 2500 Millisekunden dauert.
 *
 * Probleme kann es theoretisch geben:
 * - Gesamtdauer ist zu tief (< als ca. 19ms): Dann kann Kanal 1 nicht zuverlässig erkannt werden, sofern alle Kanäle auf Maximum sind.
 * - Auf der Fernsteuerung werden die Servowege auf bis zu 150% eingestellt. Wenn dies bei allen Kanälen passiert,
 *   und alle Kanäle auf Maximum sind, kann es auch das obige Problem geben (puls plus Pause dauert dann deutlich mehr als 2000 Mikrosekunden).
 *
 * Das Programm ist für Arduino 1 geschrieben. Dieser läuft mit 16MHz, und Input Kanäle 2 und 3 können Interrupt-getrieben ausgelesen werden.
 * Für andere Aduinos müssen wahrscheinlich Input Kanal und der Divisor für den Zähler angepasst werden.
*/

#define PPM_SIGNAL 2  // Pin für Input-Signal (Orange oder Gelbe Ader des Kabels). Beim Uno Board gehen nur 2 oder 3, da nur diese Interrupt-fähig sind
unsigned int channel[9];  // Array für die Puls/Pausenlänge der einzelnen Kanäle plus die Pause nach Kanal 8
boolean readyToShow = false; // Die Anzeige der Kanalwerte muss mit der Ausgabe der Werte synchronisiert werden. True = kann angezeigt werden.
int currentChannel = -1; // Aktueller Kanal. Dieser ist erst bekannt, nachdem die Pause nach Kanal 8 einmal erkannt worden ist. -1 heisst "unbekannt".

void setup() {
  setupPPM();
  Serial.begin(115200);
}

void loop() {
  unsigned int total = channel[8]; // Summiere alle Kanäle plus die Pause
  if (readyToShow) {
    // Ausgeben aller Werte des soeben beendeten Zykluses
    for (int i = 0; i < 8; i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(channel[i]);
      Serial.print("; ");
      total += channel[i];
    }
    Serial.print("Pause: ");
    Serial.print(channel[8]);
    Serial.print("; Total = ");
    Serial.println(total);
    readyToShow = false; // Selber Wert nicht nochmals ausgeben; Array mit Kanalwerten wieder zum Schreiben freigeben.
    currentChannel = -1; // Sicherstellen, dass erst wieder gelesen wird, nachdem der Pausenimpuls empfangen wurde.
    delay(2000); // Pause, damit man auch Zeit hat, sich die Resultate anzusehen
  }
}

void setupPPM(void) {
  // Setup für die PPM Analyse
  pinMode(PPM_SIGNAL, INPUT); // Input-Pin auf lesen einstellen
  attachInterrupt(PPM_SIGNAL - 2, read_ppm, RISING); // Setzen des Interrupts (0 für Pin 2, 1 für Pin 3). Routine wird ausgelöst wenn der Wert am Pin auf 1 geht.

  // Timer 1 wird zum Messen der Pulsdauer verwendet
  TCCR1A = 0;  // Reset des Timers
  TCCR1B = 0;  // Timer stoppen
  TCCR1B |= (1 << CS11);  // Timer start, Scaler 1/8: Jede 0.5 Mikrosekunden zählt der Timer hoch
}

void read_ppm() {  // Interrupt-Service Routine: Wert am Pin hat auf 1 gewechselt
  static unsigned int pauseDuration; // Zeit seit dem letzten Wechsel auf 1

  pauseDuration = TCNT1; // Zählerwert auslesen
  TCNT1 = 0; // Reset des Zählers
  pauseDuration = pauseDuration >> 1; // Umrechnen in Mikrosekunden

  if (!readyToShow) { // Nur auslesen, wenn erlaubt
    if (pauseDuration > 2500) { // Pause über 2.5 ms zeigt den Start eines Zykluses an
      if (currentChannel != -1) { // Der allererste Durchlauf kann in der Mitte starten, deshalb muss einmal die Pause abgewartet werden.
        // Wir sind an Ende eines gültigen Auslesezykluses
        channel[8] = pauseDuration;
        readyToShow = true; // Alle Kanäle ausgelesen, bereit zur Anzeige
      }
      currentChannel = 0; // Auf jeden Fall Kanal auf 0
    } else {  // Keine Schluss-Pause, sondern Kanalwert
      if (currentChannel != -1) {  // Wir sind mitten in einem gültigen Auslesezyklus
        if (currentChannel < 8) { // Nur 8 Kanäle sind unterstützt
          channel[currentChannel] = pauseDuration; // Wert ins Array schreiben
        }
        currentChannel++; // Kanal hochzählen
      } // Sonst wird noch gewartet, bis der Kanal bekannt ist.
    }
  }
}
