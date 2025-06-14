#include "http_server.h"
#include "sylar/http/servlets/config_servlet.h"
#include "sylar/http/servlets/status_servlet.h"
#include "sylar/log.h"

namespace sylar {
namespace http {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, sylar::IOManager* worker,
                       sylar::IOManager* io_worker,
                       sylar::IOManager* accept_worker)
    : TcpServer(worker, io_worker, accept_worker), m_isKeepalive(keepalive) {
  m_dispatch = std::make_shared<ServletDispatch>();

  m_type = "http";
  m_dispatch->addServlet("/_/status", std::make_shared<StatusServlet>());
  m_dispatch->addServlet("/_/config", std::make_shared<ConfigServlet>());
}

void HttpServer::setName(const std::string& v) {
  TcpServer::setName(v);
  m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
  SYLAR_LOG_DEBUG(g_logger) << "handleClient " << *client;
  HttpSession::ptr session = std::make_shared<HttpSession>(client);
  do {
    auto req = session->recvRequest();
    if (!req) {
      SYLAR_LOG_DEBUG(g_logger)
          << "recv http request fail, errno=" << errno
          << " errstr=" << strerror(errno) << " cliet:" << *client
          << " keep_alive=" << m_isKeepalive;
      break;
    }

    HttpResponse::ptr rsp = std::make_shared<HttpResponse>(
        req->getVersion(), req->isClose() || !m_isKeepalive);
    rsp->setHeader("Server", getName());
    rsp->setHeader("Content-Type", "application/json;charset=utf8");
    {
      // 注意这里没有新建协程对象 !!!
      // 切换IO协程调度器到worker协程调度器，这里使用的是同一个协程对象
      sylar::SchedulerSwitcher sw(m_worker);
      // worker协程调度器继续调度执行这个协程
      m_dispatch->handle(req, rsp, session);
      // 当SchedulerSwitcher析构时，会自动切换回原来的协程调度器
    }
    session->sendResponse(rsp);

    if (!m_isKeepalive || req->isClose()) {
      break;
    }
  } while (true);
  session->close();
}

}  // namespace http
}  // namespace sylar
