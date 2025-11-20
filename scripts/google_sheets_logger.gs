function doPost(e) {
  try {
    // 1. Interpreta o texto da requisição como um objeto JSON.
    var data = JSON.parse(e.postData.contents);

    // 2. Extrai cada valor do objeto JSON.
    var timestamp = new Date(); // Cria um carimbo de data/hora do momento atual.
    var temperaturaAr = data.temperaturaAr;
    var umidadeAr = data.umidadeAr;
    var umidadeSolo = data.umidadeSolo;
    var co2PPM = data.co2PPM;
    var estadoBomba = data.estadoBomba;
    var estadoCooler = data.estadoCooler;
    var classe = data.classe;

    // 3. Acessa a sua planilha ativa e a aba específica onde os dados serão salvos.
    // !!! IMPORTANTE: Mude "Dados" para o nome exato da sua aba na planilha!!!
    var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    var sheet = spreadsheet.getSheetByName("Dados");

    // 4. Adiciona uma nova linha com todos os dados.
    // A ordem aqui determina a ordem das colunas na sua planilha.
    sheet.appendRow([
      timestamp,
      umidadeSolo,
      umidadeAr,
      temperaturaAr,
      co2PPM,
      estadoBomba,
      estadoCooler,
      classe
    ]);

    // 5. Retorna uma resposta de sucesso para o ESP32.
    return ContentService
      .createTextOutput(JSON.stringify({ "result": "success" }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (error) {
    // Em caso de qualquer erro, ele será registrado para depuração.
    Logger.log(error.toString());
    return ContentService
      .createTextOutput(JSON.stringify({ "result": "error", "message": error.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
