#pragma once
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include <cstdint>
#include <string>
namespace openpuzzle::client {
struct HttpResponse {
  unsigned status=0;
  std::string body,error,redirectLocation;
  bool success()const{return error.empty()&&status>=200&&status<300;}
};
class WindowsHttpTransport {
public:
  // Synchronous, direct WinHTTP request. No shell, application retries, cookies
  // or automatic redirects. Same-origin redirects are explicitly bounded.
  // HTTPS retains normal Windows certificate checks.
  // Timeout applies to each operation; total deadline is checked between reads.
  static HttpResponse request(const std::string& method,const std::string& url,
      const std::string& body={},std::uint32_t timeoutMilliseconds=30000);
};
}
