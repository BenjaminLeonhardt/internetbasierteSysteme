Technische internetbasierte Systeme

Projekt Smart Home

Berkan Demir (78096), 
Benjamin Leonhardt (3018332)


Install instructions

1.) Checkout with git clone https://github.com/BenjaminLeonhardt/internetbasierteSysteme.git

2.) Install of the driver ESP32 by Espressif Systems and select the uPesy Wroom DevKit board

3.) Install all ZIP files in ArduinoIDE (Sketch -> Bibliothek einbinden -> .ZIP-Bibliothek hinzufügen...)
    (Servo.zip & pitches.zip)

4.) Install all libraries:
    ESP32Servo, ArduinoJson, Time(timelib), GxEPD2, DHT sensor library
    
5.) Setup your WiFi by entering your SSID and Password in the Project file
    Line 17 and 18

6.) Choose 115200 Baud in the Serial Monitor

7.) Compile and upload the file

8.) The adress of the ESP will be shown in the Serial Monitor