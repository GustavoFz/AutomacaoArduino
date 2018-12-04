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

#include <FS.h>                 //Esta precisa ser a primeira referência, ou nada dará certo e sua vida será arruinada. kkk
#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>        //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <EEPROM.h>

#define DEBUG                   //Se descomentar esta linha vai habilitar a 'impressão' na porta serial

//Coloque os valores padrões aqui, porém na interface eles poderão ser substituídos.
#define servidor_mqtt             "m15.cloudmqtt.com"  //URL do servidor MQTT
#define servidor_mqtt_porta       "17206"  //Porta do servidor (a mesma deve ser informada na variável abaixo)
#define servidor_mqtt_usuario     "vwwbegxo"  //Usuário
#define servidor_mqtt_senha       "Znnrk8axY8pB"  //Senha
#define mqtt_topico_sub           "esp8266/pincmd"    //Tópico para subscrever o comando a ser dado no pino declarado abaixo
#define mqtt_topico_pub           "teste"
#define mqtt_topico_pub2           "teste2"
#define INTERVALO_ENVIO           2000

//Declaração do pino que será utilizado e a memória alocada para armazenar o status deste pino na EEPROM
#define pino                      2                   //Pino que executara a acao dado no topico "esp8266/pincmd" e terá seu status informado no tópico "esp8266/pinstatus"
#define memoria_alocada           4                   //Define o quanto sera alocado na EEPROM (valores entre 4 e 4096 bytes)

WiFiClient espClient;                                 //Instância do WiFiClient
PubSubClient client(espClient);                       //Passando a instância do WiFiClient para a instância do PubSubClient

unsigned long primeiraLiagacao;
long previousMillis = 0;
unsigned long inicioTempo;
unsigned long horaInicio;
unsigned long horaFinal;
unsigned long finalTempo;
unsigned long tempoLigado;
unsigned long totalizador;
char formataTempo[11];
char formataHora[11];
uint8_t statusAnt   =             0;                  //Variável que armazenará o status do pino que foi gravado anteriormente na EEPROM
bool precisaSalvar  =             false;              //Flag para salvar os dados

unsigned long horas;
unsigned long minutos;
unsigned long segundos;

//Função para imprimir na porta serial
void imprimirSerial(bool linha, String mensagem) {
#ifdef DEBUG
  if (linha) {
    Serial.println(mensagem);
  } else {
    Serial.print(mensagem);
  }
#endif
}

//Função de retorno para notificar sobre a necessidade de salvar as configurações
void precisaSalvarCallback() {
  imprimirSerial(true, "As configuracoes tem que ser salvas.");
  precisaSalvar = true;
}

//Função que reconecta ao servidor MQTT
void reconectar() {
  //Repete até conectar
  while (!client.connected()) {
    imprimirSerial(false, "Tentando conectar ao servidor MQTT...");

    //Tentativa de conectar. Se o MQTT precisa de autenticação, será chamada a função com autenticação, caso contrário, chama a sem autenticação.
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

//Função que verifica qual foi o último status do pino antes do ESP ser desligado
void lerStatusAnteriorPino() {
  EEPROM.begin(memoria_alocada);  //Aloca o espaco definido na memoria
  statusAnt = EEPROM.read(0);     //Le o valor armazenado na EEPROM e passa para a variável "statusAnt"
  if (statusAnt > 1) {
    statusAnt = 0;                //Provavelmente é a primeira leitura da EEPROM, passando o valor padrão para o pino.
    EEPROM.write(0, statusAnt);
  }
  digitalWrite(pino, statusAnt);
  EEPROM.end();
}

//Função que grava status do pino na EEPROM
void gravarStatusPino(uint8_t statusPino) {
  EEPROM.begin(memoria_alocada);
  EEPROM.write(0, statusPino);
  EEPROM.end();
}

//Calcula tempo
void calculaTempo() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > INTERVALO_ENVIO) {
    previousMillis = currentMillis;

    horas = (currentMillis / 3600000);
    minutos = ( currentMillis / 60000 ) % 60;
    segundos = ( currentMillis / 1000 ) % 60;
    sprintf(formataTempo, "%02d:%02d:%02d", horas, minutos, segundos);
    client.publish(mqtt_topico_pub, formataTempo);
    imprimirSerial(true, "contando...");
    imprimirSerial(true, formataTempo);


    if (digitalRead(pino) == HIGH) {

      horas = (millis() - horaInicio) / 3600000;
      minutos = ((millis() - horaInicio) / 60000 ) % 60;
      segundos = ((millis() - horaInicio) / 1000 ) % 60;
      sprintf(formataHora, "%02d:%02d:%02d", horas, minutos, segundos);
      client.publish(mqtt_topico_pub2, formataHora);
      imprimirSerial(true, "Contagem quando lampada ligada");
      imprimirSerial(true, formataHora);
    }
    //  if (digitalRead(pino) == LOW) {
    //    //    client.publish(mqtt_topico_pub2, formataHora);
    //    //    imprimirSerial(true, "Contagem quando lampada ligada");
    //    //    imprimirSerial(true, formataHora);
    //  }
  }
}

//Função que será chamada ao receber mensagem do servidor MQTT
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
    sprintf(formataTempo, "%02d:%02d:%02d", 0, 0, 0);
    client.publish(mqtt_topico_pub2, formataHora);
    imprimirSerial(true, "Contagem quando lampada ligada");
    imprimirSerial(true, formataHora);

  } else {
    imprimirSerial(true, "Trocando o estado do pino...");
    digitalWrite(pino, !digitalRead(pino));
    gravarStatusPino(digitalRead(pino));
  }

  imprimirSerial(false, "Status do pino depois de processar o comando: ");
  imprimirSerial(true, String(digitalRead(pino)).c_str());

}

//Função inicial (será executado SOMENTE quando ligar o ESP)
void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  imprimirSerial(true, "...");

  if (digitalRead(pino) == LOW){
    sprintf(formataTempo, "%02d:%02d:%02d", 0, 0, 0);
    client.publish(mqtt_topico_pub2, formataHora);
    imprimirSerial(true, "Contagem quando lampada ligada");
    imprimirSerial(true, formataHora);
  }

  calculaTempo();
  //Fazendo o pino ser de saída, pois ele irá "controlar" algo.
  pinMode(pino, OUTPUT);
  //Formatando a memória interna
  //(descomente a linha abaixo enquanto estiver testando e comente ou apague quando estiver pronto)
  //SPIFFS.format();

  //Iniciando o SPIFSS (SPI Flash File System)
  imprimirSerial(true, "Iniciando o SPIFSS (SPI Flash File System)");
  if (SPIFFS.begin()) {
    imprimirSerial(true, "Sistema de arquivos SPIFSS montado!");
    if (SPIFFS.exists("/config.json")) {
      //Arquivo de configuração existe e será lido.
      imprimirSerial(true, "Abrindo o arquivo de configuracao...");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        imprimirSerial(true, "Arquivo de configuracao aberto.");
        size_t size = configFile.size();

        //Alocando um buffer para armazenar o conteúdo do arquivo.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          //Copiando as variáveis salvas previamente no aquivo json para a memória do ESP.
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

  //Parâmetros extras para configuração
  //Depois de conectar, parameter.getValue() vai pegar o valor configurado.
  //Os campos do WiFiManagerParameter são: id do parâmetro, nome, valor padrão, comprimento
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", servidor_mqtt, 40);
  WiFiManagerParameter custom_mqtt_port("port", "Porta", servidor_mqtt_porta, 6);
  WiFiManagerParameter custom_mqtt_user("user", "Usuario", servidor_mqtt_usuario, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "Senha", servidor_mqtt_senha, 20);
  WiFiManagerParameter custom_mqtt_topic_sub("topic_sub", "Topico para subscrever", mqtt_topico_sub, 30);

  //Inicialização do WiFiManager. Uma vez iniciado não é necessário mantê-lo em memória.
  WiFiManager wifiManager;

  //Definindo a função que informará a necessidade de salvar as configurações
  wifiManager.setSaveConfigCallback(precisaSalvarCallback);

  //Adicionando os parâmetros para conectar ao servidor MQTT
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_topic_sub);

  //Busca o ID e senha da rede wifi e tenta conectar.
  //Caso não consiga conectar ou não exista ID e senha,
  //cria um access point com o nome "AutoConnectAP" e a senha "senha123"
  //E entra em loop aguardando a configuração de uma rede WiFi válida.
  if (!wifiManager.autoConnect("AutoConnectAP", "senha123")) {
    imprimirSerial(true, "Falha ao conectar. Excedeu o tempo limite para conexao.");
    delay(3000);
    //Reinicia o ESP e tenta novamente ou entra em sono profundo (DeepSleep)
    ESP.reset();
    delay(5000);
  }

  //Se chegou até aqui é porque conectou na WiFi!
  imprimirSerial(true, "Conectado!! :)");

  //Lendo os parâmetros atualizados
  strcpy(servidor_mqtt, custom_mqtt_server.getValue());
  strcpy(servidor_mqtt_porta, custom_mqtt_port.getValue());
  strcpy(servidor_mqtt_usuario, custom_mqtt_user.getValue());
  strcpy(servidor_mqtt_senha, custom_mqtt_pass.getValue());
  strcpy(mqtt_topico_sub, custom_mqtt_topic_sub.getValue());

  //Salvando os parâmetros informados na tela web do WiFiManager
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
}

//Função de repetição (será executado INFINITAMENTE até o ESP ser desligado)
void loop() {
  if (!client.connected()) {
    reconectar();
  }
  client.loop();
  calculaTempo();
}
