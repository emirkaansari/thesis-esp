uint8_t scanDelay = 5;              // 5 second delay between reads

// Libraries for RFID (mfrc522)
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>        // Not using anything fancy here
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <unordered_map>
#include <string>
ESP8266WiFiMulti WiFiMulti;
// Instantiate mfrc522
#define RST_PIN         D4          // Configurable, see typical pin layout above
#define SS_PIN          D8          // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

// Instantiate OLED - Adafruit Library.  Can't define SCL/SDA :(
#define OLED_RESET -1
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64              // This is presented on Adafruit boards, not on cheap eBay units.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/**
 * Initialize.
 */
void setup() {
    Serial.begin(115200);

    Serial.println();
    Serial.println("Initializing SPI...");
    SPI.begin();                   // Init SPI bus
    Serial.println("Initializing RFID...");
    mfrc522.PCD_Init();            // Init MFRC522 card
    Serial.println("Initializing OLED...");
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false);   // Adafruit...and get their logo from .cpp
    display.display();
    delay(1000);
    
    display.clearDisplay();

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println(F("Scan a MIFARE Classic PICC to demonstrate basic recognition."));
    Serial.print(F("Using key (for A and B):"));
    serial_dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
    Serial.println();
    // Connect to WiFi
    Serial.println();
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP("TP-Link_9815", "72481646");

    while (WiFiMulti.run() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

std::unordered_map<std::string, int> sessionMap;

void loop() {
    HTTPClient http;    //Declare object of class HTTPClient
    WiFiClient wifiClient;
    display.setTextColor(WHITE, BLACK);
    display.setTextSize(1);
    display.setCursor(0,0);

    display.println("Scanning...");
    display.println();
    display.display();
    
    // Look for new cards
    if ( ! mfrc522.PICC_IsNewCardPresent()) 
        return;

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;

    // Show some details of the PICC (that is: the tag/card)
    // UID is a unique four hex pairs, e.g. E6 67 2A 12 
    Serial.print(F("Card UID:"));
    serial_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    display.println("Card UID: ");
    oled_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    display.println();
    // PICC type, e.g. "MIFARE 1K0"
    Serial.print(F("PICC type: "));
    display.println(("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));
    display.println(mfrc522.PICC_GetTypeName(piccType));
    display.display();

    // Halt PICC

    Serial.print("Delay scanning...");
    delay(scanDelay * 1000);       //Turn seconds into milliseconds
    Serial.println(" ...Ready to scan again!");
    display.clearDisplay();
     // If a new card is present
     
        String cardID = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            cardID += mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ";
            cardID += String(mfrc522.uid.uidByte[i], HEX);
        }
        
        std::string cardID_std = cardID.c_str();
        int workSessionId = sessionMap[cardID_std];

        if(WiFiMulti.run()== WL_CONNECTED){   //Check WiFi connection statu
            String encodedCardID = urlEncode(cardID);
            Serial.println(encodedCardID);
            http.begin(wifiClient, "http://10.60.0.105:8080/productivity/" + encodedCardID);
            http.addHeader("Content-Type", "text/plain");
              //Specify content-type header
            http.addHeader("productivity_tracker_id", "1");  //Add the card ID to the header
            if (workSessionId != 0) {
                http.addHeader("work_session_id", String(workSessionId));
            }
            int httpCode = http.POST("");   //Send the request
            String payload = http.getString();    //Get the response payload

            Serial.println(httpCode);   //Print HTTP return code
            Serial.println(payload);    //Print request response payload
             if (payload.toInt() != 0) {
              std::string cardID_std = cardID.c_str();
              sessionMap[cardID_std] = payload.toInt();
            }
            else {
              display.clearDisplay();
              display.println(payload);
            }
            

            http.end();  //Close connection
        } else {
            Serial.println("Error in WiFi connection");
        }

        delay(3000);  //Send a request every 3 seconds
    mfrc522.PICC_HaltA();
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void serial_dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}    
void oled_dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        display.print(buffer[i] < 0x10 ? " 0" : " ");
        display.print(buffer[i], HEX);
    }
}
String urlEncode(const String &s) {
    const char *hex = "0123456789abcdef";
    String result = "";
    for (unsigned int i = 0; i < s.length(); i++) {
        if (('a' <= s[i] && s[i] <= 'z')
            || ('A' <= s[i] && s[i] <= 'Z')
            || ('0' <= s[i] && s[i] <= '9')) {
            result += s[i];
        } else {
            result += '%';
            result += hex[s[i] >> 4];
            result += hex[s[i] & 15];
        }
    }
    return result;
}