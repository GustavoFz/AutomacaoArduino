/*
  Equivalencia das saidas Digitais entre NodeMCU e ESP8266 (na IDE do Arduino)
  NodeMCU - ESP8266
  D0 = 16;
  D1 = 5;
  D2 = 4;
  D3 = 0;
  D4 = 2;
  D5 = 14;
  D6 = 12;
  D7 = 13;
  D8 = 15;
  D9 = 3;
  D10 = 1;
*/

#include <FS.h>                 //Esta precisa ser a primeira referÃªncia, ou nada darÃ¡ certo e sua vida serÃ¡ arruinada. kkk
#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>        //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <EEPROM.h>

#define DEBUG                   //Se descomentar esta linha vai habilitar a 'impressÃ£o' na porta serial

//Coloque os valores padrÃµes aqui, porÃ©m na interface eles poderÃ£o ser substituÃ­dos.
#define servidor_mqtt             "m15.cloudmqtt.com"  //URL do servidor MQTT
#define servidor_mqtt_porta       "17206"  //Porta do servidor (a mesma deve ser informada na variÃ¡vel abaixo)
#define servidor_mqtt_usuario     "vwwbegxo"  //UsuÃ¡rio
#define servidor_mqtt_senha       "Znnrk8axY8pB"  //Senha
#define mqtt_topico_sub           "esp8266/pincmd"    //TÃ³pico para subscrever o comando a ser dado no pino declarado abaixo
#define mqtt_topico_pub           "tempoTotal"
#define mqtt_topico_pub2          "tempoLampadaLigada"
#define mqtt_topico_pub3          "tempoTotalLampadaLigada"
//#define mqtt_topico_pub4          "gastoEnergiaMinuto"
#define INTERVALO_ENVIO           2000

//DeclaraÃ§Ã£o do pino que serÃ¡ utilizado e a memÃ³ria alocada para armazenar o status deste pino na EEPROM
#define pino                      2                   //Pino que executara a acao dado no topico "esp8266/pincmd" e terÃ¡ seu status informado no tÃ³pico "esp8266/pinstatus"
#define memoria_alocada           4                   //Define o quanto sera alocado na EEPROM (valores entre 4 e 4096 bytes)

WiFiClient espClient;                                 //InstÃ¢ncia do WiFiClient
PubSubClient client(espClient);                       //Passando a instÃ¢ncia do WiFiClient para a instÃ¢ncia do PubSubClient

unsigned long primeiraLiagacao;
long previousMillis = 0;
unsigned long horaInicio;
unsigned long salvaTempo1;
unsigned long salvaTempo2;
unsigned long salvaTempo3;
unsigned long currentMillis;
//unsigned long gastoEnergiaMinuto;

unsigned long acumuladorHora = 0;
unsigned long acumuladorMinuto = 0;
unsigned long acumuladorSegundo = 0;

char Total[14];
char formataTempo[14];
char formataHora[14];
//char TotalEnergia[14];


uint8_t statusAnt   =             0;                  //VariÃ¡vel que armazenarÃ¡ o status do pino que foi gravado anteriormente na EEPROM
bool precisaSalvar  =             false;              //Flag para salvar os dados

unsigned long horas;
unsigned long minutos;
unsigned long segundos;
unsigned long horas1;
unsigned long minutos1;
unsigned long segundos1;

//FunÃ§Ã£o para imprimir na porta serial
void imprimirSerial(bool linha, String mensagem) {
#ifdef DEBUG
  if (linha) {
    Serial.println(mensagem);
  } else {
    Serial.print(mensagem);
  }
#endif
}

//FunÃ§Ã£o de retorno para notificar sobre a necessidade de salvar as configuraÃ§Ãµes
void precisaSalvarCallback() {
  imprimirSerial(true, "As configuracoes tem que ser salvas.");
  precisaSalvar = true;
}

//FunÃ§Ã£o que reconecta ao servidor MQTT
void reconectar() {
  //Repete atÃ© conectar
  while (!client.connected()) {
    imprimirSerial(false, "Tentando conectar ao servidor MQTT...");

    //Tentativa de conectar. Se o MQTT precisa de autenticaÃ§Ã£o, serÃ¡ chamada a funÃ§Ã£o com autenticaÃ§Ã£o, caso contrÃ¡rio, chama a sem autenticaÃ§Ã£o.
    bool conectado = strlen(servidor_mqtt_usuario) > 0 ?
                     client.connect("ESP8266Client", servidor_mqtt_usuario, servidor_mqtt_senha) :
                     client.connect("ESP8266Client");

    if (conectado) {
      imprimirSerial(true, "Conectado!");
      //Subscreve para monitorar os comandos recebidos
      client.subscribe(mqtt_topico_sub, 1); //QoS 1
    } else {
      imprimirSerial(false, "Falhou ao tentar conectar. Codigo: ");
      imprimirSerial(false, String(client.state()).c_str());
      imprimirSerial(true, " tentando novamente em 5 segundos");
      //Aguarda 5 segundos para tentar novamente
      delay(5000);
    }
  }
}

//FunÃ§Ã£o que verifica qual foi o Ãºltimo status do pino antes do ESP ser desligado
void lerStatusAnteriorPino() {
  EEPROM.begin(memoria_alocada);  //Aloca o espaco definido na memoria
  statusAnt = EEPROM.read(0);     //Le o valor armazenado na EEPROM e passa para a variÃ¡vel "statusAnt"
  if (statusAnt > 1) {
    statusAnt = 0;                //Provavelmente Ã© a primeira leitura da EEPROM, passando o valor padrÃ£o para o pino.
    EEPROM.write(0, statusAnt);
  }
  digitalWrite(pino, statusAnt);
  EEPROM.end();
}

//FunÃ§Ã£o que grava status do pino na EEPROM
void gravarStatusPino(uint8_t statusPino) {
  EEPROM.begin(memoria_alocada);
  EEPROM.write(0, statusPino);
  EEPROM.end();
}

//Calcula tempo
void calculaTempo() {

  currentMillis = millis();

  if (currentMillis - previousMillis > INTERVALO_ENVIO) {
    previousMillis = currentMillis;

    horas1 = currentMillis / 3600000;
    minutos1 = ( currentMillis / 60000 ) % 60;
    segundos1 = ( currentMillis / 1000 ) % 60;
    sprintf(formataTempo, "%02d:%02d:%02d", horas1, minutos1, segundos1);
    client.publish(mqtt_topico_pub, formataTempo);
    imprimirSerial(false, "contando...");
    imprimirSerial(true, formataTempo);


    if (digitalRead(pino) == HIGH) {

      horas = (currentMillis - horaInicio) / 3600000;
      minutos = (currentMillis - horaInicio) / 60000;
      segundos = (currentMillis - horaInicio) / 1000;
      sprintf(formataHora, "%02d:%02d:%02d", horas, minutos % 60, segundos % 60);
      client.publish(mqtt_topico_pub2, formataHora);
      imprimirSerial(true, "Lampada ligada: ");
      imprimirSerial(true, formataHora);

      salvaTempo1 = (acumuladorHora + horas);
      salvaTempo2 = (acumuladorMinuto + minutos) % 60;
      salvaTempo3 = (acumuladorSegundo + segundos) % 60;
      sprintf(Total, "%02d:%02d:%02d", salvaTempo1, salvaTempo2, salvaTempo3);
      imprimirSerial(false, "Lampada ligada total: ");
      imprimirSerial(true, Total);
      client.publish(mqtt_topico_pub3, Total);
    }

//    gastoEnergiaMinuto = ((0.015 * salvaTempo1) * 100) / 60;
//    sprintf(TotalEnergia, "%.2f", gastoEnergiaMinuto);
//    client.publish(mqtt_topico_pub4, TotalEnergia);
//    imprimirSerial(false, "Energia ligada total: ");
//    imprimirSerial(true, TotalEnergia);
//    imprimirSerial(true, String(gastoEnergiaMinuto);
  }
}

//FunÃ§Ã£o que serÃ¡ chamada ao receber mensagem do servidor MQTT
void retorno(char* topico, byte* mensagem, unsigned int tamanho) {
  //Convertendo a mensagem recebida para string
  mensagem[tamanho] = '\0';
  String strMensagem = String((char*)mensagem);
  strMensagem.toLowerCase();
  //float f = s.toFloat();

  imprimirSerial(false, "Mensagem recebida! Topico: ");
  imprimirSerial(false, topico);
  imprimirSerial(false, ". Tamanho: ");
  imprimirSerial(false, String(tamanho).c_str());
  imprimirSerial(false, ". Mensagem: ");
  imprimirSerial(true, strMensagem);

  //Executando o comando solicitado
  imprimirSerial(false, "Status do pino antes de processar o comando: ");
  imprimirSerial(true, String(digitalRead(pino)).c_str());

  if (strMensagem == "liga") {
    imprimirSerial(true, "Colocando o pino em stado ALTO...");
    digitalWrite(pino, HIGH);
    gravarStatusPino(HIGH);
    horaInicio = millis();

  } else if (strMensagem == "desliga") {
    imprimirSerial(true, "Colocando o pino em stado BAIXO...");
    digitalWrite(pino, LOW);
    gravarStatusPino(LOW);
    imprimirSerial(true, "Contagem quando lampada ligada");
    imprimirSerial(true, formataHora);
    acumuladorHora = acumuladorHora + horas;
    acumuladorMinuto = acumuladorMinuto + minutos;
    acumuladorSegundo = acumuladorSegundo + segundos;

  } else {
    imprimirSerial(true, "Trocando o estado do pino...");
    digitalWrite(pino, !digitalRead(pino));
    gravarStatusPino(digitalRead(pino));
  }

  imprimirSerial(false, "Status do pino depois de processar o comando: ");
  imprimirSerial(true, String(digitalRead(pino)).c_str());

}

//FunÃ§Ã£o inicial (serÃ¡ executado SOMENTE quando ligar o ESP)
void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  imprimirSerial(true, "...");


  //Fazendo o pino ser de saÃ­da, pois ele irÃ¡ "controlar" algo.
  pinMode(pino, OUTPUT);
  //Formatando a memÃ³ria interna
  //(descomente a linha abaixo enquanto estiver testando e comente ou apague quando estiver pronto)
  //SPIFFS.format();

  //Iniciando o SPIFSS (SPI Flash File System)
  imprimirSerial(true, "Iniciando o SPIFSS (SPI Flash File System)");
  if (SPIFFS.begin()) {
    imprimirSerial(true, "Sistema de arquivos SPIFSS montado!");
    if (SPIFFS.exists("/config.json")) {
      //Arquivo de configuraÃ§Ã£o existe e serÃ¡ lido.
      imprimirSerial(true, "Abrindo o arquivo de configuracao...");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        imprimirSerial(true, "Arquivo de configuracao aberto.");
        size_t size = configFile.size();

        //Alocando um buffer para armazenar o conteÃºdo do arquivo.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          //Copiando as variÃ¡veis salvas previamente no aquivo json para a memÃ³ria do ESP.
          imprimirSerial(true, "arquivo json analisado.");
          strcpy(servidor_mqtt, json["servidor_mqtt"]);
          strcpy(servidor_mqtt_porta, json["servidor_mqtt_porta"]);
          strcpy(servidor_mqtt_usuario, json["servidor_mqtt_usuario"]);
          strcpy(servidor_mqtt_senha, json["servidor_mqtt_senha"]);
          strcpy(mqtt_topico_sub, json["mqtt_topico_sub"]);

        } else {
          imprimirSerial(true, "Falha ao ler as configuracoes do arquivo json.");
        }
      }
    }
  } else {
    imprimirSerial(true, "Falha ao montar o sistema de arquivos SPIFSS.");
  }
  //Fim da leitura do sistema de arquivos SPIFSS

  //ParÃ¢metros extras para configuraÃ§Ã£o
  //Depois de conectar, parameter.getValue() vai pegar o valor configurado.
  //Os campos do WiFiManagerParameter sÃ£o: id do parÃ¢metro, nome, valor padrÃ£o, comprimento
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", servidor_mqtt, 40);
  WiFiManagerParameter custom_mqtt_port("port", "Porta", servidor_mqtt_porta, 6);
  WiFiManagerParameter custom_mqtt_user("user", "Usuario", servidor_mqtt_usuario, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "Senha", servidor_mqtt_senha, 20);
  WiFiManagerParameter custom_mqtt_topic_sub("topic_sub", "Topico para subscrever", mqtt_topico_sub, 30);

  //InicializaÃ§Ã£o do WiFiManager. Uma vez iniciado nÃ£o Ã© necessÃ¡rio mantÃª-lo em memÃ³ria.
  WiFiManager wifiManager;

  //Definindo a funÃ§Ã£o que informarÃ¡ a necessidade de salvar as configuraÃ§Ãµes
  wifiManager.setSaveConfigCallback(precisaSalvarCallback);

  //Adicionando os parÃ¢metros para conectar ao servidor MQTT
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_topic_sub);

  //Busca o ID e senha da rede wifi e tenta conectar.
  //Caso nÃ£o consiga conectar ou nÃ£o exista ID e senha,
  //cria um access point com o nome "AutoConnectAP" e a senha "senha123"
  //E entra em loop aguardando a configuraÃ§Ã£o de uma rede WiFi vÃ¡lida.
  if (!wifiManager.autoConnect("AutoConnectAP", "senha123")) {
    imprimirSerial(true, "Falha ao conectar. Excedeu o tempo limite para conexao.");
    delay(3000);
    //Reinicia o ESP e tenta novamente ou entra em sono profundo (DeepSleep)
    ESP.reset();
    delay(5000);
  }

  //Se chegou atÃ© aqui Ã© porque conectou na WiFi!
  imprimirSerial(true, "Conectado!! :)");

  //Lendo os parÃ¢metros atualizados
  strcpy(servidor_mqtt, custom_mqtt_server.getValue());
  strcpy(servidor_mqtt_porta, custom_mqtt_port.getValue());
  strcpy(servidor_mqtt_usuario, custom_mqtt_user.getValue());
  strcpy(servidor_mqtt_senha, custom_mqtt_pass.getValue());
  strcpy(mqtt_topico_sub, custom_mqtt_topic_sub.getValue());

  //Salvando os parÃ¢metros informados na tela web do WiFiManager
  if (precisaSalvar) {
    imprimirSerial(true, "Salvando as configuracoes");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["servidor_mqtt"] = servidor_mqtt;
    json["servidor_mqtt_porta"] = servidor_mqtt_porta;
    json["servidor_mqtt_usuario"] = servidor_mqtt_usuario;
    json["servidor_mqtt_senha"] = servidor_mqtt_senha;
    json["mqtt_topico_sub"] = mqtt_topico_sub;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      imprimirSerial(true, "Houve uma falha ao abrir o arquivo de configuracao para incluir/alterar as configuracoes.");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  imprimirSerial(false, "IP: ");
  imprimirSerial(true, WiFi.localIP().toString());

  //Informando ao client do PubSub a url do servidor e a porta.
  int portaInt = atoi(servidor_mqtt_porta);
  client.setServer(servidor_mqtt, portaInt);
  client.setCallback(retorno);

  //Obtendo o status do pino antes do ESP ser desligado
  lerStatusAnteriorPino();

  if (digitalRead(pino) == LOW) {
    sprintf(formataHora, "%02d:%02d:%02d", 0, 0, 0);
  }
  calculaTempo();
}



//FunÃ§Ã£o de repetiÃ§Ã£o (serÃ¡ executado INFINITAMENTE atÃ© o ESP ser desligado)
void loop() {
  if (!client.connected()) {
    reconectar();
  }
  client.loop();
  calculaTempo();
}

