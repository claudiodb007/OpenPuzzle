#include "openpuzzle/client/HttpRangeClient.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

class OneShotHttpServer {
public:
  OneShotHttpServer(
      std::string status,
      std::string body,
      std::string expectedPath = {},
      std::string expectedBody = {})
      : status_(std::move(status)),
        body_(std::move(body)),
        expectedPath_(
            std::move(expectedPath)),
        expectedBody_(
            std::move(expectedBody)) {
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
      const std::string &content) {
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
      std::string &request) {
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
            reinterpret_cast<sockaddr *>(
                &address),
            sizeof(address)) == 0);

    socklen_t addressSize =
        sizeof(address);

    assert(
        getsockname(
            listenFd,
            reinterpret_cast<sockaddr *>(
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

      if (!expectedPath_.empty()) {
        const std::string requestLine =
            "POST " +
            expectedPath_ +
            " HTTP/1.1\r\n";

        if (
            request.find(requestLine) !=
            0
        ) {
          close(clientFd);
          close(listenFd);
          _exit(4);
        }
      }

      if (!expectedBody_.empty()) {
        const auto headerEnd =
            request.find("\r\n\r\n");

        if (
            headerEnd ==
            std::string::npos
        ) {
          close(clientFd);
          close(listenFd);
          _exit(5);
        }

        const std::string requestBody =
            request.substr(
                headerEnd + 4);

        if (
            requestBody !=
            expectedBody_
        ) {
          close(clientFd);
          close(listenFd);
          _exit(6);
        }
      }

      const std::string response =
          "HTTP/1.1 " + status_ +
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

      _exit(sent ? 0 : 3);
    }

    close(listenFd);
  }
};

} // namespace

int main() {
  const std::string assignmentId =
      "11111111-1111-4111-8111-111111111111";

  const std::string clientId =
      "22222222-2222-4222-8222-222222222222";

  /*
   * O pedido de atribuição identifica explicitamente
   * o backend mesmo quando o slot local é primary.
   */
  {
    const std::string expectedBody =
        "{\"client_id\":"
        "\"22222222-2222-4222-8222-222222222222\","
        "\"puzzle\":71,"
        "\"execution_slot\":\"primary\","
        "\"backend\":\"cpu\","
        "\"target_duration_minutes\":60,"
        "\"speed_mkeys\":0.000000}";

    OneShotHttpServer server(
        "200 OK",
        R"JSON({
          "assignment_id":
            "33333333-3333-4333-8333-333333333333",
          "puzzle": 71,
          "range_id": 591,
          "target":
            "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU",
          "start": "400000000000000000",
          "end": "400000000000000FFF"
        })JSON",
        "/api/range/claim",
        expectedBody);

    HttpRangeClient client(
        server.url());

    const auto assignment =
        client.claim(
            clientId,
            71,
            60,
            0.0,
            "cpu");

    assert(assignment);
    assert(client.lastError().empty());

    server.wait();
  }

  /*
   * Rejeição de progresso atravessa o curl e
   * preserva o código JSON do servidor.
   */
  {
    OneShotHttpServer server(
        "409 Conflict",
        R"JSON({
          "error": "assignment_lease_expired",
          "message": "Assignment lease has expired"
        })JSON");

    HttpRangeClient client(
        server.url());

    assert(
        !client.progress(
            assignmentId,
            clientId,
            1250.5,
            "2500000"));

    assert(
        client.lastErrorCode() ==
        "assignment_lease_expired");

    assert(
        client.lastError().find(
            "Assignment lease has expired") !=
        std::string::npos);

    server.wait();
  }

  /*
   * Rejeição da conclusão também preserva
   * o respetivo código JSON.
   */
  {
    OneShotHttpServer server(
        "409 Conflict",
        R"JSON({
          "error": "invalid_assignment_state",
          "message": "Assignment is not assigned"
        })JSON");

    HttpRangeClient client(
        server.url());

    assert(
        !client.complete(
            assignmentId,
            clientId,
            0,
            "completed",
            "2500000"));

    assert(
        client.lastErrorCode() ==
        "invalid_assignment_state");

    assert(
        client.lastError().find(
            "Assignment is not assigned") !=
        std::string::npos);

    server.wait();
  }


  /*
   * O relatório de solução atravessa uma ligação
   * HTTP real com somente assignment_id e client_id.
   */
  {
    const std::string expectedBody =
        "{\"assignment_id\":"
        "\"11111111-1111-4111-8111-111111111111\","
        "\"client_id\":"
        "\"22222222-2222-4222-8222-222222222222\"}";

    OneShotHttpServer server(
        "201 Created",
        R"JSON({
          "success": true,
          "report_id":
            "33333333-3333-4333-8333-333333333333",
          "puzzle": 71,
          "status": "pending",
          "already_reported": false
        })JSON",
        "/api/puzzle/report-solution",
        expectedBody);

    HttpRangeClient client(
        server.url());

    assert(
        client.reportSolution(
            assignmentId,
            clientId)
    );

    assert(
        client.lastError().empty()
    );

    assert(
        client.lastErrorCode().empty()
    );

    server.wait();
  }

  std::cout
      << "HttpRangeClientIntegrationTests passed\n";

  return 0;
}
