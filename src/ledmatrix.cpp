/**************************************************************************************************
 * @file ledmatrix.cpp
 * @author Christian Harraeus <christian@harraeus.de>
 * @brief Implementierung der Klasse @em LedMatrix.
 * @version 0.1
 * @date 2017-11-10
 * 
 * Copyright © 2017 - 2020. All rights reserved.
 * 
 **************************************************************************************************/

#include "ledmatrix.hpp"
#include <Arduino.h>

/** Konstanten für die Zuordnung der Arduino-Pins zu den MIC5891- und MIC5821-Schieberegister-Leitungen */
const uint8_t CLOCK = PIN4;     ///< Arduino-Pin für CLOCK des MIC5891/5821
const uint8_t DATA_IN = PIN5;   ///< Arduino-Pin für DATA_IN des MIC5891/5821
const uint8_t STRB = PIN3;      ///< Arduino-Pin für STRB des MIC5891/5821
const uint8_t OE = PIN2;        ///< Arduino-Pin für OE des MIC5891/5821

/** Konstanten für's Blinken */
const unsigned int BLINK_VERSATZ = 447;     ///< Versatz für die Startzeiten der Blinkgeschwindigkeiten. Damit
                                            ///< nicht alles so gleich im Takt blinkt

/** Konstanten für die Dauern der Hell- und Dunkelphasen beim Blinken */
const unsigned long int BLINK_INTERVAL_BRIGHT[NO_OF_SPEED_CLASSES] = {
    1000,   ///< Normales Blinken: Dauer der Hellphase in Millisekunden
    2000    ///< Langsames Blinken: Dauer der Hellphase in Millisekunden
};
const unsigned long int BLINK_INTERVAL_DARK[NO_OF_SPEED_CLASSES] = {
    1000,   ///< Normales Blinken: Dauer der Dunkelphase in Millisekunden
    6000    ///< Langsames Blinken: Dauer der Dunkelphase in Millisekunden
};

/** Konstanten für die Bitmuster für die Anzeige der Werte auf den 7-Segment-Anzeigen
* segmentBits bildet ein 7-Segment-Display mit Dezimalpunkt ab.
*
*       a
*    -------
*    |     |
*  f |     | b
*    |  g  |
*    -------
*    |     |
*  e |     | c
*    |     |
*    -------
*       d
*
* segmentBits enthält das Bitmuster, die die Ziffer/den Buchstaben darstellen.
* a wird dabei durch das niederstwertige Bit repräsentiert, g durch das höchstwertige Bit.
 */
const uint32_t SEGMENT_CHARS = 29;              ///< Anzahl verfügbare Zeichen
const uint8_t segmentBits[SEGMENT_CHARS] = {    ///< segmentBits enthält das Bitmuster, die die Ziffer/den Buchstaben darstellen.
    //gfedcba
    0b0111111, ///<  "0": Segmente f, e, d, c, b, a    --> 0x3f   --> segmentBits[0]
    0b0000110, ///<  "1": Segmente c, b                --> 0x06   --> segmentBits[1]
    0b1011011, ///<  "2": Segmente g, e, d, b, a       --> 0x5b   --> segmentBits[2]
    0b1001111, ///<  "3": Segmente g, d, c, b, a       --> 0x4f   --> segmentBits[3]
    0b1100110, ///<  "4": Segmente g, f, c, b          --> 0x66   --> segmentBits[4]
    0b1101101, ///<  "5": Segmente g, f, d, c, a       --> 0x6d   --> segmentBits[5]
    0b1111101, ///<  "6": Segmente g, f, e, d, c, a    --> 0x7d   --> segmentBits[6]
    0b0000111, ///<  "7": Segmente c, b, a             --> 0x07   --> segmentBits[7]
    0b1111111, ///<  "8": Segmente g, f, e, d, c, b, a --> 0x7f   --> segmentBits[8]
    0b1101111, ///<  "9": Segmente g, f, d, c, b, a    --> 0x6f   --> segmentBits[9]
    0b0000000, ///<  " ": alle Segmente aus            --> 0x00   --> segmentBits[10]
    0b1001001, ///< Drei waagerechte Striche: Segmente g, d, a  --> 0x49 --> segementBits[11]
    0b0110110, ///< Zwei senkrechte Striche:  Segmente f, e, c, b --> segmentBits[12]
    0b1110111, ///<  "A": Segmente g, f, e, b, c, a .  -->        --> segmentBits[13]
    0b1111100, ///<  "b": Segmente g, f, e, d, c                  --> segmentBits[14]
    0b0111001, ///<  "C": Segmente f, e, d, a                     --> segmentBits[15]
    0b1011110, ///<  "d": Segmente g, e, d, c, b                  --> segmentBits[16]
    0b1111001, ///<  "E": Segmente g, f, e, d, a       --> 0x79   --> segmentBits[17]
    0b1110001, ///<  "F": Segmente g, f, e, a                     --> segmentBits[18]
    0b0110110, ///<  "H": Segmente g, f, e, c, b                  --> segmentBits[19]
    0b0111000, ///<  "L": Segmente f, e, d                        --> segmentBits[20]
    0b1011100, ///<  "o": Segmente g, e, d, c                     --> segmentBits[21]
    0b1110011, ///<  "P": Segmente g, f, e, b, a                  --> segmentBits[22]
    0b1010000, ///<  "r": Segmente g, e                --> 0x50   --> segmentBits[23]
    0b0111110, ///<  "U": Segmente f, e, d, c, b                  --> segmentBits[24]
    0b0011100, ///<  "u": Segmente e, d, c                        --> segmentBits[25]
    0b1000000, ///<  "-": Segment g                    --> 0x40   --> segmentBits[26]
    0b1100011, ///<  "°": Segment g, f, b, a                      --> segmentBits[27]
    0b1011000  ///<  "c": Segment g, e, d, c                      --> segmentBits[28]
};


/**************************************************************************************************/
LedMatrix::LedMatrix() {
    /// Die Matrizen initalisieren
    for (uint32_t row = 0; row != LED_ROWS; ++row) {
        hwMatrix[row] = 0;                  // Alle LEDs ausschalten
        matrix[row] = 0;                    // Alle LEDs sind ausgeschaltet
    }
    /// Defaultmäßig das Blinken deaktivieren
    for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
        for (uint8_t row = 0; row != LED_ROWS; ++row) {
            blinkStatus[speedClass][row] = 0;         // Keine LED blinkt
        }
        isBlinkDarkPhase[speedClass] = true;
        blinkStartTime[speedClass] = millis() + speedClass * BLINK_VERSATZ;   // Startzeit mit Versatz
    }
    /// Defaultwerte für die Blinkdauern der nächsten anstehenden Hellphase
    nextBlinkInterval[BLINK_NORMAL] = BLINK_INTERVAL_BRIGHT[BLINK_NORMAL];    // Dauer der Hellphase als Initialwert
    nextBlinkInterval[BLINK_SLOW] = BLINK_INTERVAL_BRIGHT[BLINK_SLOW];


    // - Auf den 7 Digits des XPDR die Ziffernfolge 0..6 anzeigen
    // - Die Dezimalpunkte im Digit 2 und 6 anzeigen
    // - Die LED R anzeigen
    // - Auf den oberen 4 Digits der Uhr folgende Zeichen anzeigen:
    //   -- Drei waagerechte Striche
    //   -- Minus
    //   -- E und Dezimalpunkt
    //   -- r
    // - Auf den unteren 4 Digits der Uhr folgende Zeichen anzeigen:
    //   -- kleines o und Dezimalpunkt
    //   -- F
    //   -- C
    //   -- Zwei senkrechte Striche
    // - Die Minutentrenner-LEDs der Uhr anzeigen
    // - Die LED UT anzeigen
    // vgl. Doku "Belegung der LED-Matrix des 1. IO-Warrior" in Evernote
    //            most sig. Byte   least sig. Byte
    //            7      07      07      07      0
//    matrix[0] = 0b00000000010010010011111100010000;
//    matrix[1] = 0b00000000010000000000011000010000;
//    matrix[2] = 0b00000000111110011101101100000000;
//    matrix[3] = 0b00000000010100000100111100010000;
//    matrix[4] = 0b00000000110111001110011000000000;
//    matrix[5] = 0b00000000011000110110110100000000;
//    matrix[6] = 0b00000000001110010111110100000000;
//    matrix[7] = 0b00000000001101100000000000010000;  
}


/**************************************************************************************************/
void LedMatrix::initHardware() {
    /// Die eingebaut LED als Status aktivieren.
    pinMode(LED_BUILTIN, OUTPUT);
    for (unsigned int i = 1; i != 4; ++i) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }
    
    /// Die benötigten Pins für die Ansteuerung der Schieberegister initialisieren.
    pinMode(CLOCK, OUTPUT);
    pinMode(DATA_IN, OUTPUT);
    pinMode(STRB, OUTPUT);
    pinMode(OE, OUTPUT);
    delay(1);
    digitalWrite(CLOCK, LOW);
    digitalWrite(DATA_IN, LOW);
    digitalWrite(STRB, LOW);
    delayMicroseconds(500);
    digitalWrite(STRB, HIGH);       // Latches umgehen --> immer auf HIGH setzen
    digitalWrite(OE, LOW);
    delayMicroseconds(500);
}
    

/**************************************************************************************************/
uint8_t LedMatrix::get7SegBits(const unsigned char character) {
    if (character >= SEGMENT_CHARS) {
        // character muss eines der @em CHAR_x Konstanten sein.
        /// Prüfen, ob entweder eine Ziffer oder eine Ziffer als Char übergeben wurde.
        /// Bei Char wird der ASCII-Code übergeben, ASCII '0' = 48, ASCII '9' = 57;
        /// Fehler: Index referenziert keinen Eintrag aus unserer Zeichentabelle.
        if ((character >= 48) && (character <= 57)) {
            // Ziffer als ASCII-Code erhalten; diesen in eine Ziffer umrechnen, 
            // ASCII-Code 48 = '0'; diese abziehen um den Wert zu erhalten.
            return segmentBits[character - 48];
        } else {
            return segmentBits[CHAR_ERROR];
        }
    } else {
        return segmentBits[character];
    }
}


/**************************************************************************************************/
bool LedMatrix::isValidRowCol(const uint8_t row, const uint8_t col) {
    /// Zulässige Werte row: [0..LED_ROWS - 1], 
    /// Zulässige Werte col: [0 ..LED_COLS - 1].
    return ((row < LED_ROWS) && (col < LED_COLS));
};


/**************************************************************************************************/
bool LedMatrix::isValidBlinkSpeed(const uint8_t blinkSpeed) {
    return (blinkSpeed < NO_OF_SPEED_CLASSES);
}


/**************************************************************************************************/
void LedMatrix::doBlink() {
    /// checken, ob überhaupt (normal) geblinkt werden muss: 
    /// Wenn alle Werte in blinkStatus Null sind, ist für alle LEDs das Blinken ausgeschaltet. 
    /// Sollte für mindestens eine LED das Blinken eingeschaltet sein, wird blinkOn auf true gesetzt.
    bool blinkOn = false;
    for (auto &bs : blinkStatus) {
        for (auto &col : bs) {
            blinkOn = (col != 0);
            if (blinkOn) {
                break;
            }
        }
        if (blinkOn) {
            break;
        }       
    }
    
    /// Wenn was zu blinken ist und das Blinkintervall abgelaufen ist, die Blinkphase umschalten.  
    /// isBlinkDarkPhaseXXX wurde mit Anfangs mit false initialisiert, d.h. das Blinken startet immer
    /// mit einer Hellphase.
    if (blinkOn) {
        /// Check auf Zeitablauf, so dass die Hell- und Dunkelphasen umgeschaltet werden müssen
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (millis() - blinkStartTime[speedClass] > nextBlinkInterval[speedClass]) {
                if (isBlinkDarkPhase[speedClass]) {
                    nextBlinkInterval[speedClass] = BLINK_INTERVAL_BRIGHT[speedClass];
                } else {
                    nextBlinkInterval[speedClass] = BLINK_INTERVAL_DARK[speedClass];
                } 
                isBlinkDarkPhase[speedClass] = ! isBlinkDarkPhase[speedClass];
                blinkStartTime[speedClass] = millis(); // + speedClass * BLINK_VERSATZ;
            }
        }
    }
    
    /// Die matrix in die hwMatrix kopieren, die die LEDs steuert. Während der Dunkelphase müssen die entsprechenden
    /// blinkenden LEDs ausgeschaltet werden.
    for (uint32_t row = 0; row != LED_ROWS; ++row) {
        hwMatrix[row] = matrix[row];
        /// Wenn was zu blinken ist, die hwMatrix entsprechen korrigieren
        if (blinkOn) {
            for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
                if (isBlinkDarkPhase[speedClass]) {
                    hwMatrix[row] &= ~ blinkStatus[speedClass][row];      // LEDs, die normal blinken sollen, dunkel schalten
                }
            }
        }
    }
}


/*************************************************************************************************
 * @note Diese Funktion muss regelmäßig und sehr häufig innerhalb des loop
 * aufgerufen werden!
 * 
 * Falls Blinken eingeschaltet ist, zuerst den gewünschten LED-Status 
 * (d.h. an- oder ausgeschaltet) noch gemäß der aktuellen Blinkphase 
 * (hell oder dunkel) anpassen; das wird durch Aufruf von doBlink()
 * erledigt.
 * 
 * Je Row werden alle 32 Bits (je Column ein Bit) durch die Schieberegister
 * geschossen und anschließend das entsprechende Row-Bit. Beim 
 * ersten Schleifendurchgang werden also 32 Bits angezeigt. Beim 
 * nächsten Schleifendurchgang die nächsten 32 Bits usw. Durch 
 * die hohe Geschwindigkeit wird eine statische Anzeige erzielt.
 * Wenn zu viele andere Aktivitäten zwischen den display()-Aufrufen
 * stattfinden, wird die Anzeige mehr oder weniger stark flimmern.
 * 
*/
void LedMatrix::display() {
    // Alle Berechnungen zum Blinken erledigen
    doBlink();
    // Die hwMatrix serialisieren und in die Schieberegister schieben und die Outputs scharf schalten
    for (uint8_t row = 0; row != LED_ROWS; ++row) {
        digitalWrite(STRB, LOW);    // STROBE unbedingt auf LOW setzen damit die Registerinhalte in die Latches übernommen werden
        
        // die 32 Column-Bits der jeweiligen Row durch/in die Schieberegister schieben
        for (uint32_t bit = sizeof(hwMatrix[row]) * 8; bit != 0; --bit) {
            digitalWrite(DATA_IN, (hwMatrix[row] >> (bit - 1)) & 1);     //
            digitalWrite(CLOCK, HIGH);                  // DATA_IN in Shift-Register übernehmen
            delayMicroseconds(1);
            digitalWrite(CLOCK, LOW);
        }
        
        // nachdem alle Column-Bits übertragen sind, muss noch das zugehörige Row-Bit übertragen werden.
        // Da das MSB zuerst übertragen werden muss, muss hier statt "row" der Ausdruck "7 - row"
        // verwendet werden.
        uint8_t activeRow = 1 << row;
        for (uint8_t bit = sizeof(activeRow) * 8; bit != 0; --bit) {
            digitalWrite(DATA_IN, (activeRow >> (bit - 1)) & 1);
            digitalWrite(CLOCK, HIGH);                  // DATA_IN in Shift-Register übernehmen
            delayMicroseconds(1);
            digitalWrite(CLOCK, LOW);
            delayMicroseconds(1);
        }
        
        digitalWrite(STRB, HIGH);   // STROBE wieder auf HIGH setzen die Latch-Inhalte auf die Outputs geschaltet werden
        //delayMicroseconds(1);
    }
}


/**************************************************************************************************/
bool LedMatrix::isLedOn(const uint8_t row, const uint8_t col) {
    if (isValidRowCol(row, col)) {
        return (matrix[row] & (static_cast<uint32_t>(1) << col)) != 0;
    } else {
        return false;      // unzulässige Row oder Col; muss zwischen 0 und LED_ROWS - 1 bzw. LED_COLS - 1 sein
    }    
}


/**************************************************************************************************/
int LedMatrix::ledOn(const uint8_t row, const uint8_t col) {
    if (isValidRowCol(row, col)) {
        matrix[row] |= static_cast<uint32_t>(1) << col;    // An der Stelle col soll das Bit gesetzt werden
        return 0;
    } else {
        return -1;      // unzulässige Row oder Col
    }
}


/**************************************************************************************************/
int LedMatrix::ledOff(const uint8_t row, const uint8_t col) {
    if (isValidRowCol(row, col)) {
        matrix[row] &= ~ (static_cast<uint32_t>(1) << col);
        return 0;
    } else {
        return -1;      // unzulässige Row oder Col
    }
}


/**************************************************************************************************/
int LedMatrix::ledToggle(const uint8_t row, const uint8_t col) {
    if ((row >= LED_ROWS) || (col >= LED_COLS)) {
        return -1;      // unzulässige Row oder Col
    } else {
        if (isLedOn(row, col)) {
            ledOff(row, col);
        } else {
            ledOn(row, col);
        }
        return 0;
    }
}


/**************************************************************************************************/
int LedMatrix::ledBlinkOn(const uint8_t row, const uint8_t col, const uint8_t blinkSpeed) {
    if (isValidRowCol(row, col) && isValidBlinkSpeed(blinkSpeed)) {
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (blinkSpeed == speedClass) {
                blinkStatus[speedClass][row] |= static_cast<uint32_t>(1) << col;     // An der Stelle col soll das Bit gesetzt werden
                break;
             }
        }
        return 0;        
    } else {
        return -1;
    }
}


/**************************************************************************************************/
int LedMatrix::ledBlinkOff(const uint8_t row, const uint8_t col, const uint8_t blinkSpeed) {
    if (isValidRowCol(row, col) && isValidBlinkSpeed(blinkSpeed)) {
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (blinkSpeed == speedClass) {
                blinkStatus[speedClass][row] &= ~ (static_cast<uint32_t>(1) << col);
                break;
            }
        }
        return 0;
    } else {
        return -1;
    }    
}


/**************************************************************************************************/
int LedMatrix::isLedBlinkOn(const uint8_t row, const uint8_t col, const uint8_t blinkSpeed) {
    int returnvalue = 0;

    if (isValidRowCol(row, col) && isValidBlinkSpeed(blinkSpeed)) {
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (blinkSpeed == speedClass) {
                returnvalue = ((blinkStatus[speedClass][row] & (static_cast<uint32_t>(1) << col)) != 0);
            }
        }
    } else {
        returnvalue = -1;      // unzulässige Row oder Col; muss zwischen 0 und LED_ROWS - 1 bzw. LED_COLS - 1 sein
    }        
    return returnvalue;
}


/**************************************************************************************************/
int LedMatrix::set7SegValue(const uint8_t row, const uint8_t col0, const unsigned char newValue, const bool dpOn) {
    // prüfen, ob insbes. alle 8 cols, die für ein 7-Segement-Display benötigt werden, innerhalb
    // des gültigen Berichs liegen
    if (isValidRowCol(row, col0) && isValidRowCol(row, col0 + 7)) {
        // row und col0 passen
        matrix[row] &= ~ (static_cast<uint32_t>(0b11111111) << col0);  // zuerst alle Bits des 7-Segment-Displays löschen
        matrix[row] |= static_cast<uint32_t>(get7SegBits(newValue)) << col0;  
        if (dpOn) {
            ledOn(row, col0 + 7);   // der Dezimalpunkt ist immer das höchstwertigste Bit im Zeichenbyte
        } else {
            ledOff(row, col0 + 7);
        }
        return 0;
    } else {
        // unzulässige Row oder Col
        return -1;      
    }
}


/**************************************************************************************************/
int LedMatrix:: set7SegBlinkOn(const uint8_t row, const uint8_t col0, const bool dpBlink, 
                               const uint8_t blinkSpeed) {
    // Blinken der 7-Segment-Anzeige und ggf. auch des Dezimalpunkts einschalten
    // Dabei nach Blinkgeschwindigkeiten unterscheiden
    if (isValidRowCol(row, col0) && isValidRowCol(row, col0 + 7) && isValidBlinkSpeed(blinkSpeed)) {
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (blinkSpeed == speedClass) {
                if (dpBlink) {
                    // alle Bits inkl. Dezimalpunkt des 7-Segment-Displays zum Blinken einschalten
                    blinkStatus[speedClass][row] |= static_cast<uint32_t>(0b11111111) << col0;  
                } else {
                    // alle Bits, aber ohne Dezimalpunkt, des 7-Segment-Displays zum Blinken einschalten
                    blinkStatus[speedClass][row] |= static_cast<uint32_t>(0b01111111) << col0;  // alle Bits, aber ohne Dezimalpunkt, des 7-Segment-Displays zum Blinken markieren
                }
                break;
            }
        }
        return 0;
    } else {
        // unzulässige Row oder Col
        return -1;      
    }    
}


/**************************************************************************************************/
int LedMatrix:: set7SegBlinkOff(const uint8_t row, const uint8_t col0, const bool dpBlink, 
                               const uint8_t blinkSpeed) {
    // Blinken der 7-Segment-Anzeige und ggf. auch des Dezimalpunkts ausschalten
    // Dabei nach Blinkgeschwindigkeiten unterscheiden
    if (isValidRowCol(row, col0) && isValidRowCol(row, col0 + 7) && isValidBlinkSpeed(blinkSpeed)) {
        for (uint8_t speedClass = 0; speedClass != NO_OF_SPEED_CLASSES; ++speedClass) {
            if (blinkSpeed == speedClass) {
                if (dpBlink) {
                    // das Blinker aller Bits inkl. Dezimalpunkt des 7-Segment-Displays ausschalten
                    blinkStatus[speedClass][row] &= ~ (static_cast<uint32_t>(0b11111111) << col0);  
                } else {
                    // das Blinken aller Bits, aber ohne Dezimalpunkt, des 7-Segment-Displays einschalten
                    blinkStatus[speedClass][row] &= ~ (static_cast<uint32_t>(0b01111111) << col0);
                }
                break;
            }
        }
        return 0;
    } else {
        // unzulässige Row oder Col
        return -1;      
    }
}


/**************************************************************************************************/
void LedMatrix::powerOnSelfTest() {
    for (uint8_t row = 1; row != 7; ++row) {
        set7SegValue(row, 8, row - 1);      // FL 123 und XPDR-Code 2345
    }
    set7SegValue(0, 8, CHAR_MINUS);
    for (uint8_t row = 4; row != 8; ++row) {     // alte Form: for (uint8_t row = 4; row != 8; ++row, DP_ON)
        set7SegValue(row, 16, row - 3);     // 12:34 Uhr 
    }
    ledOn(7, 4);        // R-LED an
    ledBlinkOn(7, 4, BLINK_SLOW);   // und diese langsam blinken lassen
    ledOn(0, 4);        // Stunden-Minuten-Trenner 
    ledOn(1, 4);        // Stunden-Minuten-Trenner
    ledBlinkOn(0, 4);   // Beide Stunden-Minuten-Trenner sollen normal-schnell blinken
    ledBlinkOn(1, 4);
    set7SegValue(0, 16, 2);     // 27°C 
    set7SegValue(1, 16, 7);
    set7SegValue(2, 16, CHAR_DEGREE);
    set7SegValue(3, 16, CHAR_C);
    ledOn(3, 4);        // UT-LED an
    
}


#ifndef NDEBUG_LEDS
/**************************************************************************************************/
void LedMatrix::printMatrix() {
    Serial.println(); Serial.println("Die Bytes der Matrix:");
    uint8_t row = 0;
    for (uint32_t col : hwMatrix) {   // col enthält jetzt alle 32 Bits der Column; wird für alle Rows durchlaufen
        Serial.print("Row "); Serial.print(row); Serial.print(":  ");
        Serial.print(col); Serial.print(" --> ");
        uint8_t col1 = col >> 24;
        Serial.print(col1); Serial.print(" ");
        col1 = col >> 16;
        Serial.print(col1); Serial.print(" ");
        col1 = col >> 8;
        Serial.print(col1); Serial.print(" ");
        col1 = col;
        Serial.print(col1); Serial.println(" ");
        ++row;
    }
    Serial.println();
    Serial.print("Matrixspaltensize="); Serial.println(sizeof(hwMatrix[0]) * 8);
}

#endif

