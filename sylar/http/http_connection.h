#ifndef __SYLAR_HTTP_CONNECTION_H__
#define __SYLAR_HTTP_CONNECTION_H__

#include <list>

#include "http.h"
#include "sylar/streams/socket_stream.h"
#include "sylar/thread.h"
#include "sylar/uri.h"

namespace sylar {
namespace http {

struct HttpResult {
  typedef std::shared_ptr<HttpResult> ptr;
  enum class Error {
    /// 正常
    OK = 0,
    /// 非法URL
    INVALID_URL = 1,
    /// 无法解析HOST
    INVALID_HOST = 2,
    /// 连接失败
    CONNECT_FAIL = 3,
    /// 连接被对端关闭
    SEND_CLOSE_BY_PEER = 4,
    /// 发送请求产生Socket错误
    SEND_SOCKET_ERROR = 5,
    /// 超时
    TIMEOUT = 6,
    /// 创建Socket失败
    CREATE_SOCKET_ERROR = 7,
    /// 从连接池中取连接失败
    POOL_GET_CONNECTION = 8,
    /// 无效的连接
    POOL_INVALID_CONNECTION = 9,
  };

  HttpResult(int _result, HttpResponse::ptr _response,
             const std::string& _error)
      : result(_result), response(_response), error(_error) {}

  /// 错误码
  int result;
  /// HTTP响应结构体
  HttpResponse::ptr response;
  /// 错误描述
  std::string error;

  std::string toString() const;
};

class HttpConnectionPool;

class HttpConnection : public SocketStream {
  friend class HttpConnectionPool;

 public:
  typedef std::shared_ptr<HttpConnection> ptr;

  static HttpResult::ptr DoGet(
      const std::string& url, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoGet(
      Uri::ptr uri, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoPost(
      const std::string& url, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoPost(
      Uri::ptr uri, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoRequest(
      HttpMethod method, const std::string& url, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoRequest(
      HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  static HttpResult::ptr DoRequest(HttpRequest::ptr req, Uri::ptr uri,
                                   uint64_t timeout_ms);

  static HttpResult::ptr DoRequest(HttpRequest::ptr req, Address::ptr addr,
                                   bool is_https, uint64_t timeout_ms);

  static HttpResult::ptr DoRequest(HttpRequest::ptr req, Socket::ptr sock,
                                   uint64_t timeout_ms);

  HttpConnection(Socket::ptr sock, bool owner = true);

  ~HttpConnection();

  HttpResponse::ptr recvResponse();
  int sendRequest(HttpRequest::ptr req);

 private:
  uint64_t m_createTime = 0;  // 创建时间
  uint64_t m_request = 0;     // 处理请求的次数
};

class HttpConnectionPool {
 public:
  typedef std::shared_ptr<HttpConnectionPool> ptr;
  typedef Mutex MutexType;

  static HttpConnectionPool::ptr Create(const std::string& uri,
                                        const std::string& vhost,
                                        uint32_t max_size,
                                        uint32_t max_alive_time,
                                        uint32_t max_request);

  HttpConnectionPool(const std::string& host, const std::string& vhost,
                     uint32_t port, bool is_https, uint32_t max_size,
                     uint32_t max_alive_time, uint32_t max_request);

  // 从 http 连接池中获取一个连接
  HttpConnection::ptr getConnection(uint64_t& timeout_ms);

  HttpResult::ptr doGet(const std::string& url, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers = {},
                        const std::string& body = "");

  HttpResult::ptr doGet(Uri::ptr uri, uint64_t timeout_ms,
                        const std::map<std::string, std::string>& headers = {},
                        const std::string& body = "");

  HttpResult::ptr doPost(const std::string& url, uint64_t timeout_ms,
                         const std::map<std::string, std::string>& headers = {},
                         const std::string& body = "");

  HttpResult::ptr doPost(Uri::ptr uri, uint64_t timeout_ms,
                         const std::map<std::string, std::string>& headers = {},
                         const std::string& body = "");

  HttpResult::ptr doRequest(
      HttpMethod method, const std::string& url, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  HttpResult::ptr doRequest(
      HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
      const std::map<std::string, std::string>& headers = {},
      const std::string& body = "");

  HttpResult::ptr doRequest(HttpRequest::ptr req, uint64_t timeout_ms);

 private:
  static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

 private:
  std::string m_host;
  std::string m_vhost;
  uint32_t m_port;          // 端口号
  uint32_t m_maxSize;       // 连接池最大连接数
  uint32_t m_maxAliveTime;  // 每个连接存活的时间
  uint32_t m_maxRequest;    // 每个连接最大请求次数
  bool m_isHttps;           // 是否 https
  std::string m_service;

  MutexType m_mutex;
  std::list<HttpConnection*> m_conns;   // 所有可用连接
  std::atomic<uint32_t> m_total = {0};  // 存活连接数
};

}  // namespace http
}  // namespace sylar

#endif
