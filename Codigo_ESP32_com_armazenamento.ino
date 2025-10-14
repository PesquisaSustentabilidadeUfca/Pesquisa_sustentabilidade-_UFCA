#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include "esp_eap_client.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include "time.h"

//CONFIGURAÇÃO WIFI

const char* ssid = "***"; //adicionar informações da rede.
const char* password = "***";

/*const char* ssid = "****"; //adicionar informações da rede eduroam.
#define EAP_IDENTITY "eduroam"
#define EAP_PASSWORD "********"
#define EAP_USERNAME "***"*/

//PINOS DO SENSOR
const int trigPin = 5;
const int echoPin = 18;

//CONFIGS DE BUFFER
#define MAX_PENDENTES 17000
float pendentes[MAX_PENDENTES];
int index_pendentes = 0;
time_t tempo_desconexao = 0;

//VARIÁVEIS DE MEDIÇÃO
float duration, distance;
char data[20], hora[20];
bool tdata = true;

char *GScriptId = "AKfycbxwnnYTfbyqYn6ZN4zQthXreZQBWyoYNpJwo0xMzcHAYuEY-k2cEKwJjkJM5-2ZkeuP";

WebServer server(80);  // Porta HTTP padrão
String mensagemHtml = "Aguardando leitura.";
String mensagemHtml1 = "Aguardando leitura..";
String mensagemHtml2 = "Aguardando leitura...";
String mensagemHtml3 = "Aguardando leitura....";
String mensagemHtml4 = "Aguardando leitura.....";

bool verificarConexaoInternet() {
  HTTPClient http;
  http.begin("http://google.com");
  int httpCode = http.GET();
  http.end();

  // Se código for maior que 0 e <400, a conexão está ok
  return (httpCode > 0 && httpCode < 400);
}

void atua_web(){
  mensagemHtml4 = mensagemHtml3;
  mensagemHtml3 = mensagemHtml2;
  mensagemHtml2 = mensagemHtml1;
  mensagemHtml1 = mensagemHtml; 
}

void handleRoot(){
  String html = R"rawliteral(
    <head>
      <style>
        body {
          background-color: black;
          color: white;
          font-family: Arial, sans-serif;
          margin-top: 50px;
        }
        h1 {
          font-size: 2em;
        }
      </style>
    </head>
    <meta http-equiv='refresh' content='1'>
    <body>
  )rawliteral";

  html += "<p> " + mensagemHtml + " </p>";
  html += "<p> " + mensagemHtml1 + " </p>";
  html += "<p> " + mensagemHtml2 + " </p>";
  html += "<p> " + mensagemHtml3 + " </p>";
  html += "<p> " + mensagemHtml4 + " </p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

//FUNÇÃO PARA ENVIAR MEDIÇÃO
bool enviarmedicao(float d, const char* data_str, const char* hora_str){
  String url = "https://script.google.com/macros/s/" + String(GScriptId) +
               "/exec?value1=" + String(d, 2) +
               "&data=" + String(data_str) +
               "&hora=" + String(hora_str);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if(httpCode > 0){
    Serial.println("Dados enviados: " + String(d, 2) + " em " + String(data_str) + " às " + String(hora_str));
    atua_web();
    mensagemHtml = ("Dados enviados: " + String(d, 2) + " em " + String(data_str) + " as " + String(hora_str));
    http.end();
    return true;
  }else{
    Serial.println("Erro HTTP: " + String(httpCode));
    atua_web();
    mensagemHtml = ("Erro HTTP: " + String(httpCode));
    http.end();
    tdata = false;
    return false;
  }
}

//SETUP
void setup(){
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(2, OUTPUT);
  Serial.begin(9600);

  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME,EAP_PASSWORD);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while(WiFi.status() != WL_CONNECTED){
    delay(500); 
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  atua_web();
  mensagemHtml = ("\nWiFi conectado!");
  configTime(-3 * 3600, 0, "time.nist.gov", "a.st1.ntp.br", "pool.ntp.org");
  // Inicializa OTA
  ArduinoOTA
    .onStart([](){
      Serial.println("Iniciando OTA");
      atua_web();
      mensagemHtml = ("Iniciando OTA");
    })
    .onEnd([](){
      Serial.println("\nOTA concluída!");
      atua_web();
      mensagemHtml = ("\nOTA concluída!");
    })
    .onError([](ota_error_t error){
      Serial.printf("Erro OTA [%u]: ", error);
    });

  if (!MDNS.begin("esp32")) {
  Serial.println("Erro ao iniciar mDNS");
  return;
  }

  ArduinoOTA.begin();
  Serial.println("Pronto para OTA");
  atua_web();
  mensagemHtml = ("Pronto para OTA");
  Serial.println("\nIP do ESP32: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.begin();
}

//ENVIA DADOS PENDENTES COM BASE NA RECONSTRUÇÃO DO TEMPO
void enviarPendentes(){
  if(tempo_desconexao == 0) return;

  for(int i = 0; i < index_pendentes; i++){
    time_t tempo_estimado = tempo_desconexao + (i * 5); // 5 segundos por amostra
    struct tm *timeinfo = localtime(&tempo_estimado);
    strftime(data, sizeof(data), "%m/%d/%Y", timeinfo);
    strftime(hora, sizeof(hora), "%H:%M:%S", timeinfo);

    if(enviarmedicao(pendentes[i], data, hora)){
      Serial.println("Dado pendente enviado.");
      atua_web();
      mensagemHtml = ("Dado pendente enviado.");
    }else{
      Serial.println("Falha ao enviar dado pendente.");
      atua_web();
      mensagemHtml = ("Falha ao enviar dado pendente.");
      break;
    }
  }

  // Limpa buffer
  index_pendentes = 0;
  tempo_desconexao = 0;
}

//LOOP
void loop(){
      // Reconectar Wi-Fi se desconectar
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    // Espera pequena para tentar reconectar (opcional)
    delay(500);
  }
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

//CÁLCULO DA DISTÂNCIA COM BASE NA DURAÇÃO E VELOCIDADE DO PULSO

  duration = pulseIn(echoPin, HIGH);
  distance = (duration * .0343) / 2;
  Serial.print("Distância (cm): ");
  Serial.println(distance);

  if(WiFi.status() == WL_CONNECTED && verificarConexaoInternet() || WiFi.status() == WL_CONNECTED && tdata){
    if(index_pendentes > 0){
      enviarPendentes();
    }

    // obtém tempo atual
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      strftime(data, sizeof(data), "%m/%d/%Y", &timeinfo);
      strftime(hora, sizeof(hora), "%H:%M:%S", &timeinfo);
      enviarmedicao(distance, data, hora);
    }else{
      Serial.println("Erro ao obter hora.");
      atua_web();
      mensagemHtml = ("Erro ao obter hora.");
    }
  }else{
    // registra momento da queda de conexão
    if(tempo_desconexao == 0){
      time_t agora;
      time(&agora);
      tempo_desconexao = agora;
      Serial.println("Conexão perdida. Registrando tempo de desconexão.");
      atua_web();
      mensagemHtml = ("Conexão perdida. Registrando tempo de desconexão.");
    }

    if(index_pendentes < MAX_PENDENTES){
      pendentes[index_pendentes++] = distance;
      Serial.println("Distância armazenada localmente.");
      atua_web();
      mensagemHtml = ("Distância armazenada localmente.");
    }else{
      Serial.println("Buffer cheio. Dados serão descartados.");
      atua_web();
      mensagemHtml = ("Buffer cheio. Dados serão descartados.");
    }
  }

  //Esse delay e de 1s mas a aplicação demora 4s para ser esecutada então toda vez que for modificado o tempo de coleta considere o delay de 4s da aplicação.
  delay(1000); //Área que modifica o tempo do envio do dado
  ArduinoOTA.handle();
  server.handleClient();
  digitalWrite(2, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
}
