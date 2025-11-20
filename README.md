# Protótipo de Estufa Automatizada com TinyML (ESP32)

Este repositório contém os arquivos do Trabalho de Conclusão de Curso (TCC) sobre automação de cultivo doméstico utilizando microcontrolador e Inteligência Artificial na borda (Edge AI).

## 🎯 Visão Geral do Sistema
O projeto implementa uma estratégia híbrida de controle para otimizar o microclima de uma estufa doméstica de morangos:

* **Ventilação:** Controlada por **Machine Learning (TinyML)**. Uma Rede Neural treina no Edge Impulse classifica as condições ambientais e aciona as ventoinhas preventivamente.
* **Irrigação:** Controlada por **Lógica Condicional (Histerese)**. Garante a segurança hídrica da planta baseada em limiares fixos de umidade do solo.

## 📂 Estrutura do Repositório
* `/datasets`: Dados brutos coletados para treinamento (exportados do Edge Impulse).
* `/lib`: Biblioteca de inferência gerada pelo Edge Impulse.
* `/src`: Código fonte principal do firmware (C++).
* `platformio.ini`: Arquivo de configuração do ambiente, dependências

## 🛠️ Hardware Utilizado
* **MCU:** ESP32 (DevKit V1)
* **Sensores:** DHT22 (Temp/Umid), MQ135 e Sensor de Umidade de Solo Resistivo (FC-28).
* **Atuadores:** Bomba d'água e cooler.

## 🚀 Como Reproduzir
1. Clone este repositório.
2. Instale a biblioteca exportada do Edge Impulse (disponível na pasta `/src` ou link externo).
3. Compile o código `main.ino` utilizando a IDE do PlatformIO como extensão do VS Code.

## ☁️ Integração com Google Sheets
O projeto envia dados para uma planilha do Google para registro histórico. O script de recepção encontra-se na pasta `/scripts`.

**Para configurar:**
1. Crie uma nova Planilha Google.
2. Vá em **Extensões > Apps Script**.
3. Copie o conteúdo de `/scripts/google_sheets_logger.gs` e cole no editor.
4. Implante como **App da Web** (Web App) com acesso "Qualquer pessoa (inclusive anônimos)".
5. Copie a URL gerada e atualize a variável `String GOOGLE_SCRIPT_ID` no arquivo `src/main.cpp`.

---
*Desenvolvido como requisito para obtenção do título de Engenheira de Controle e Automação no Instituto Federal de Educação, Ciência e Tecnologia de São Paulo, Campus Hortolândia.*



