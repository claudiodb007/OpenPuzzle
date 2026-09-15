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
      {"speed", {"Speed", "Velocidade", "Vitesse", "Velocidad"}},
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
