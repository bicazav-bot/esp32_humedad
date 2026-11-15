#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "RED-DIRECT-HD";
const char* password = "19752000rene2226";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

const int pinSensorSuelo = 34;
const int pinRelay = 23;

const int pinLedRojo = 25;
const int pinLedVerde = 26;
const int pinLedAzul = 27;

int umbralRiego = 50;                  
String modoOperacion = "AUTO";         

const char* topicSoil      = "esp32/riego/suelo";
const char* topicPump      = "esp32/riego/bomba";
const char* topicStatus    = "esp32/riego/status";
const char* topicUmbralOff = "esp32/riego/umbral_off";
const char* topicModo      = "esp32/riego/modo";

unsigned long ultimaLectura = 0;
const long intervaloLectura = 3000; 
int ultimoPorcentajeEnviado = -1;
String ultimoEstadoRelay = "OFF";

void actualizarLedRGB(int porcentaje) {
  digitalWrite(pinLedRojo, HIGH);
  digitalWrite(pinLedVerde, HIGH);
  digitalWrite(pinLedAzul, HIGH);

  if (porcentaje >= 0 && porcentaje <= 20) {
    digitalWrite(pinLedRojo, LOW);
  } 
  else if (porcentaje > 20 && porcentaje <= 50) {
    digitalWrite(pinLedAzul, LOW);
  } 
  else {
    digitalWrite(pinLedVerde, LOW);
  }
}

void forzarAccionRelay(String estadoFisico, String mensajeWeb) {
  ultimoEstadoRelay = estadoFisico;
  if (estadoFisico == "ON") {
    digitalWrite(pinRelay, HIGH);
  } else {
    digitalWrite(pinRelay, LOW);
  }
  client.publish(topicPump, mensajeWeb.c_str());
}

void evaluarYControlarRiego() {
  int valorBruto = analogRead(pinSensorSuelo);
  int porcentajeHumedad = map(valorBruto, 4095, 1350, 0, 100); 
  porcentajeHumedad = constrain(porcentajeHumedad, 0, 100);

  ultimoPorcentajeEnviado = porcentajeHumedad;

  actualizarLedRGB(porcentajeHumedad);

  if (modoOperacion == "AUTO") {
    if (porcentajeHumedad <= umbralRiego) {
      if (ultimoEstadoRelay != "ON") {
        forzarAccionRelay("ON", "AUTO - ENCENDIDO");
      }
    } else {
      if (ultimoEstadoRelay != "OFF") {
        forzarAccionRelay("OFF", "AUTO - APAGADO");
      }
    }
  }

  client.publish(topicSoil, String(porcentajeHumedad).c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  mensaje.trim();

  if (String(topic) == topicUmbralOff) {
    umbralRiego = mensaje.toInt();
    if (modoOperacion == "AUTO") {
      evaluarYControlarRiego(); 
    }
  }
  else if (String(topic) == topicModo) {
    if (mensaje == "AUTO") {
      modoOperacion = "AUTO";
      int valorBruto = analogRead(pinSensorSuelo);
      int porcentajeHumedad = map(valorBruto, 4095, 1350, 0, 100); 
      porcentajeHumedad = constrain(porcentajeHumedad, 0, 100);
      ultimoPorcentajeEnviado = porcentajeHumedad;

      actualizarLedRGB(porcentajeHumedad);

      if (porcentajeHumedad <= umbralRiego) {
        forzarAccionRelay("ON", "AUTO - ENCENDIDO");
      } else {
        forzarAccionRelay("OFF", "AUTO - APAGADO");
      }
      client.publish(topicSoil, String(porcentajeHumedad).c_str());
    } 
    else if (mensaje == "MANUAL_ON") {
      modoOperacion = "MANUAL"; 
      forzarAccionRelay("ON", "ENCENDIDO FORZADO");
    } 
    else if (mensaje == "MANUAL_OFF") {
      modoOperacion = "MANUAL"; 
      forzarAccionRelay("OFF", "APAGADO FORZADO");
    }
  }
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32AgroIoT-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), nullptr, nullptr, topicStatus, 1, true, "OFFLINE")) {
      client.subscribe(topicUmbralOff);
      client.subscribe(topicModo);
      
      client.publish(topicStatus, "ONLINE", true);
      client.publish(topicPump, ultimoEstadoRelay.c_str());
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, LOW);
  ultimoEstadoRelay = "OFF";

  pinMode(pinLedRojo, OUTPUT);
  pinMode(pinLedVerde, OUTPUT);
  pinMode(pinLedAzul, OUTPUT);
  
  digitalWrite(pinLedRojo, HIGH);
  digitalWrite(pinLedVerde, HIGH);
  digitalWrite(pinLedAzul, HIGH);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long ahora = millis();
  if (ahora - ultimaLectura >= intervaloLectura) {
    ultimaLectura = ahora;
    evaluarYControlarRiego();
  }
}