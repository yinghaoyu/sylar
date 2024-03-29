#include <string>

#include "http_session.h"
#include "http_parser.h"

namespace sylar {
namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner)
    : SocketStream(sock, owner) {}

HttpRequest::ptr HttpSession::recvRequest() {
  HttpRequestParser::ptr parser = std::make_shared<HttpRequestParser>();
  uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
  std::shared_ptr<char> buffer(new char[buff_size],
                               [](char* ptr) { delete[] ptr; });
  char* data = buffer.get();
  int offset = 0;
  do {
    int len = read(data + offset, buff_size - offset);
    if (len <= 0) {
      close();
      return nullptr;
    }
    len += offset;
    size_t nparse = parser->execute(data, len);
    if (parser->hasError()) {
      close();
      return nullptr;
    }
    offset = len - nparse;
    if (offset == (int)buff_size) {
      close();
      return nullptr;
    }
    if (parser->isFinished()) {
      break;
    }
  } while (true);
  int64_t length = parser->getContentLength();

  auto v = parser->getData()->getHeader("Expect");
  if (strcasecmp(v.c_str(), "100-continue") == 0) {
    // 客户端在发送 POST 数据前，先征询服务器的情况。
    // 通常在处理 POST 大数据时，才会使用 100-continue 协议
    // 服务端如果处理，只需要返回 100-continue 给客户端即可
    static const std::string s_data = "HTTP/1.1 100 Continue\r\n\r\n";
    writeFixSize(s_data.c_str(), s_data.size());
    parser->getData()->delHeader("Expect");
  }

  if (length > 0) {
    std::string body;
    body.resize(length);

    int len = 0;
    if (length >= offset) {
      memcpy(&body[0], data, offset);
      len = offset;
    } else {
      memcpy(&body[0], data, length);
      len = length;
    }
    length -= offset;
    if (length > 0) {
      if (readFixSize(&body[len], length) <= 0) {
        close();
        return nullptr;
      }
    }
    parser->getData()->setBody(body);
  }
  parser->getData()->init();
  return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
  std::stringstream ss;
  ss << *rsp;
  std::string data = ss.str();
  return writeFixSize(data.c_str(), data.size());
}

}  // namespace http
}  // namespace sylar
