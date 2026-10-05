#include "UiLanguage.hpp"

#include <QHash>
#include <QStringList>

namespace openpuzzle::ui {

namespace {

const QHash<QString, QStringList>& translations() {
  static const QHash<QString, QStringList> values = {
      {"subtitle", {
          "Distributed Bitcoin Puzzle search client",
          "Cliente distribuído de pesquisa Bitcoin Puzzle",
          "Client distribué de recherche Bitcoin Puzzle",
          "Cliente distribuido de búsqueda Bitcoin Puzzle"}},
      {"language", {"Language", "Idioma", "Langue", "Idioma"}},
      {"theme", {"Theme", "Tema", "Thème", "Tema"}},
      {"light", {"Light", "Claro", "Clair", "Claro"}},
      {"dark", {"Dark", "Escuro", "Sombre", "Oscuro"}},
      {"running", {"Running", "Em execução", "En cours", "En ejecución"}},
      {"stopped", {"Stopped", "Parado", "Arrêté", "Detenido"}},
      {"unavailable", {"Unavailable", "Indisponível", "Indisponible", "No disponible"}},
      {"identity_unavailable", {"Identity unavailable", "Identidade indisponível", "Identité indisponible", "Identidad no disponible"}},
      {"status_pending", {"Checking status", "A consultar o estado", "Lecture de l’état", "Consultando el estado"}},
      {"status_unavailable", {"Status unavailable", "Estado indisponível", "État indisponible", "Estado no disponible"}},
      {"status_failed", {"The status command failed.", "O comando de estado falhou.", "La commande d’état a échoué.", "El comando de estado falló."}},
      {"invalid_status", {"The client returned incomplete or unrecognized status information.", "O cliente devolveu informação de estado incompleta ou desconhecida.", "Le client a renvoyé un état incomplet ou non reconnu.", "El cliente devolvió información de estado incompleta o desconocida."}},
      {"status_unavailable_hint", {
          "Refresh status or run diagnostics before starting new work. Last confirmed details are retained.",
          "Atualize o estado ou execute o diagnóstico antes de iniciar novo trabalho. Os últimos detalhes confirmados são conservados.",
          "Actualisez l’état ou lancez le diagnostic avant de démarrer un nouveau travail. Les derniers détails confirmés sont conservés.",
          "Actualice el estado o ejecute el diagnóstico antes de iniciar un nuevo trabajo. Se conservan los últimos detalles confirmados."}},
      {"identity_unavailable_message", {
          "An execution cannot be confirmed. Refresh status or run diagnostics before starting new work.",
          "Não foi possível confirmar uma execução. Atualize o estado ou execute o diagnóstico antes de iniciar novo trabalho.",
          "Une exécution ne peut pas être confirmée. Actualisez l’état ou lancez le diagnostic avant de démarrer un nouveau travail.",
          "No se pudo confirmar una ejecución. Actualice el estado o ejecute el diagnóstico antes de iniciar un nuevo trabajo."}},
      {"current_state", {"Current status", "Estado atual", "État actuel", "Estado actual"}},
      {"refresh", {"Refresh", "Atualizar", "Actualiser", "Actualizar"}},
      {"consulting", {"Reading client status...", "A consultar o estado do cliente...", "Lecture de l’état du client...", "Consultando el estado del cliente..."}},
      {"no_execution", {"No active execution.", "Nenhuma execução ativa.", "Aucune exécution active.", "No hay ninguna ejecución activa."}},
      {"runtime_active", {"The OpenPuzzle runtime is active.", "O runtime OpenPuzzle está ativo.", "Le runtime OpenPuzzle est actif.", "El runtime OpenPuzzle está activo."}},
      {"solution_found", {"Solution found", "Solução encontrada", "Solution trouvée", "Solución encontrada"}},
      {"solution_saved", {"Solution found — saved securely in ~/OpenPuzzle-Solutions. The private key is not displayed or uploaded.", "Solução encontrada — guardada em segurança em ~/OpenPuzzle-Solutions. A chave privada não é mostrada nem enviada.", "Solution trouvée — enregistrée en toute sécurité dans ~/OpenPuzzle-Solutions. La clé privée n’est ni affichée ni envoyée.", "Solución encontrada — guardada de forma segura en ~/OpenPuzzle-Solutions. La clave privada no se muestra ni se envía."}},
      {"puzzle", {"Puzzle", "Puzzle", "Puzzle", "Puzzle"}},
      {"engine", {"Engine", "Engine", "Moteur", "Motor"}},
      {"backend", {"Backend", "Backend", "Backend", "Backend"}},
      {"device", {"Device", "Dispositivo", "Périphérique", "Dispositivo"}},
      {"execution_mode", {"Execution mode", "Modo de execução", "Mode d’exécution", "Modo de ejecución"}},
      {"bitcrack_cuda", {"BitCrack — CUDA", "BitCrack — CUDA", "BitCrack — CUDA", "BitCrack — CUDA"}},
      {"bitcrack_opencl", {"BitCrack — OpenCL", "BitCrack — OpenCL", "BitCrack — OpenCL", "BitCrack — OpenCL"}},
      {"bitcrack_cuda_opencl", {"BitCrack — CUDA + OpenCL", "BitCrack — CUDA + OpenCL", "BitCrack — CUDA + OpenCL", "BitCrack — CUDA + OpenCL"}},
      {"keyhunt_cpu", {"KeyHunt — CPU", "KeyHunt — CPU", "KeyHunt — CPU", "KeyHunt — CPU"}},
      {"kangaroo_cuda", {"Kangaroo — CUDA", "Kangaroo — CUDA", "Kangaroo — CUDA", "Kangaroo — CUDA"}},
      {"cuda_device", {"CUDA device", "Dispositivo CUDA", "Périphérique CUDA", "Dispositivo CUDA"}},
      {"opencl_device", {"OpenCL device", "Dispositivo OpenCL", "Périphérique OpenCL", "Dispositivo OpenCL"}},
      {"cpu_threads", {"CPU threads", "Threads de CPU", "Threads CPU", "Hilos de CPU"}},
      {"rusticl_radeonsi", {"AMD GPU via Rusticl (radeonsi)", "GPU AMD via Rusticl (radeonsi)", "GPU AMD via Rusticl (radeonsi)", "GPU AMD mediante Rusticl (radeonsi)"}},
      {"auto_start", {"Start this search automatically with the computer", "Iniciar esta pesquisa automaticamente com o computador", "Démarrer automatiquement cette recherche avec l’ordinateur", "Iniciar esta búsqueda automáticamente con el ordenador"}},
      {"auto_start_enabled", {"Automatic startup enabled", "Arranque automático ativado", "Démarrage automatique activé", "Inicio automático activado"}},
      {"auto_start_enabled_message", {"The selected search will start at system startup. The current execution was not changed. For startup before login, enable user lingering once: sudo loginctl enable-linger $USER", "A pesquisa escolhida iniciará no arranque do sistema. A execução atual não foi alterada. Para arrancar antes do login, ative uma vez: sudo loginctl enable-linger $USER", "La recherche sélectionnée démarrera avec le système. L’exécution actuelle n’a pas été modifiée. Pour démarrer avant la connexion, activez une fois : sudo loginctl enable-linger $USER", "La búsqueda seleccionada se iniciará con el sistema. La ejecución actual no ha cambiado. Para iniciar antes de acceder, active una vez: sudo loginctl enable-linger $USER"}},
      {"auto_start_disabled", {"Automatic startup disabled", "Arranque automático desativado", "Démarrage automatique désactivé", "Inicio automático desactivado"}},
      {"auto_start_disabled_message", {"The current execution was not stopped.", "A execução atual não foi parada.", "L’exécution actuelle n’a pas été arrêtée.", "La ejecución actual no se ha detenido."}},
      {"auto_start_failed", {"Automatic startup could not be changed", "Não foi possível alterar o arranque automático", "Impossible de modifier le démarrage automatique", "No se pudo cambiar el inicio automático"}},
      {"thermal_safety", {"Thermal safety", "Proteção térmica", "Protection thermique", "Protección térmica"}},
      {"thermal_hint", {"These settings apply to new GPU executions. Active searches are never reconfigured silently.", "Estas definições aplicam-se a novas execuções GPU. As pesquisas ativas nunca são reconfiguradas silenciosamente.", "Ces réglages s’appliquent aux nouvelles exécutions GPU. Les recherches actives ne sont jamais reconfigurées silencieusement.", "Estos ajustes se aplican a nuevas ejecuciones GPU. Las búsquedas activas nunca se reconfiguran silenciosamente."}},
      {"thermal_enabled", {"Enable thermal monitoring", "Ativar monitorização térmica", "Activer la surveillance thermique", "Activar la supervisión térmica"}},
      {"thermal_warning", {"Warning temperature", "Temperatura de aviso", "Température d’avertissement", "Temperatura de aviso"}},
      {"thermal_critical", {"Critical temperature", "Temperatura crítica", "Température critique", "Temperatura crítica"}},
      {"thermal_stop_on_critical", {"Request an orderly stop at the critical temperature", "Pedir uma paragem ordenada à temperatura crítica", "Demander un arrêt ordonné à la température critique", "Solicitar una parada ordenada al alcanzar la temperatura crítica"}},
      {"save_thermal", {"Save thermal settings", "Guardar definições térmicas", "Enregistrer les réglages thermiques", "Guardar ajustes térmicos"}},
      {"thermal_saved_message", {"Thermal settings saved. They will apply to new GPU executions.", "Definições térmicas guardadas. Serão aplicadas às novas execuções GPU.", "Réglages thermiques enregistrés. Ils s’appliqueront aux nouvelles exécutions GPU.", "Ajustes térmicos guardados. Se aplicarán a las nuevas ejecuciones GPU."}},
      {"thermal_invalid", {"The warning temperature must be between 30.0 and 110.0 °C. The critical temperature must be higher and no greater than 120.0 °C.", "A temperatura de aviso deve estar entre 30,0 e 110,0 °C. A temperatura crítica deve ser superior e não pode exceder 120,0 °C.", "La température d’avertissement doit être comprise entre 30,0 et 110,0 °C. La température critique doit être supérieure et ne pas dépasser 120,0 °C.", "La temperatura de aviso debe estar entre 30,0 y 110,0 °C. La temperatura crítica debe ser superior y no superar 120,0 °C."}},
      {"thermal_save_failed", {"The thermal configuration could not be saved. The previous configuration remains unchanged.", "Não foi possível guardar a configuração térmica. A configuração anterior permanece inalterada.", "La configuration thermique n’a pas pu être enregistrée. La configuration précédente reste inchangée.", "No se pudo guardar la configuración térmica. La configuración anterior permanece sin cambios."}},
      {"thermal_state", {"Thermal state", "Estado térmico", "État thermique", "Estado térmico"}},
      {"thermal_state_disabled", {"Disabled", "Desativado", "Désactivé", "Desactivado"}},
      {"thermal_state_unavailable", {"Unavailable", "Indisponível", "Indisponible", "No disponible"}},
      {"thermal_state_normal", {"Normal", "Normal", "Normal", "Normal"}},
      {"thermal_state_warning", {"Warning", "Aviso", "Avertissement", "Aviso"}},
      {"thermal_state_critical", {"Critical", "Crítico", "Critique", "Crítico"}},
      {"thermal_state_invalid", {"Invalid", "Inválido", "Invalide", "No válido"}},
      {"thermal_alert_warning_title", {"GPU temperature warning", "Aviso de temperatura da GPU", "Avertissement de température GPU", "Aviso de temperatura de la GPU"}},
      {"thermal_alert_critical_title", {"Critical GPU temperature", "Temperatura crítica da GPU", "Température GPU critique", "Temperatura crítica de la GPU"}},
      {"thermal_alert_invalid_title", {"GPU sensor warning", "Aviso do sensor da GPU", "Avertissement du capteur GPU", "Aviso del sensor de la GPU"}},
      {"thermal_alert_stop_requested", {"OpenPuzzle requested an orderly stop to preserve synchronization.", "O OpenPuzzle pediu uma paragem ordenada para preservar a sincronização.", "OpenPuzzle a demandé un arrêt ordonné afin de préserver la synchronisation.", "OpenPuzzle solicitó una parada ordenada para conservar la sincronización."}},
      {"thermal_alert_diagnostic_only", {"Diagnostic-only mode is active; the execution has not been stopped automatically.", "O modo apenas de diagnóstico está ativo; a execução não foi parada automaticamente.", "Le mode diagnostic uniquement est actif ; l’exécution n’a pas été arrêtée automatiquement.", "El modo de solo diagnóstico está activo; la ejecución no se ha detenido automáticamente."}},
      {"thermal_alert_invalid_message", {"The sensor reading is invalid. Check the GPU telemetry before continuing unattended.", "A leitura do sensor é inválida. Verifique a telemetria da GPU antes de continuar sem supervisão.", "La lecture du capteur est invalide. Vérifiez la télémétrie GPU avant de continuer sans surveillance.", "La lectura del sensor no es válida. Compruebe la telemetría de la GPU antes de continuar sin supervisión."}},
      {"thermal_transition_warning", {"%1 reached the warning temperature (%2).", "%1 atingiu a temperatura de aviso (%2).", "%1 a atteint la température d’avertissement (%2).", "%1 alcanzó la temperatura de aviso (%2)."}},
      {"thermal_transition_critical", {"%1 reached the critical temperature (%2).", "%1 atingiu a temperatura crítica (%2).", "%1 a atteint la température critique (%2).", "%1 alcanzó la temperatura crítica (%2)."}},
      {"thermal_transition_invalid", {"%1 reported an invalid temperature reading.", "%1 comunicou uma leitura de temperatura inválida.", "%1 a signalé une mesure de température invalide.", "%1 comunicó una lectura de temperatura no válida."}},
      {"thermal_transition_recovered", {"%1 returned to a normal temperature (%2).", "%1 regressou a uma temperatura normal (%2).", "%1 est revenu à une température normale (%2).", "%1 volvió a una temperatura normal (%2)."}},
      {"thermal_history", {"Thermal history", "Histórico térmico", "Historique thermique", "Historial térmico"}},
      {"thermal_history_empty", {"No thermal events have been recorded.", "Ainda não foram registados eventos térmicos.", "Aucun événement thermique n’a été enregistré.", "Todavía no se han registrado eventos térmicos."}},
      {"thermal_history_recovery", {"The local thermal history could not be read. Its file has been preserved. Clear the history to start a new file.", "Não foi possível ler o histórico térmico local. O ficheiro foi preservado. Limpe o histórico para criar um novo ficheiro.", "Impossible de lire l’historique thermique local. Le fichier a été conservé. Effacez l’historique pour créer un nouveau fichier.", "No se pudo leer el historial térmico local. El archivo se ha conservado. Borre el historial para crear un archivo nuevo."}},
      {"thermal_history_clear", {"Clear thermal history", "Limpar histórico térmico", "Effacer l’historique thermique", "Borrar historial térmico"}},
      {"thermal_history_clear_title", {"Clear thermal history", "Limpar histórico térmico", "Effacer l’historique thermique", "Borrar historial térmico"}},
      {"thermal_history_clear_question", {"Permanently remove every locally recorded thermal event?", "Remover permanentemente todos os eventos térmicos registados localmente?", "Supprimer définitivement tous les événements thermiques enregistrés localement ?", "¿Eliminar permanentemente todos los eventos térmicos registrados localmente?"}},
      {"thermal_history_cleared", {"Thermal history cleared.", "Histórico térmico limpo.", "Historique thermique effacé.", "Historial térmico borrado."}},
      {"thermal_history_clear_failed", {"The local thermal history could not be cleared.", "Não foi possível limpar o histórico térmico local.", "Impossible d’effacer l’historique thermique local.", "No se pudo borrar el historial térmico local."}},
      {"thermal_history_save_failed", {"The thermal event could not be saved locally.", "Não foi possível guardar localmente o evento térmico.", "Impossible d’enregistrer localement l’événement thermique.", "No se pudo guardar localmente el evento térmico."}},
      {"speed", {"Speed", "Velocidade", "Vitesse", "Velocidad"}},
      {"temperature", {"Temperature", "Temperatura", "Température", "Temperatura"}},
      {"power", {"Power", "Potência", "Puissance", "Potencia"}},
      {"progress", {"Progress", "Progresso", "Progression", "Progreso"}},
      {"assignment", {"Assignment", "Atribuição", "Attribution", "Asignación"}},
      {"slot", {"Slot", "Slot", "Slot", "Slot"}},
      {"primary_slot", {"PRIMARY", "PRINCIPAL", "PRINCIPAL", "PRINCIPAL"}},
      {"new_execution", {"New execution", "Nova execução", "Nouvelle exécution", "Nueva ejecución"}},
      {"rusticl", {"Rusticl drivers", "Drivers Rusticl", "Pilotes Rusticl", "Controladores Rusticl"}},
      {"rusticl_hint", {"Optional, for example: radeonsi", "Opcional, por exemplo: radeonsi", "Facultatif, par exemple : radeonsi", "Opcional, por ejemplo: radeonsi"}},
      {"start", {"Start", "Iniciar", "Démarrer", "Iniciar"}},
      {"safe_stop", {"Safe Stop", "Safe Stop", "Arrêt sûr", "Parada segura"}},
      {"stop_now", {"Stop now", "Parar agora", "Arrêter maintenant", "Detener ahora"}},
      {"tools", {"Tools", "Ferramentas", "Outils", "Herramientas"}},
      {"benchmark", {"Benchmark", "Benchmark", "Benchmark", "Benchmark"}},
      {"self_test", {"Self-test", "Autoteste", "Auto-test", "Autoprueba"}},
      {"doctor", {"Doctor", "Diagnóstico", "Diagnostic", "Diagnóstico"}},
      {"updates", {"Check updates", "Procurar atualizações", "Rechercher des mises à jour", "Buscar actualizaciones"}},
      {"audit", {"Audit", "Auditoria", "Audit", "Auditoría"}},
      {"install_kangaroo", {"Install Kangaroo", "Instalar Kangaroo", "Installer Kangaroo", "Instalar Kangaroo"}},
      {"details", {"Details and messages", "Detalhes e mensagens", "Détails et messages", "Detalles y mensajes"}},
      {"details_hint", {"OpenPuzzle messages appear here.", "As mensagens do OpenPuzzle aparecem aqui.", "Les messages OpenPuzzle apparaissent ici.", "Los mensajes de OpenPuzzle aparecen aquí."}},
      {"messages", {"Messages", "Mensagens", "Messages", "Mensajes"}},
      {"status_details", {"Raw status", "Estado detalhado", "État détaillé", "Estado detallado"}},
      {"status_hint", {"The detailed client status appears here.", "O estado detalhado do cliente aparece aqui.", "L’état détaillé du client apparaît ici.", "El estado detallado del cliente aparece aquí."}},
      {"runtime_log", {"Runtime log", "Registo da execução", "Journal d’exécution", "Registro de ejecución"}},
      {"runtime_log_hint", {"Continuous execution messages appear here.", "As mensagens da execução contínua aparecem aqui.", "Les messages d’exécution continue apparaissent ici.", "Los mensajes de ejecución continua aparecen aquí."}},
      {"runtime_log_path", {"Runtime log", "Registo da execução", "Journal d’exécution", "Registro de ejecución"}},
      {"runtime_log_failed", {"The private runtime log could not be created.", "Não foi possível criar o registo privado da execução.", "Impossible de créer le journal d’exécution privé.", "No se pudo crear el registro privado de ejecución."}},
      {"footer", {"The interface uses the installed OpenPuzzle client.", "A interface utiliza o cliente OpenPuzzle instalado.", "L’interface utilise le client OpenPuzzle installé.", "La interfaz utiliza el cliente OpenPuzzle instalado."}},
      {"cli_missing", {"The openpuzzle executable was not found in PATH.", "O executável openpuzzle não foi encontrado no PATH.", "L’exécutable openpuzzle est introuvable dans le PATH.", "No se encontró el ejecutable openpuzzle en PATH."}},
      {"status_timeout", {"The status command exceeded 10 seconds.", "O comando de estado excedeu 10 segundos.", "La commande d’état a dépassé 10 secondes.", "El comando de estado superó los 10 segundos."}},
      {"empty_status", {"The client returned no status information.", "O cliente não devolveu informação de estado.", "Le client n’a renvoyé aucune information d’état.", "El cliente no devolvió información de estado."}},
      {"invalid_configuration", {"Invalid configuration", "Configuração inválida", "Configuration invalide", "Configuración no válida"}},
      {"start_failed", {"The client could not be started.", "Não foi possível iniciar o cliente.", "Impossible de démarrer le client.", "No se pudo iniciar el cliente."}},
      {"execution_started", {"Execution started", "Execução iniciada", "Exécution démarrée", "Ejecución iniciada"}},
      {"launcher_pid", {"Launcher PID", "PID do launcher", "PID du lanceur", "PID del lanzador"}},
      {"error", {"Error", "Erro", "Erreur", "Error"}},
      {"control_failed", {"The control command could not be started.", "Não foi possível iniciar o comando de controlo.", "Impossible de lancer la commande de contrôle.", "No se pudo iniciar el comando de control."}},
      {"no_message", {"No additional message.", "Sem mensagem adicional.", "Aucun message supplémentaire.", "Sin mensaje adicional."}},
      {"stop_title", {"Stop OpenPuzzle", "Parar o OpenPuzzle", "Arrêter OpenPuzzle", "Detener OpenPuzzle"}},
      {"stop_question", {"Stop the active execution now? Any available Kangaroo checkpoint will be preserved.", "Parar agora a execução ativa? Qualquer checkpoint Kangaroo disponível será preservado.", "Arrêter l’exécution active maintenant ? Tout checkpoint Kangaroo disponible sera conservé.", "¿Detener ahora la ejecución activa? Se conservará cualquier checkpoint Kangaroo disponible."}},
      {"install_title", {"Install Kangaroo", "Instalar Kangaroo", "Installer Kangaroo", "Instalar Kangaroo"}},
      {"install_question", {"Download and build the pinned Kangaroo engine now?", "Descarregar e compilar agora o motor Kangaroo fixado?", "Télécharger et compiler maintenant le moteur Kangaroo épinglé ?", "¿Descargar y compilar ahora el motor Kangaroo fijado?"}},
      {"safe_stop_hint", {"Finish the current bounded range and block the next one.", "Termina o range limitado atual e bloqueia o seguinte.", "Termine la plage limitée actuelle et bloque la suivante.", "Termina el rango limitado actual y bloquea el siguiente."}},
      {"kangaroo_stop_hint", {"Kangaroo has no bounded completion point; use Stop now.", "Kangaroo não tem um ponto de conclusão limitado; utilize Parar agora.", "Kangaroo n’a pas de point de fin limité ; utilisez Arrêter maintenant.", "Kangaroo no tiene un punto de finalización limitado; use Detener ahora."}},
      {"busy_active", {"Unavailable while an execution is active.", "Indisponível durante uma execução ativa.", "Indisponible pendant une exécution active.", "No disponible durante una ejecución activa."}},
  };

  return values;
}

} // namespace

QString languageCode(UiLanguage language) {
  switch (language) {
  case UiLanguage::Portuguese:
    return "pt";
  case UiLanguage::French:
    return "fr";
  case UiLanguage::Spanish:
    return "es";
  case UiLanguage::English:
  default:
    return "en";
  }
}

UiLanguage languageFromCode(const QString& code) {
  if (code == "pt") {
    return UiLanguage::Portuguese;
  }
  if (code == "fr") {
    return UiLanguage::French;
  }
  if (code == "es") {
    return UiLanguage::Spanish;
  }
  return UiLanguage::English;
}

QString translated(
    UiLanguage language,
    const QString& key) {
  const auto found = translations().constFind(key);
  if (found == translations().constEnd()) {
    return key;
  }

  const int index = static_cast<int>(language);
  return index >= 0 && index < found->size()
      ? found->at(index)
      : found->at(0);
}

} // namespace openpuzzle::ui
