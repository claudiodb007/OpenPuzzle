#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <utility>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

class OneShotHttpServer {
public:
  OneShotHttpServer(
      std::string status,
      std::string body,
      std::string expectedPath,
      std::string expectedBody)
      : status_(std::move(status)),
        body_(std::move(body)),
        expectedPath_(std::move(expectedPath)),
        expectedBody_(std::move(expectedBody)) {
    start();
  }

  ~OneShotHttpServer() {
    wait();
  }

  std::string url() const {
    return
        "http://127.0.0.1:" +
        std::to_string(port_);
  }

  void wait() {
    if (childPid_ <= 0) {
      return;
    }

    int status = 0;

    const pid_t result =
        waitpid(
            childPid_,
            &status,
            0);

    childPid_ = 0;

    assert(result > 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
  }

private:
  std::string status_;
  std::string body_;
  std::string expectedPath_;
  std::string expectedBody_;

  int port_ = 0;
  pid_t childPid_ = 0;

  static bool sendAll(
      int socketFd,
      const std::string& content) {
    std::size_t sent = 0;

    while (sent < content.size()) {
      const ssize_t result =
          send(
              socketFd,
              content.data() + sent,
              content.size() - sent,
              MSG_NOSIGNAL);

      if (result <= 0) {
        return false;
      }

      sent +=
          static_cast<std::size_t>(
              result);
    }

    return true;
  }

  static bool readRequest(
      int socketFd,
      std::string& request) {
    char buffer[4096];

    std::size_t expectedSize = 0;

    while (true) {
      const ssize_t received =
          recv(
              socketFd,
              buffer,
              sizeof(buffer),
              0);

      if (received <= 0) {
        return false;
      }

      request.append(
          buffer,
          static_cast<std::size_t>(
              received));

      const auto headerEnd =
          request.find("\r\n\r\n");

      if (headerEnd ==
          std::string::npos) {
        continue;
      }

      if (expectedSize == 0) {
        std::size_t contentLength = 0;

        const auto lengthPosition =
            request.find(
                "Content-Length:");

        if (lengthPosition !=
            std::string::npos) {
          const auto valueStart =
              lengthPosition +
              std::string(
                  "Content-Length:").size();

          const auto valueEnd =
              request.find(
                  "\r\n",
                  valueStart);

          contentLength =
              static_cast<std::size_t>(
                  std::stoul(
                      request.substr(
                          valueStart,
                          valueEnd -
                              valueStart)));
        }

        expectedSize =
            headerEnd + 4 +
            contentLength;
      }

      if (request.size() >=
          expectedSize) {
        return true;
      }
    }
  }

  void start() {
    const int listenFd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);

    assert(listenFd >= 0);

    int reuseAddress = 1;

    assert(
        setsockopt(
            listenFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) == 0);

    sockaddr_in address{};

    address.sin_family =
        AF_INET;

    address.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    address.sin_port =
        htons(0);

    assert(
        bind(
            listenFd,
            reinterpret_cast<sockaddr*>(
                &address),
            sizeof(address)) == 0);

    socklen_t addressSize =
        sizeof(address);

    assert(
        getsockname(
            listenFd,
            reinterpret_cast<sockaddr*>(
                &address),
            &addressSize) == 0);

    port_ =
        ntohs(
            address.sin_port);

    assert(
        listen(
            listenFd,
            1) == 0);

    childPid_ =
        fork();

    assert(childPid_ >= 0);

    if (childPid_ == 0) {
      alarm(10);

      const int clientFd =
          accept(
              listenFd,
              nullptr,
              nullptr);

      if (clientFd < 0) {
        _exit(1);
      }

      std::string request;

      if (!readRequest(
              clientFd,
              request)) {
        close(clientFd);
        close(listenFd);
        _exit(2);
      }

      const std::string requestLine =
          "POST " +
          expectedPath_ +
          " HTTP/1.1\r\n";

      if (request.find(requestLine) != 0) {
        close(clientFd);
        close(listenFd);
        _exit(3);
      }

      const auto headerEnd =
          request.find("\r\n\r\n");

      if (headerEnd ==
          std::string::npos) {
        close(clientFd);
        close(listenFd);
        _exit(4);
      }

      const std::string requestBody =
          request.substr(
              headerEnd + 4);

      if (requestBody !=
          expectedBody_) {
        close(clientFd);
        close(listenFd);
        _exit(5);
      }

      const std::string response =
          "HTTP/1.1 " +
          status_ +
          "\r\n"
          "Content-Type: application/json\r\n"
          "Content-Length: " +
          std::to_string(body_.size()) +
          "\r\n"
          "Connection: close\r\n"
          "\r\n" +
          body_;

      const bool sent =
          sendAll(
              clientFd,
              response);

      close(clientFd);
      close(listenFd);

      _exit(sent ? 0 : 6);
    }

    close(listenFd);
  }
};

ClientExecutionState makeState(
    const std::filesystem::path& workspace,
    int pid) {
  ClientExecutionState state;

  state.active = true;

  state.assignmentId =
      "11111111-1111-4111-8111-111111111111";

  state.clientId =
      "22222222-2222-4222-8222-222222222222";

  state.puzzle = 71;
  state.rangeId = 999999;
  state.pid = pid;
  state.bootId =
      ClientStateStore::currentBootId();
  const auto stateStartTime =
      openpuzzle::LinuxProcessIdentity::
          startTime(state.pid);

  if (stateStartTime) {
    state.processStartTime =
        *stateStartTime;
  }

  state.target =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  state.start =
      "400000000000000000";

  state.end =
      "40000000FFFFFFFFFF";

  state.engine =
      "BitCrack";

  state.backend =
      "CUDA";

  state.workspace =
      workspace.string();

  state.command =
      "test";

  return state;
}

void writeFile(
    const std::filesystem::path& path,
    const std::string& content) {
  std::ofstream output(path);
  output << content;
}

} // namespace

int main() {
  const char* originalHome =
      std::getenv("HOME");

  const bool hadHome =
      originalHome != nullptr;

  const std::string savedHome =
      hadHome
          ? originalHome
          : "";

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-execution-sync-" +
          std::to_string(getpid())
      );

  const auto workspace =
      temporaryHome /
      "workspace";

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      workspace);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  ExecutionSyncService service;

  assert(
      ExecutionSyncService::classifyProgressError(
          "assignment_not_found") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyProgressError(
          "assignment_lease_expired") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyProgressError(
          "invalid_keys_checked") ==
      AssignmentUploadStatus::PermanentFailure);

  assert(
      ExecutionSyncService::classifyProgressError(
          "progress_failed") ==
      AssignmentUploadStatus::TemporaryFailure);

  assert(
      ExecutionSyncService::classifyProgressError(
          "") ==
      AssignmentUploadStatus::TemporaryFailure);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "assignment_not_found") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "invalid_assignment_state") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "invalid_exit_code") ==
      AssignmentUploadStatus::PermanentFailure);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "completion_failed") ==
      AssignmentUploadStatus::TemporaryFailure);

  /*
   * Sem estado local não existe sincronização.
   */
  {
    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(!result.hasState);
  }

  /*
   * found.txt não vazio suspende toda a
   * sincronização e preserva o estado.
   */
  {
    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    writeFile(
        workspace / "found.txt",
        "synthetic-test-secret\n");

    writeFile(
        workspace / "exit.code",
        "0\n");

    const auto solution =
        ExecutionSyncService::solutionFile(
            workspace.string());

    assert(solution);

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(result.solutionFound);
    assert(
        result.solutionPath ==
        (workspace / "found.txt").string());

    assert(!result.completionUploaded);
    assert(!result.stateRemoved);
    assert(ClientStateStore::load());

    std::filesystem::remove(
        workspace / "found.txt");

    std::filesystem::remove(
        workspace / "exit.code");

    assert(
        ClientStateStore::remove());
  }

  /*
   * Ficheiro vazio não representa solução.
   */
  {
    writeFile(
        workspace / "found.txt",
        "");

    assert(
        !ExecutionSyncService::solutionFile(
            workspace.string()));

    std::filesystem::remove(
        workspace / "found.txt");
  }

  /*
   * O resultado nativo do PSCKangaroo também suspende a
   * sincronização. Outros engines não podem interpretá-lo,
   * e ligações simbólicas nunca contam como resultados.
   */
  {
    const auto nativeResult =
        workspace / "RESULTS.TXT";

    writeFile(
        nativeResult,
        "PRIVATE KEY: synthetic-test-secret\n");

    assert(
        ExecutionSyncService::solutionFile(
            workspace.string(),
            "PSCKangaroo"));

    assert(
        !ExecutionSyncService::solutionFile(
            workspace.string(),
            "BitCrack"));

    std::filesystem::remove(nativeResult);
    std::filesystem::create_symlink(
        "/etc/passwd",
        nativeResult);

    assert(
        !ExecutionSyncService::solutionFile(
            workspace.string(),
            "Kangaroo"));

    std::filesystem::remove(nativeResult);
  }

  /*
   * Processo terminado com exit code diferente de zero:
   * tenta reportar failed e preserva o estado quando
   * o servidor não está disponível.
   */
  {
    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    writeFile(
        workspace / "exit.code",
        "7\n");

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.hasExitCode);
    assert(result.exitCode == 7);

    assert(!result.completionUploaded);
    assert(!result.completionError.empty());
    assert(!result.stateRemoved);

    assert(
        ClientStateStore::load());

    assert(
        ClientStateStore::remove());
  }

  /*
   * Processo desaparecido sem exit.code representa
   * interrupção abrupta. Deve tentar cancelar com
   * código -3 e preservar o estado quando o servidor
   * não está disponível.
   */
  {
    std::filesystem::remove(
        workspace / "exit.code");

    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.interrupted);
    assert(result.hasExitCode);
    assert(result.exitCode == -3);

    assert(!result.completionUploaded);
    assert(
        result.completionStatus ==
        AssignmentUploadStatus::TemporaryFailure);
    assert(!result.completionError.empty());
    assert(!result.stateRemoved);

    assert(
        ClientStateStore::load());

    assert(
        ClientStateStore::remove());
  }




  /*
   * Estado legado <= 1.0.16:
   *
   * não existe boot_id, portanto mesmo que o PID numérico exista não há
   * prova de que seja o supervisor original. Deve entrar em recuperação.
   */
  {
    const auto workspace =
        temporaryHome /
        "workspace-legacy-state";

    std::filesystem::create_directories(
        workspace);

    std::filesystem::remove(
        workspace / "exit.code");

    auto state =
        makeState(
            workspace,
            static_cast<int>(getpid()));

    state.bootId.clear();

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.interrupted);
    assert(result.hasExitCode);
    assert(result.exitCode == -3);

    assert(!result.completionUploaded);

    assert(
        result.completionStatus ==
        AssignmentUploadStatus::TemporaryFailure);

    /*
     * A API indisponível não pode destruir o estado legado.
     */
    const auto preserved =
        ClientStateStore::load();

    assert(preserved);
    assert(preserved->bootId.empty());

    assert(
        ClientStateStore::remove());

    std::filesystem::remove_all(
        workspace);
  }


  /*
   * PID reutilizado depois de reboot:
   *
   * o PID existe no sistema atual, mas pertence a um boot diferente.
   * Não pode ser tratado como a execução antiga ainda ativa.
   */
  {
    const auto workspace =
        temporaryHome /
        "workspace-pid-reuse-after-reboot";

    std::filesystem::create_directories(
        workspace);

    std::filesystem::remove(
        workspace / "exit.code");

    auto state =
        makeState(
            workspace,
            static_cast<int>(getpid()));

    state.bootId =
        "00000000-0000-0000-0000-000000000000";

    assert(
        state.bootId !=
        ClientStateStore::currentBootId());

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);

    /*
     * getpid() existe, mas o boot_id não corresponde.
     */
    assert(!result.running);
    assert(result.interrupted);
    assert(result.hasExitCode);
    assert(result.exitCode == -3);

    assert(!result.completionUploaded);
    assert(
        result.completionStatus ==
        AssignmentUploadStatus::TemporaryFailure);

    /*
     * A falha temporária da API continua a preservar o estado.
     */
    const auto preserved =
        ClientStateStore::load();

    assert(preserved);
    assert(
        preserved->bootId ==
        state.bootId);

    assert(
        ClientStateStore::remove());

    std::filesystem::remove_all(
        workspace);
  }


  /*
   * Recuperação concorrente:
   *
   * - CUDA desapareceu sem exit.code;
   * - OpenCL continua vivo;
   * - recuperar CUDA não pode modificar OpenCL;
   * - consultar OpenCL não pode modificar CUDA.
   */
  {
    const auto cudaWorkspace =
        temporaryHome /
        "workspace-cuda-recovery";

    const auto openclWorkspace =
        temporaryHome /
        "workspace-opencl-running";

    std::filesystem::create_directories(
        cudaWorkspace);

    std::filesystem::create_directories(
        openclWorkspace);

    auto cudaState =
        makeState(
            cudaWorkspace,
            999999999);

    cudaState.assignmentId =
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    cudaState.rangeId = 920001;
    cudaState.backend = "CUDA";
    cudaState.device = 0;

    auto openclState =
        makeState(
            openclWorkspace,
            static_cast<int>(getpid()));

    openclState.assignmentId =
        "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    openclState.rangeId = 920002;
    openclState.backend = "OpenCL";
    openclState.device = 1;

    assert(
        ClientStateStore::save(
            cudaState,
            "cuda"));

    assert(
        ClientStateStore::save(
            openclState,
            "opencl"));

    /*
     * CUDA está morto e não tem exit.code:
     * deve ser tratado como interrupção.
     *
     * O servidor de teste não existe, portanto
     * o estado CUDA deve ser preservado para retry.
     */
    const auto cudaRecovery =
        service.tick(
            "http://127.0.0.1:1",
            "cuda");

    assert(cudaRecovery.hasState);
    assert(!cudaRecovery.running);
    assert(cudaRecovery.interrupted);
    assert(cudaRecovery.hasExitCode);
    assert(cudaRecovery.exitCode == -3);

    assert(
        cudaRecovery.completionStatus ==
        AssignmentUploadStatus::TemporaryFailure);

    assert(!cudaRecovery.completionUploaded);
    assert(!cudaRecovery.stateRemoved);

    /*
     * Ambos os estados continuam presentes porque
     * a sincronização CUDA não conseguiu chegar
     * ao servidor.
     */
    const auto cudaAfterRecovery =
        ClientStateStore::load(
            "cuda");

    const auto openclAfterCudaRecovery =
        ClientStateStore::load(
            "opencl");

    assert(cudaAfterRecovery);
    assert(openclAfterCudaRecovery);

    assert(
        cudaAfterRecovery->assignmentId ==
        cudaState.assignmentId);

    assert(
        openclAfterCudaRecovery->assignmentId ==
        openclState.assignmentId);

    /*
     * OpenCL tem um PID vivo.
     * O tick do slot OpenCL deve apenas reconhecer
     * a execução ativa e não tentar finalizá-la.
     */
    const auto openclRecovery =
        service.tick(
            "http://127.0.0.1:1",
            "opencl");

    assert(openclRecovery.hasState);
    assert(openclRecovery.running);
    assert(!openclRecovery.interrupted);
    assert(!openclRecovery.hasExitCode);
    assert(!openclRecovery.completionUploaded);
    assert(!openclRecovery.stateRemoved);

    /*
     * Consultar OpenCL também não pode tocar no
     * estado CUDA pendente de sincronização.
     */
    const auto cudaAfterOpenclTick =
        ClientStateStore::load(
            "cuda");

    const auto openclAfterOwnTick =
        ClientStateStore::load(
            "opencl");

    assert(cudaAfterOpenclTick);
    assert(openclAfterOwnTick);

    assert(
        cudaAfterOpenclTick->assignmentId ==
        cudaState.assignmentId);

    assert(
        openclAfterOwnTick->assignmentId ==
        openclState.assignmentId);

    assert(
        ClientStateStore::remove(
            "cuda"));

    assert(
        ClientStateStore::remove(
            "opencl"));

    std::filesystem::remove_all(
        cudaWorkspace);

    std::filesystem::remove_all(
        openclWorkspace);
  }


  /*
   * Recuperação após reboot completo:
   *
   * CUDA e OpenCL ficaram ambos com estado local,
   * mas os respetivos supervisores desapareceram
   * sem produzir exit.code.
   *
   * Cada slot deve ser classificado de forma
   * independente como interrupção abrupta.
   */
  {
    const auto cudaWorkspace =
        temporaryHome /
        "workspace-dual-dead-cuda";

    const auto openclWorkspace =
        temporaryHome /
        "workspace-dual-dead-opencl";

    std::filesystem::create_directories(
        cudaWorkspace);

    std::filesystem::create_directories(
        openclWorkspace);

    auto cudaState =
        makeState(
            cudaWorkspace,
            999999997);

    cudaState.assignmentId =
        "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
    cudaState.rangeId = 930001;
    cudaState.backend = "CUDA";
    cudaState.device = 0;

    auto openclState =
        makeState(
            openclWorkspace,
            999999998);

    openclState.assignmentId =
        "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
    openclState.rangeId = 930002;
    openclState.backend = "OpenCL";
    openclState.device = 1;

    assert(
        ClientStateStore::save(
            cudaState,
            "cuda"));

    assert(
        ClientStateStore::save(
            openclState,
            "opencl"));

    /*
     * Nenhum workspace tem exit.code.
     */
    assert(
        !std::filesystem::exists(
            cudaWorkspace / "exit.code"));

    assert(
        !std::filesystem::exists(
            openclWorkspace / "exit.code"));

    const auto cudaRecovery =
        service.tick(
            "http://127.0.0.1:1",
            "cuda");

    assert(cudaRecovery.hasState);
    assert(!cudaRecovery.running);
    assert(cudaRecovery.interrupted);
    assert(cudaRecovery.hasExitCode);
    assert(cudaRecovery.exitCode == -3);
    assert(!cudaRecovery.completionUploaded);
    assert(!cudaRecovery.stateRemoved);

    /*
     * Recuperar CUDA não pode destruir OpenCL.
     */
    const auto openclBeforeRecovery =
        ClientStateStore::load(
            "opencl");

    assert(openclBeforeRecovery);

    assert(
        openclBeforeRecovery->assignmentId ==
        openclState.assignmentId);

    const auto openclRecovery =
        service.tick(
            "http://127.0.0.1:1",
            "opencl");

    assert(openclRecovery.hasState);
    assert(!openclRecovery.running);
    assert(openclRecovery.interrupted);
    assert(openclRecovery.hasExitCode);
    assert(openclRecovery.exitCode == -3);
    assert(!openclRecovery.completionUploaded);
    assert(!openclRecovery.stateRemoved);

    /*
     * Ambos permanecem guardados enquanto o servidor
     * não aceitar a sincronização. Isto impede que
     * a recuperação seja perdida durante uma falha
     * temporária de rede/API.
     */
    const auto cudaPending =
        ClientStateStore::load(
            "cuda");

    const auto openclPending =
        ClientStateStore::load(
            "opencl");

    assert(cudaPending);
    assert(openclPending);

    assert(
        cudaPending->assignmentId ==
        cudaState.assignmentId);

    assert(
        openclPending->assignmentId ==
        openclState.assignmentId);

    assert(
        ClientStateStore::remove(
            "cuda"));

    assert(
        ClientStateStore::remove(
            "opencl"));

    std::filesystem::remove_all(
        cudaWorkspace);

    std::filesystem::remove_all(
        openclWorkspace);
  }


  /*
   * Recuperação aceite pelo servidor:
   *
   * dois slots sobrevivem localmente a um reboot,
   * os processos desapareceram e não existe exit.code.
   *
   * Quando a API aceita cancelled/-3:
   * - completionUploaded deve ser true;
   * - completionStatus deve ser Uploaded;
   * - apenas o estado do slot sincronizado é removido.
   */
  {
    const auto cudaWorkspace =
        temporaryHome /
        "workspace-success-cuda";

    const auto openclWorkspace =
        temporaryHome /
        "workspace-success-opencl";

    std::filesystem::create_directories(
        cudaWorkspace);

    std::filesystem::create_directories(
        openclWorkspace);

    auto cudaState =
        makeState(
            cudaWorkspace,
            999999995);

    cudaState.assignmentId =
        "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    cudaState.rangeId = 940001;
    cudaState.backend = "CUDA";
    cudaState.device = 0;

    auto openclState =
        makeState(
            openclWorkspace,
            999999996);

    openclState.assignmentId =
        "ffffffff-ffff-4fff-8fff-ffffffffffff";
    openclState.rangeId = 940002;
    openclState.backend = "OpenCL";
    openclState.device = 1;

    assert(
        ClientStateStore::save(
            cudaState,
            "cuda"));

    assert(
        ClientStateStore::save(
            openclState,
            "opencl"));

    /*
     * Primeiro o CUDA.
     */
    const std::string expectedCudaBody =
        "{\"assignment_id\":"
        "\"eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee\","
        "\"client_id\":"
        "\"22222222-2222-4222-8222-222222222222\","
        "\"exit_code\":-3,"
        "\"status\":\"cancelled\"}";

    OneShotHttpServer cudaServer(
        "200 OK",
        R"JSON({"success":true})JSON",
        "/api/range/complete",
        expectedCudaBody);

    const auto cudaRecovery =
        service.tick(
            cudaServer.url(),
            "cuda");

    cudaServer.wait();

    assert(cudaRecovery.hasState);
    assert(cudaRecovery.interrupted);
    assert(cudaRecovery.hasExitCode);
    assert(cudaRecovery.exitCode == -3);

    assert(cudaRecovery.completionUploaded);

    assert(
        cudaRecovery.completionStatus ==
        AssignmentUploadStatus::Uploaded);

    assert(cudaRecovery.stateRemoved);

    assert(
        !ClientStateStore::load(
            "cuda"));

    /*
     * O OpenCL ainda não foi sincronizado:
     * tem de continuar integralmente presente.
     */
    const auto openclAfterCuda =
        ClientStateStore::load(
            "opencl");

    assert(openclAfterCuda);

    assert(
        openclAfterCuda->assignmentId ==
        openclState.assignmentId);

    /*
     * Agora o OpenCL.
     */
    const std::string expectedOpenclBody =
        "{\"assignment_id\":"
        "\"ffffffff-ffff-4fff-8fff-ffffffffffff\","
        "\"client_id\":"
        "\"22222222-2222-4222-8222-222222222222\","
        "\"exit_code\":-3,"
        "\"status\":\"cancelled\"}";

    OneShotHttpServer openclServer(
        "200 OK",
        R"JSON({"success":true})JSON",
        "/api/range/complete",
        expectedOpenclBody);

    const auto openclRecovery =
        service.tick(
            openclServer.url(),
            "opencl");

    openclServer.wait();

    assert(openclRecovery.hasState);
    assert(openclRecovery.interrupted);
    assert(openclRecovery.hasExitCode);
    assert(openclRecovery.exitCode == -3);

    assert(openclRecovery.completionUploaded);

    assert(
        openclRecovery.completionStatus ==
        AssignmentUploadStatus::Uploaded);

    assert(openclRecovery.stateRemoved);

    assert(
        !ClientStateStore::load(
            "opencl"));

    /*
     * Depois de ambos os ACKs do servidor,
     * não pode restar estado concorrente antigo.
     */
    assert(
        !ClientStateStore::load(
            "cuda"));

    assert(
        !ClientStateStore::load(
            "opencl"));

    std::filesystem::remove_all(
        cudaWorkspace);

    std::filesystem::remove_all(
        openclWorkspace);
  }


  /*
   * BitCrack atualiza o progresso com carriage
   * return. Deve ser usada a última amostra e não
   * a primeira ocorrência da linha acumulada.
   */
  {
    writeFile(
        workspace / "bitcrack.log",
        "AMD Radeon RX 57 | 1 target "
        "400.00 MKey/s "
        "(775,946,240 total) [00:00:01]\r"
        "AMD Radeon RX 57 | 1 target "
        "425.64 MKey/s "
        "(15,518,924,800 total) [00:00:34]\r"
        "AMD Radeon RX 57 | 1 target "
        "428.23 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r");

    const auto progress =
        ExecutionSyncService::latestProgress(
            workspace.string());

    assert(progress);

    assert(
        progress->speedMKeys > 428.22 &&
        progress->speedMKeys < 428.24);

    assert(
        progress->keysChecked ==
        "123354480640");

    std::filesystem::remove(
        workspace / "bitcrack.log");
  }

  /*
   * Um range curto pode terminar antes da primeira
   * linha periódica. O marcador final continua a ser
   * prova válida; progresso sem o marcador não é.
   */
  {
    writeFile(
        workspace / "bitcrack.log",
        "[Info] Starting at: "
        "000000000000000000000000000000000000000000000076000ED8CC0DF8627A\n"
        "[Info] Ending at:   "
        "000000000000000000000000000000000000000000000076000ED8CC9DF86279\n"
        "[Info] Done\n"
        "[Info] Reached end of keyspace\n");

    assert(
        ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    writeFile(
        workspace / "bitcrack.log",
        "GPU | 1 target 425.64 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r");

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    writeFile(
        workspace / "bitcrack.log",
        "GPU | 1 target 425.64 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r"
        "[Info] Reached end of keyspace\n");

    assert(
        ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    const auto count =
        ExecutionSyncService::assignedKeyCount(
            "400000000000000000",
            "40000000FFFFFFFFFF");

    assert(count);
    assert(*count == "1099511627776");

    assert(
        !ExecutionSyncService::assignedKeyCount(
            "20",
            "10"));

    assert(
        !ExecutionSyncService::assignedKeyCount(
            "not-hex",
            "20"));

    std::filesystem::remove(
        workspace / "bitcrack.log");
  }

  /*
   * KeyHunt usa um log próprio. A métrica pública
   * conta dois hashes comprimidos por escalar; o
   * parser devolve chaves privadas reais e exige End.
   */
  {
    writeFile(
        workspace / "keyhunt.log",
        "[+] Total 12509184 keys in 2 seconds: "
        "~6 Mkeys/s (6254592 keys/s)\r"
        "End\n");

    const auto progress =
        ExecutionSyncService::latestProgress(
            workspace.string(),
            "KeyHunt");

    assert(progress);
    assert(
        progress->keysChecked ==
        "6254592");
    assert(
        progress->speedMKeys > 3.127295 &&
        progress->speedMKeys < 3.127297);

    assert(
        ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "KeyHunt"));

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "BitCrack"));

    writeFile(
        workspace / "keyhunt.log",
        "[+] Total 12509184 keys in 2 seconds: "
        "~6 Mkeys/s (6254592 keys/s)\r");

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "KeyHunt"));

    std::filesystem::remove(
        workspace / "keyhunt.log");
  }

  std::filesystem::remove_all(
      temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(
        unsetenv("HOME") == 0);
  }

  /*
   * Same-boot PID reuse:
   *
   * PID exists and boot_id matches, but starttime belongs to a different
   * process instance. The state must enter recovery instead of being
   * considered an active execution.
   */
  {
    const auto workspace =
        temporaryHome /
        "workspace-same-boot-pid-reuse";

    std::filesystem::create_directories(
        workspace);

    std::filesystem::remove(
        workspace / "exit.code");

    auto state =
        makeState(
            workspace,
            static_cast<int>(getpid()));

    const auto currentStartTime =
        openpuzzle::LinuxProcessIdentity::
            startTime(state.pid);

    assert(currentStartTime);
    assert(*currentStartTime > 0);

    state.processStartTime =
        *currentStartTime + 1;

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.interrupted);
    assert(result.hasExitCode);
    assert(result.exitCode == -3);

    assert(!result.completionUploaded);

    assert(
        result.completionStatus ==
        AssignmentUploadStatus::
            TemporaryFailure);

    const auto preserved =
        ClientStateStore::load();

    assert(preserved);

    assert(
        preserved->processStartTime ==
        state.processStartTime);

    assert(
        ClientStateStore::remove());

    std::filesystem::remove_all(
        workspace);
  }


  /*
   * OpenPuzzle 1.0.17 state:
   *
   * PID and boot_id may still be valid, but process_start_time does not
   * exist. The incomplete identity must fail closed and enter recovery.
   */
  {
    const auto workspace =
        temporaryHome /
        "workspace-legacy-1.0.17-starttime";

    std::filesystem::create_directories(
        workspace);

    std::filesystem::remove(
        workspace / "exit.code");

    auto state =
        makeState(
            workspace,
            static_cast<int>(getpid()));

    assert(!state.bootId.empty());

    state.processStartTime = 0;

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.interrupted);
    assert(result.hasExitCode);
    assert(result.exitCode == -3);

    assert(!result.completionUploaded);

    assert(
        result.completionStatus ==
        AssignmentUploadStatus::
            TemporaryFailure);

    const auto preserved =
        ClientStateStore::load();

    assert(preserved);
    assert(!preserved->bootId.empty());
    assert(
        preserved->processStartTime == 0);

    assert(
        ClientStateStore::remove());

    std::filesystem::remove_all(
        workspace);
  }


  std::cout
      << "ExecutionSyncServiceTests passed\n";

  return 0;
}
