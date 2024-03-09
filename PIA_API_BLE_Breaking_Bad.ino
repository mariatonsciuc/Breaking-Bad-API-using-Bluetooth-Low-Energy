#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String TeamID;
//numele serverului ble
#define bleServerName "ESP32_BLE_A26"

//definim o variabila pt statusul conexiunii
bool connected = false;

DynamicJsonDocument JsonDoc(4096);
char TEAM_ID[20] = "A26";

#define SERVICE_UUID "9775e9d3-2ab4-4cdf-a892-3c711558aaa2"
#define CHARACTERISTIC_UUID "ce403472-ad95-4810-af3c-37fc662f2941"

//definim o caracteristica cu proprietatile ble
BLECharacteristic characteristic(
 CHARACTERISTIC_UUID,
 BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
);
//definim o caracteristica descriptiva 
BLEDescriptor *characteristicDescriptor = new
BLEDescriptor(BLEUUID((uint16_t)0x2902));

// ajuta sa monitorizam statusul serverului BLE, cand se conecteaza afiseaza acest lucru
class MyServerCallbacks: public BLEServerCallbacks {
 void onConnect(BLEServer* pServer) {
 connected = true;
 Serial.println("Device connected");
 };

//similar
 void onDisconnect(BLEServer* pServer) {
 connected = false;
 Serial.println("Device disconnected");
 }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
//apelam functia onWrite de fiecare data cand caracteristica BLE primeste o valoare noua
  void onWrite(BLECharacteristic *pCharacteristic) {
    //se obtine valoarea caracteristicii sub forma unui sir de caractere si il afisam
    std::string tmpCharacteristic = pCharacteristic->getValue();
    Serial.println(tmpCharacteristic.c_str());

    //deserializam sirul JSON obtinut in documentul JSON dinamic  
    deserializeJson(JsonDoc, tmpCharacteristic.c_str());
    //extragem valoarea campului "action" 
    std::string jsonaction = JsonDoc["action"];

    //daca se solicita informatii despre retelele Wi-Fi, stocam id-ul echipei in teamID
    if (jsonaction.compare("getNetworks") == 0) {
      std::string teamID = JsonDoc["teamId"];

      //scanam retelele disponibile si le stocam in variabila n 
      int n = WiFi.scanNetworks();

      for (int i = 0; i < n; ++i) {
       //stocam informatiile fiecarei retele
        DynamicJsonDocument networkDoc(4096);
        JsonObject jsonnetwork = networkDoc.to<JsonObject>();
      //adaugam informatiile retelei
        jsonnetwork["ssid"] = WiFi.SSID(i);
        jsonnetwork["strength"] = WiFi.RSSI(i);
        jsonnetwork["encryption"] = WiFi.encryptionType(i);
        jsonnetwork["teamId"] = teamID;

        TeamID = String(teamID.c_str());
        std::string jsonData;
        //serializam documentul networkDoc
        serializeJson(networkDoc, jsonData);
        serializeJson(networkDoc, Serial);
        //setam valoarea caracteristicii
        pCharacteristic->setValue(jsonData);
        //se trimite o notificare catre dispozitiv a.i. sa primeasca datele retelei prin BLE
        pCharacteristic->notify();
      
      }
    } //daca actiunea din campul action este connect
    else if (jsonaction.compare("connect") == 0) {
      //atunci extragem valorile
      std::string jsonssid = JsonDoc["ssid"];
      std::string jsonpassword = JsonDoc["password"];
      //dispozitivul începe procesul de conectare la rețeaua WiFi specificată
      WiFi.begin(jsonssid.c_str(), jsonpassword.c_str());
      //adaugam intarziere de 5 sec pt a permite dispozitivului sa se conecteze 
      delay(5000);

      //verificam daca dispozitivul e deja conectat la wifi
      if (WiFi.status() == WL_CONNECTED) {
        //cream un nou obiect connectDoc de tip json 
        DynamicJsonDocument connectDoc(4096);
        JsonObject jsonconnect = connectDoc.to<JsonObject>();

        jsonconnect["ssid"] = WiFi.SSID();
        jsonconnect["connected"] = true;
        jsonconnect["teamId"] = TeamID;

        std::string jsonData;
        serializeJson(connectDoc, jsonData);
        serializeJson(connectDoc, Serial);
        pCharacteristic->setValue(jsonData);
        pCharacteristic->notify();
      } else {
        Serial.println("Connection failed");
      }
    } else if (jsonaction.compare("getData") == 0) {
      const char* URL = "http://proiectia.bogdanflorea.ro/api/breaking-bad/characters";

      HTTPClient http;

      http.begin(URL); 

      int httpResponseCode = http.GET(); //realizeaza efectiv cererea GET HTTP catre URL-ul specificat

      if (httpResponseCode == HTTP_CODE_OK) { 
        String jsonResponse = http.getString();
        Serial.println(jsonResponse);

        DynamicJsonDocument getDataDoc(4096);
        deserializeJson(getDataDoc, jsonResponse);

        JsonArray records = getDataDoc.as<JsonArray>();

        for (JsonObject record : records) {
          DynamicJsonDocument recordDoc(4096);
          JsonObject jsonRecord = recordDoc.to<JsonObject>(); // se realizeaza o conversie a obiectului recordDoc intr-un obiect JsonObject

          jsonRecord["id"] = record["char_id"];
          jsonRecord["name"] = record["name"];
          jsonRecord["image"] = record["img"]; 
          jsonRecord["teamId"] = TeamID;
        

          std::string jsonData;
          serializeJson(recordDoc, jsonData);
          serializeJson(recordDoc, Serial);
          pCharacteristic->setValue(jsonData);
          pCharacteristic->notify();
        }
      } else {
        Serial.print("HTTP request failed with error code: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else if (jsonaction.compare("getDetails") == 0) { //daca actiunea este getDetails
  std::string jsonid = JsonDoc["id"]; //extragem valoarea id din json
  std::string URL = "http://proiectia.bogdanflorea.ro/api/breaking-bad/character?char_id=" + jsonid; //construim URL-ul doar pt id-ul specificat

  HTTPClient http;

  http.begin(URL.c_str());

  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String jsonResponse = http.getString();
    Serial.println(jsonResponse);

    DynamicJsonDocument detailsDoc(4096);
    deserializeJson(detailsDoc, jsonResponse);

    JsonObject record = detailsDoc.as<JsonObject>(); 
    // creeaza obiectul response JSON
    DynamicJsonDocument responseDoc(4096);
    JsonObject response = responseDoc.to<JsonObject>();

   response["id"] = record["char_id"];
          response["name"] = record["name"];
          response["image"] = record["img"]; 
          response["teamId"] = TeamID;
        //construim "description attribute"
          String description = "Character: " + String(record["name"].as<String>()) + "\n";
          description += "Birthday: " + String(record["birthday"].as<String>()) + "\n";
          description += "Occupation: " + String(record["occupation"].as<String>()) + "\n";
          description += "Status: " + String(record["status"].as<String>()) + "\n";
          description += "Nickname: " + String(record["nickname"].as<String>()) + "\n";
          description += "Appearance: " + String(record["appearance"].as<String>()) + "\n";
          description += "Portrayed: " + String(record["portrayed"].as<String>()) + "\n";


          response["description"] = description;

    //response["teamId"] = TeamID;

    std::string jsonData;
    serializeJson(response, jsonData);
    serializeJson(response, Serial);
    pCharacteristic->setValue(jsonData);
    pCharacteristic->notify();
  } else {
    Serial.print("HTTP request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

  }
};

void setup() {
  Serial.begin(115200); //permite transmiterea de date între dispozitivul Arduino și computer prin portul serial
  BLEDevice::init(bleServerName); // initializează biblioteca BLE cu numele serverului specificat (bleServerName)
  BLEServer *pServer = BLEDevice::createServer(); //Creeaza un server BLE pe dispozitiv
  pServer->setCallbacks(new MyServerCallbacks()); //Seteaza functiile de callback ale serverului BLE pentru gestionarea evenimentelor de conectare și deconectare ale clientului

  characteristic.setCallbacks(new MyCharacteristicCallbacks()); 
  BLEService *bleService = pServer->createService(SERVICE_UUID);

  bleService->addCharacteristic(&characteristic);//Adauga caracteristica BLE la serviciu.
  characteristic.addDescriptor(characteristicDescriptor);//Adaugă descriptorul caracteristicii BLE.

  bleService->start();//Initializează serviciul BLE.

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();//Obtine un obiect de reclama BLE.
  pAdvertising->addServiceUUID(SERVICE_UUID);//Adauga UUID-ul serviciului la reclama
  pServer->getAdvertising()->start();

  Serial.println("Waiting for a client connection to notify...");//Afiseaza mesajul

  WiFi.mode(WIFI_STA);//Configureaza modulul WiFi în modul stationar (client) pentru a se conecta la o retea WiFi.
  WiFi.disconnect(); //Deconectează WiFi de la reteaua curenta.
  delay(3000);//Asteaptă 3 secunde pentru a permite suficient timp pentru deconectarea WiFi 
}

void loop() {
 
}
