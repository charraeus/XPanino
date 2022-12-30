/*********************************************************************************************************//**
 * @file main.cpp
 * @author Christian Harraeus (christian@harraeus.de)
 * @brief Hauptprogramm XPanino.
 *        X-Plane-Panel mit Arduino.
 * @version 0.2
 * @date 2022-12-29
 *
 * Dieses Programm wird auf den Arduino geladen und stellt das Interface zwischen Flugsimulator
 * und der Schalter- und Anzeige-Hardware dar.
 *
 *
 * @todo Doxygen-Dokumentation anschauen und aktualieren, vereinheitlichen usw.
 *
 ************************************************************************************************************/

// Headerdateien der Objekte includen
#include <Arduino.h>
#include <buffer.hpp>
#include <dispatcher.hpp>
#include <ledmatrix.hpp>
#include <m803.hpp>
#include <Switchmatrix.hpp>
// #include <xpdr.hpp>
// #include <xplane.hpp>
//#include <commands.hpp>

// Makros für serielle Schnittstelle definieren
#define _SERIAL_BAUDRATE 115200     /// NOLINT Baudrate für den seriellen Port. Nur hier ändern!!


// Own SwitchMatrix class with specific switch events
class MySwitchMatrix : public SwitchMatrix {
public:
    /**
     * @brief Physical transmission of the data and dispatch the switch event.
     *
     * @param row Row in switch matrix.
     * @param col Column in switch matrix.
     * @param switchState State of the switch identified by row and col.\n
     *                      @em 0: Switch is off.\n
     *                      @em 1: Switch is on.\n
     *                      @em 2: Switch is long on.
     */
    virtual void transmit(uint8_t &row, uint8_t &col, uint8_t switchState) override;
};


// Objekte anlegen
DispatcherClass dispatcher; ///< Create and initialise dispatcher object
EventQueueClass eventQueue; ///< Create and initialise event queue object
BufferClass inBuffer;       ///< Create and initialise buffer object
LedMatrix leds;             ///< Create and initialise the led matrix object
MySwitchMatrix switches;    ///< Create and initialise the switch matrix object

ClockDavtronM803 m803;      ///< Create and initialise the clock object
// TransponderKT76C xpdr;      ///< Create and initialise the transponder object
// XPlaneClass xPlane;         ///< Create and initialise the X-Plane object to connect to X-Plane



void MySwitchMatrix::transmit(uint8_t &row, uint8_t &col, uint8_t switchState) {
    SwitchMatrix::transmit(row, col, switchState);
    /// Write data to serial port
    // c-string with data to be sent (e.g. via the serial port)
    char charsToSend[MAX_BUFFER_LENGTH] = "S;S;";
    // add switch name
    strcat(charsToSend, dispatcher.getSwitchName(row, col));
    // set charsToSend according to value of data
    if (switchState == 2) {
        strcat(charsToSend, ";LON");
    } else {
        strcat(charsToSend, ((switchState == 1) ? ";ON;" : ";OFF;"));
    }
    Serial.println(charsToSend);

    /// Finally dispatch the switch event
    dispatcher.dispatchSwitchEvents(row, col, switchState);
}


/*********************************************************************************************************//**
 * @brief Event: Zeichen liegt an der seriellen Schnittstelle vor.
 *
 ************************************************************************************************************/
void serialEvent() {
    while (Serial.available() > 0) {
        char inChar = char(Serial.read());
        // Check for valid characters.
        // gültige Zeichen in den Buffer aufnehmen, ungültige Zeichen ignorieren
        // gültig sind: Space, '_' und alle alfanumerischen Zeichen
        if (isAlphaNumeric(inChar) || (isPunct(inChar)) || (inChar == ' ') || (inChar == '_')) {
             inBuffer.addChar(inChar);
        } else {
            if (((inChar == '\n') || (inChar == '\r')) && (! inBuffer.isEmpty())) {
                // Zeilenende erkannt und der inBuffer ist nicht leer. D.h., vorher wurde
                // kein '\r' bzw. '\n' gelesen, was dann den inBuffer geleert hätte.
                // Also den Buffer jetzt zum Parsen zum Parser senden.
                eventQueue.addEvent(inBuffer.parseString(inBuffer.get()));
                inBuffer.wipe();
                #ifdef DEBUG
                eventQueue.printQueue();
                #endif
            }
            // All other characters are ignored.
        }
    }
}


/*********************************************************************************************************//**
 * @brief Initialisierungen des Arduino durchführen.
 *
 * Hier werden die einzelnen Displays, 7-Segment-Anzeigen und LEDs angelegt und deren Position definiert
 * sowie die Hardware initialisiert und initial abgefragt bzw. gesetzt.
 *
 ************************************************************************************************************/
void setup() {
    // Serielle Schnittstelle initialisieren
    if (Serial) {
        Serial.begin(_SERIAL_BAUDRATE, SERIAL_8N1);
        // wait for serial port to connect. Needed for native USB
        while (!Serial);
        // Schreibpuffer leeren
        Serial.flush();
        // Lesepuffer leeren
        while (Serial.available() > 0) {
            Serial.read();
        }
        Serial.println(F("XPanino"));
    }

    leds.initHardware();                      ///< Arduino-Hardware der LED-Matrix initialisieren.

    const uint8_t FL = 2;
    leds.defineDisplayField(FL, 0, {0, 8});         ///< Die 1. 7-Segment-Anzeige liegt auf der Row 0 und den Cols 8 bis 15: Hunderterstelle.
    leds.defineDisplayField(FL, 1, {1, 8});         ///< Die 2. 7-Segment-Anzeige liegt auf der Row 1 und den Cols 8 bis 15: Zehnerstelle .
    leds.defineDisplayField(FL, 2, {2, 8});         ///< Die 3. 7-Segment-Anzeige liegt auf der Row 2 und den Cols 8 bis 15: Einerstelle.
    leds.display(FL, "_.nA");
    leds.set7SegBlinkOn({0, 8}, true, BLINK_NORMAL);

    const uint8_t SQUAWK = 3;
    leds.defineDisplayField(SQUAWK, 0, {3, 8});        ///< Die 1. 7-Segment-Anzeige liegt auf der Row 3 und den Cols 8 bis 15: Tausenderstelle.
    leds.defineDisplayField(SQUAWK, 1, {4, 8});        ///< Die 2. 7-Segment-Anzeige liegt auf der Row 4 und den Cols 8 bis 15: Hunderterstelle.
    leds.defineDisplayField(SQUAWK, 2, {5, 8});        ///< Die 3. 7-Segment-Anzeige liegt auf der Row 5 und den Cols 8 bis 15: Zehnerstelle.
    leds.defineDisplayField(SQUAWK, 3, {6, 8});        ///< Die 4. 7-Segment-Anzeige liegt auf der Row 6 und den Cols 8 bis 15: Einerstelle.
    leds.display(SQUAWK, "----");

    const LedMatrixPos LED_ALT{6, 4};       ///< Die LED "ALT" liegt auf Row=2 und Col=4.leds.ledOn(LED_ALT);
    leds.ledOn(LED_ALT);
    const LedMatrixPos LED_R = {7, 4};      ///< Die LED "R" liegt auf Row=3 und Col=4.
    leds.ledOn(LED_R);
    leds.ledBlinkOn(LED_R, BLINK_SLOW);

    switches.initHardware();            ///< Die Arduino-Hardware der Schaltermatrix initialisieren.
    switches.scanSwitchPins();          ///< Initiale Schalterstände abfragen und übertragen.
    switches.transmitStatus(TRANSMIT_ALL_SWITCHES);     ///< Den aktuellen ein-/aus-Status der Schalter an den PC senden.
    #ifdef DEBUG
    switches.printMatrix();
    //eventQueue.printQueue();
    #endif
}

/*********************************************************************************************************//**
 * @brief Lfd. Verarbeitung im loop().
 *
 ************************************************************************************************************/
void loop() {
    switches.scanSwitchPins();  ///< Hardware-Schalter abfragen
    switches.transmitStatus(TRANSMIT_ONLY_CHANGED_SWITCHES);    ///< Geänderte Schalterstände verarbeiten
    //XPlane.readData();  -  Daten vom X-Plane einlesen (besser als Interrupt realisieren)
    dispatcher.dispatchAll();   ///< Eventqueue abarbeiten
    m803.updateAndProcess();
    //xpdr.updateAndProcess();
    leds.writeToHardware();     ///< LEDs anzeigen bzw. refreshen
}
