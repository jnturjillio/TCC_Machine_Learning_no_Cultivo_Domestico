/************************************************************************************************
 * * Projeto de TCC: Estufa Automatizada com ESP32
 ************************************************************************************************/

//==============================================================================================
// 1. INCLUSÃO DE BIBLIOTECAS
//==============================================================================================
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Ticker.h>
#include <TCC_-_Estufa_inferencing.h>

//==============================================================================================
// 2. CONFIGURAÇÕES DO PROJETO
//==============================================================================================
// --- Credenciais Wi-Fi ---

char ssid[] = "Rede";
char pass[] = "Senha";

// --- Endpoint do Google Apps Script ---
String googleScriptUrl = "https://script.google.com/macros/s/AKfycbz7H9NlGFZZvSKtc6r4OWo14iC5t1PCYhYkWMLmppAgjSN0C2MgSwDyOxII4sDU2ZBqiQ/exec";

//==============================================================================================
// 3. MAPEAMENTO DE HARDWARE (PINOS)
//==============================================================================================
#define PINO_SENSOR_DHT 18
#define PINO_RELE_BOMBA 13
#define PINO_RELE_COOLER 4
#define PINO_SENSOR_MQ135 32
#define PINO_SENSOR_UMIDADE_SOLO 34

//==============================================================================================
// 4. CONSTANTES E PARÂMETROS
//==============================================================================================

// --- Parâmetros de Histerese para Bomba ---
const int UMIDADE_MINIMA_IRRIGACAO = 20;
const int UMIDADE_MAXIMA_IRRIGACAO = 30;

// --- Parâmetros de Temporização ---
const unsigned long INTERVALO_LEITURAS_MS = 2500;
const unsigned long INTERVALO_ENVIO_HTTP_MS = 2500;
const unsigned long INTERVALO_TELA_LCD_MS = 5000;

// --- Parâmetros de Sensores ---
#define TIPO_SENSOR_DHT DHT22
#define MQ135_PARAM_A 116.6020682
#define MQ135_PARAM_B 2.769034857
#define MQ135_RL_KOHM 10.0

// --- Parâmetros de Deboucing para Ventilação ---
const int CONTAGEM_PARA_ACAO = 5;

//==============================================================================================
// 5. OBJETOS GLOBAIS
//==============================================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(PINO_SENSOR_DHT, TIPO_SENSOR_DHT);

Ticker cicloPrincipalTicker;
Ticker lcdTicker;
Ticker httpTicker;

//==============================================================================================
// 6. VARIÁVEIS GLOBAIS
//==============================================================================================
int umidadeSolo = 0;
float temperaturaAr = 0;
float umidadeAr = 0;
float co2PPM = 0.0;
float mq135_R0 = 0.0;
bool bombaLigada = false;
bool coolerLigado = false;
int classe = 0;
int lcdTelaAtual = 0;

// Variáveis para debouncing do cooler
int contadorParaLigarCooler = 0;
int contadorParaDesligarCooler = 0;

// Variáveis para controle de tempo
unsigned long ultimoTempoLeituras = 0;
unsigned long ultimoTempoEnvioHttp = 0;
unsigned long ultimoTempoTelaLcd = 0;

//==============================================================================================
// 7. PROTÓTIPOS DE FUNÇÕES
//==============================================================================================
void executarCicloPrincipal();
float calibrarMQ135();
void lerUmidadeSolo();
void lerUmidadeTemperaturaAr();
void lerNivelGas();
void tomarDecisaoML();
void tomarDecisaoHisterese();
void gerenciarLCD();
void enviarDadosGoogleSheet();
void configurarHora();


//==============================================================================================
// 8. SETUP
//==============================================================================================
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  pinMode(PINO_RELE_BOMBA, OUTPUT);
  digitalWrite(PINO_RELE_BOMBA, LOW);
  pinMode(PINO_RELE_COOLER, OUTPUT);
  digitalWrite(PINO_RELE_COOLER, LOW);

  // --- Inicia a conexão Wi-Fi ---
  lcd.setCursor(0, 1);
  lcd.print("Conectando WiFi...");
  WiFi.begin(ssid, pass);

  // --- Aguarda a conexão com um tempo limite de 15 segundos ---
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  // --- Verifica o resultado da conexão ---
  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("Conectado!");
    lcd.setCursor(0, 1);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Falha no WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Verifique a rede");
    while(true); // Trava o programa se não conseguir conectar
  }
  delay(2000);

  configurarHora();

  dht.begin();
  mq135_R0 = calibrarMQ135();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Online");
}

//==============================================================================================
// 9. LOOP
//==============================================================================================
void loop() {

  unsigned long tempoAtual = millis(); // Pega o tempo atual no início de cada loop

  // --- Tarefa 1: Executar Leituras e Tomada de Decisão ---
  if (tempoAtual - ultimoTempoLeituras >= INTERVALO_LEITURAS_MS) {
    ultimoTempoLeituras = tempoAtual; // Atualiza o tempo da última execução
    executarCicloPrincipal();
  }

  // --- Tarefa 2: Enviar Dados para a Planilha ---
  if (tempoAtual - ultimoTempoEnvioHttp >= INTERVALO_ENVIO_HTTP_MS) {
    ultimoTempoEnvioHttp = tempoAtual; // Atualiza o tempo da última execução
    enviarDadosGoogleSheet();
  }

  // --- Tarefa 3: Atualizar o Display LCD ---
  if (tempoAtual - ultimoTempoTelaLcd >= INTERVALO_TELA_LCD_MS) {
    ultimoTempoTelaLcd = tempoAtual; // Atualiza o tempo da última execução
    gerenciarLCD();
  }
}

//==============================================================================================
// 10. IMPLEMENTAÇÃO DAS FUNÇÕES
//==============================================================================================
void executarCicloPrincipal() {
  lerUmidadeSolo();
  lerUmidadeTemperaturaAr();
  lerNivelGas();
  tomarDecisaoHisterese(); 
  tomarDecisaoML();
}

void enviarDadosGoogleSheet() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Abortando envio para Google.");
    return;
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("\nEnviando dados para Google Sheet...");

  if (http.begin(client, googleScriptUrl)) {
    http.addHeader("Content-Type", "application/json");

    String estadoBombaString = bombaLigada ? "Ligada" : "Desligada";
    String estadoCoolerString = coolerLigado ? "Ligado" : "Desligado";

    String jsonPayload = "{\"umidadeSolo\":" + String(umidadeSolo) +
                         ",\"umidadeAr\":" + String(umidadeAr, 1) +
                         ",\"temperaturaAr\":" + String(temperaturaAr, 1) +
                         ",\"co2PPM\":" + String(co2PPM, 2) +
                         ",\"estadoBomba\":\"" + estadoBombaString + "\"" +
                         ",\"estadoCooler\":\"" + estadoCoolerString + "\"" +
                         ",\"classe\":" + String(classe) + "}";

    Serial.println("Payload: " + jsonPayload);

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      Serial.printf("[HTTP] Codigo de resposta: %d\n", httpCode);
    } else {
      Serial.printf("[HTTP] Falha no POST, erro: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    } else {
    Serial.printf("[HTTP] Nao foi possivel conectar\n");
    }
}

void configurarHora() {
  const char* servidorNTP = "br.pool.ntp.org";
  const long fusoHorarioSegundos = -10800;
  const int offsetHorarioVerao = 0;
  configTime(fusoHorarioSegundos, offsetHorarioVerao, servidorNTP);
}

void lerUmidadeSolo() {
  int valorBruto = analogRead(PINO_SENSOR_UMIDADE_SOLO);
  umidadeSolo = map(valorBruto, 1600, 3300, 100, 0);
  umidadeSolo = constrain(umidadeSolo, 0, 100);
}

void lerUmidadeTemperaturaAr() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    umidadeAr = h;
    temperaturaAr = t;
  } else {
    Serial.println("Falha na leitura do sensor DHT!");
  }
}

void lerNivelGas() {
  if (mq135_R0 <= 0) return;
  int valorBruto = analogRead(PINO_SENSOR_MQ135);
  float vOut = (valorBruto / 4095.0) * 3.3;
  float rs = MQ135_RL_KOHM * (3.3 - vOut) / vOut;
  float ratio = rs / mq135_R0;
  co2PPM = MQ135_PARAM_A * pow(ratio, -MQ135_PARAM_B);
  }

float calibrarMQ135() {
  const int leiturasCalibracao = 20;
  float r0Acumulado = 0.0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrando MQ135");
  for (int i = 0; i < leiturasCalibracao; i++) {
    float vOut = (analogRead(PINO_SENSOR_MQ135) / 4095.0) * 3.3;
    float rs = MQ135_RL_KOHM * (3.3 - vOut) / vOut;
    r0Acumulado += rs / pow(420.0 /
    MQ135_PARAM_A, -1.0 / MQ135_PARAM_B);
    lcd.setCursor(i, 1);
    lcd.print(".");
    delay(3000);
  }
  return r0Acumulado / leiturasCalibracao;
}

void tomarDecisaoHisterese() {
  bool bombaDeveriaEstarLigada = bombaLigada; // Mantém estado atual por padrão

  // Lógica de histerese
  if (umidadeSolo < UMIDADE_MINIMA_IRRIGACAO && !bombaLigada) {
    bombaDeveriaEstarLigada = true;
    Serial.println(">>> HISTERESE: Ligando bomba - solo seco");
  } else if (umidadeSolo >= UMIDADE_MAXIMA_IRRIGACAO && bombaLigada) {
    bombaDeveriaEstarLigada = false;
    Serial.println(">>> HISTERESE: Desligando bomba - solo úmido");
  }

  // Aplica a ação apenas se houver mudança
  if (bombaDeveriaEstarLigada != bombaLigada) {
    digitalWrite(PINO_RELE_BOMBA, bombaDeveriaEstarLigada ? HIGH : LOW);
    //delay(500); // Pausa para estabilização
    bombaLigada = bombaDeveriaEstarLigada;
  }
}

void tomarDecisaoML() {

  // Dados dos 4 sensores em um "buffer"
  float caracteristicas[] = { (float)umidadeSolo, umidadeAr, temperaturaAr, co2PPM };

  // Cria uma estrutura de sinal para o classificador
  signal_t sinal;
  numpy::signal_from_buffer(caracteristicas, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &sinal);

  // Estrutura para guardar o resultado
  ei_impulse_result_t resultado = { 0 };

  // Executa o modelo de ML
  EI_IMPULSE_ERROR resposta = run_classifier(&sinal, &resultado, false);

  if (resposta != EI_IMPULSE_OK) {
    Serial.printf("Erro na inferência: %d\n", resposta);
    return;
  }

  // Encontra a classe com a maior pontuação (confiança)
  int classe_predita = -1;
  float maior_confianca = 0.0;

  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (resultado.classification[i].value > maior_confianca) {
    maior_confianca = resultado.classification[i].value;
    // Converte o label de texto (ex: "0") para um número (ex: 0)
    classe_predita = atoi(resultado.classification[i].label);
    }
  }
  
  Serial.printf("Classe Predita: %d (Confiança: %.2f)\n", classe_predita, maior_confianca);

  // --- Lógica de Decisão do Cooler (DEBOUNCER ANTI-OSCILAÇÃO) ---
  // O cooler só age se o ML "insistir" por 5 ciclos.
  bool ml_querCoolerLigado = (classe_predita == 2 || classe_predita == 3);

  if (ml_querCoolerLigado) {
    contadorParaLigarCooler++;
    contadorParaDesligarCooler = 0; // Zera o contador oposto
  } else {
    contadorParaDesligarCooler++;
    contadorParaLigarCooler = 0; // Zera o contador oposto
  }
  
  // SÓ AGE SE O ML "INSISTIR" 3 VEZES SEGUIDAS
  if (contadorParaLigarCooler >= CONTAGEM_PARA_ACAO && !coolerLigado) {
      digitalWrite(PINO_RELE_COOLER, HIGH);
      coolerLigado = true;
      Serial.println(">>> ML (Confirmado): Ligando Cooler.");
      contadorParaLigarCooler = 0; // Reseta o contador
  } 
  else if (contadorParaDesligarCooler >= CONTAGEM_PARA_ACAO && coolerLigado) {
      digitalWrite(PINO_RELE_COOLER, LOW);
      coolerLigado = false;
      Serial.println(">>> ML (Confirmado): Desligando Cooler.");
      contadorParaDesligarCooler = 0; // Reseta o contador
  }
  
  // --- Atualiza a classe global para o Google Sheets/LCD ---
  // (Baseado no estado REAL dos relés)
  if (bombaLigada && !coolerLigado) {
    classe = 1;
  } else if (!bombaLigada && coolerLigado) {
    classe = 2;
  } else if (bombaLigada && coolerLigado) {
    classe = 3;
  } else {
    classe = 0;
  }
}

void gerenciarLCD() {
  lcdTelaAtual = (lcdTelaAtual + 1) % 3;
  lcd.clear();
  switch (lcdTelaAtual) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Umid. Solo: ");
      lcd.print(umidadeSolo);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("Bomba: ");
      lcd.print(bombaLigada ? "Lig." : "Desl.");
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Umid. Ar: ");
      lcd.print((int)umidadeAr);
      lcd.print("%");
      lcd.setCursor(0, 1);
      lcd.print("Temp. Ar: ");
      lcd.print((int)temperaturaAr);
      lcd.write(223); // Símbolo de grau °
      lcd.print("C");
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("CO2: ");
      lcd.print((int)co2PPM);
      lcd.print(" PPM");
      lcd.setCursor(0, 1);
      lcd.print("Cooler: ");
      lcd.print(coolerLigado ? "Lig." : "Desl.");
      break;
  }
}