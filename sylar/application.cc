#include <signal.h>
#include <unistd.h>

#include "sylar/application.h"
#include "sylar/config.h"
#include "sylar/daemon.h"
#include "sylar/db/fox_thread.h"
#include "sylar/db/redis.h"
#include "sylar/dns.h"
#include "sylar/env.h"
#include "sylar/http/ws_server.h"
#include "sylar/log.h"
#include "sylar/module.h"
#include "sylar/ns/name_server_module.h"
#include "sylar/rock/rock_server.h"
#include "sylar/rock/rock_stream.h"
#include "sylar/tcp_server.h"
#include "sylar/worker.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sylar::ConfigVar<std::string>::ptr g_server_work_path =
    sylar::Config::Lookup("server.work_path", std::string("/apps/work/sylar"),
                          "server work path");

static sylar::ConfigVar<std::string>::ptr g_server_pid_file =
    sylar::Config::Lookup("server.pid_file", std::string("sylar.pid"),
                          "server pid file");

static sylar::ConfigVar<std::string>::ptr g_service_discovery_zk =
    sylar::Config::Lookup("service_discovery.zk", std::string(""),
                          "service discovery zookeeper");

static sylar::ConfigVar<std::string>::ptr g_service_discovery_redis =
    sylar::Config::Lookup("service_discovery.redis.name", std::string(""),
                          "service discovery redis name");

static sylar::ConfigVar<std::vector<TcpServerConf>>::ptr g_servers_conf =
    sylar::Config::Lookup("servers", std::vector<TcpServerConf>(),
                          "http server config");

std::string GetServerWorkPath() {
  return g_server_work_path->getValue();
}

Application* Application::s_instance = nullptr;

Application::Application() {
  s_instance = this;
}

bool Application::init(int argc, char** argv) {
  m_argc = argc;
  m_argv = argv;

  sylar::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
  sylar::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
  sylar::EnvMgr::GetInstance()->addHelp("c", "conf path default: ./conf");
  sylar::EnvMgr::GetInstance()->addHelp("p", "print help");

  bool is_print_help = false;
  if (!sylar::EnvMgr::GetInstance()->init(argc, argv)) {
    is_print_help = true;
  }

  if (sylar::EnvMgr::GetInstance()->has("p")) {
    is_print_help = true;
  }

  std::string conf_path = sylar::EnvMgr::GetInstance()->getConfigPath();
  SYLAR_LOG_INFO(g_logger) << "load conf path:" << conf_path;
  sylar::Config::LoadFromConfDir(conf_path);

  ModuleMgr::GetInstance()->init();
  std::vector<Module::ptr> modules;
  ModuleMgr::GetInstance()->listAll(modules);

  for (auto i : modules) {
    i->onBeforeArgsParse(argc, argv);
  }

  if (is_print_help) {
    sylar::EnvMgr::GetInstance()->printHelp();
    return false;
  }

  for (auto i : modules) {
    i->onAfterArgsParse(argc, argv);
  }
  modules.clear();

  int run_type = 0;
  if (sylar::EnvMgr::GetInstance()->has("s")) {
    run_type = 1;
  }
  if (sylar::EnvMgr::GetInstance()->has("d")) {
    run_type = 2;
  }

  if (run_type == 0) {
    sylar::EnvMgr::GetInstance()->printHelp();
    return false;
  }

  std::string pidfile =
      g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
  if (sylar::FSUtil::IsRunningPidfile(pidfile)) {
    SYLAR_LOG_ERROR(g_logger) << "server is running:" << pidfile;
    return false;
  }

  if (!sylar::FSUtil::Mkdir(g_server_work_path->getValue())) {
    SYLAR_LOG_FATAL(g_logger)
        << "create work path [" << g_server_work_path->getValue()
        << " errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Application::run() {
  bool is_daemon = sylar::EnvMgr::GetInstance()->has("d");
  return start_daemon(m_argc, m_argv,
                      std::bind(&Application::main, this, std::placeholders::_1,
                                std::placeholders::_2),
                      is_daemon);
}

void sigproc(int sig) {
  SYLAR_LOG_INFO(g_logger) << "sigproc sig=" << sig;
  if (sig == SIGUSR1) {
    // 参考 nginx
    // linux下，当一个进程打开文件，
    // 日志归档时，文件被移动，由于进程没有结束，inode 节点引用计数任然 > 0，
    // 即使这个文件因为移动更换了目录，进程还是会继续写这个 inode，
    // 所以需要重新打开文件，当前目录会生成同名文件，会使用新的 inode 节点
    sylar::LoggerMgr::GetInstance()->reopen();
  }
}

void initSignal() {
  // 对一个已经关闭了的 socketfd 进行写操作，第一次会收到 rst
  // 第二次会直接退出进程，所以要忽略 SIGPIPE 信号
  signal(SIGPIPE, SIG_IGN);
  signal(SIGUSR1, sigproc);
}

int Application::main(int argc, char** argv) {
  initSignal();

  SYLAR_LOG_INFO(g_logger) << "main";
  std::string conf_path = sylar::EnvMgr::GetInstance()->getConfigPath();
  sylar::Config::LoadFromConfDir(conf_path, true);
  {
    std::string pidfile =
        g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
    std::ofstream ofs(pidfile);
    if (!ofs) {
      SYLAR_LOG_ERROR(g_logger) << "open pidfile " << pidfile << " failed";
      return false;
    }
    ofs << getpid();
  }

  m_mainIOManager = std::make_shared<sylar::IOManager>(1, true, "main");
  m_mainIOManager->schedule(std::bind(&Application::run_fiber, this));
  m_mainIOManager->addTimer(
      2000,
      [conf_path]() {
        // SYLAR_LOG_INFO(g_logger) << "hello";
        sylar::Config::LoadFromConfDir(conf_path);
      },
      true);
  m_mainIOManager->stop();
  return 0;
}

int Application::run_fiber() {
  sylar::WorkerMgr::GetInstance()->init();
  FoxThreadMgr::GetInstance()->init();
  FoxThreadMgr::GetInstance()->start();
  RedisMgr::GetInstance();
  DnsMgr::GetInstance()->init();
  DnsMgr::GetInstance()->start();
  std::vector<Module::ptr> modules;
  ModuleMgr::GetInstance()->listAll(modules);
  bool has_error = false;
  for (auto& i : modules) {
    if (!i->onLoad()) {
      SYLAR_LOG_ERROR(g_logger)
          << "module name=" << i->getName() << " version=" << i->getVersion()
          << " filename=" << i->getFilename();
      has_error = true;
    }
  }
  if (has_error) {
    _exit(0);
  }

  auto http_confs = g_servers_conf->getValue();
  std::vector<TcpServer::ptr> svrs;
  for (auto& i : http_confs) {
    SYLAR_LOG_DEBUG(g_logger) << std::endl
                              << LexicalCast<TcpServerConf, std::string>()(i);

    std::vector<Address::ptr> address;
    for (auto& a : i.address) {
      size_t pos = a.find(":");
      if (pos == std::string::npos) {
        // SYLAR_LOG_ERROR(g_logger) << "invalid address: " << a;
        address.push_back(std::make_shared<UnixAddress>(a));
        continue;
      }
      int32_t port = atoi(a.substr(pos + 1).c_str());
      // 127.0.0.1
      auto addr = sylar::IPAddress::Create(a.substr(0, pos).c_str(), port);
      if (addr) {
        address.push_back(addr);
        continue;
      }
      std::vector<std::pair<Address::ptr, uint32_t>> result;
      if (sylar::Address::GetInterfaceAddresses(result, a.substr(0, pos))) {
        for (auto& x : result) {
          auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
          if (ipaddr) {
            ipaddr->setPort(atoi(a.substr(pos + 1).c_str()));
          }
          address.push_back(ipaddr);
        }
        continue;
      }

      auto aaddr = sylar::Address::LookupAny(a);
      if (aaddr) {
        address.push_back(aaddr);
        continue;
      }
      SYLAR_LOG_ERROR(g_logger) << "invalid address: " << a;
      _exit(0);
    }
    IOManager* accept_worker = sylar::IOManager::GetThis();
    IOManager* io_worker = sylar::IOManager::GetThis();
    IOManager* process_worker = sylar::IOManager::GetThis();
    if (!i.accept_worker.empty()) {
      accept_worker = sylar::WorkerMgr::GetInstance()
                          ->getAsIOManager(i.accept_worker)
                          .get();
      if (!accept_worker) {
        SYLAR_LOG_ERROR(g_logger)
            << "accept_worker: " << i.accept_worker << " not exists";
        _exit(0);
      }
    }
    if (!i.io_worker.empty()) {
      io_worker =
          sylar::WorkerMgr::GetInstance()->getAsIOManager(i.io_worker).get();
      if (!io_worker) {
        SYLAR_LOG_ERROR(g_logger)
            << "io_worker: " << i.io_worker << " not exists";
        _exit(0);
      }
    }
    if (!i.process_worker.empty()) {
      process_worker = sylar::WorkerMgr::GetInstance()
                           ->getAsIOManager(i.process_worker)
                           .get();
      if (!process_worker) {
        SYLAR_LOG_ERROR(g_logger)
            << "process_worker: " << i.process_worker << " not exists";
        _exit(0);
      }
    }

    TcpServer::ptr server;
    if (i.type == "http") {
      server = std::make_shared<sylar::http::HttpServer>(
          i.keepalive, process_worker, io_worker, accept_worker);
    } else if (i.type == "ws") {
      server = std::make_shared<sylar::http::WSServer>(
          process_worker, io_worker, accept_worker);
    } else if (i.type == "rock") {
      server = std::make_shared<sylar::RockServer>("rock", process_worker,
                                                   io_worker, accept_worker);
    } else if (i.type == "nameserver") {
      server = std::make_shared<sylar::RockServer>("nameserver", process_worker,
                                                   io_worker, accept_worker);
      ModuleMgr::GetInstance()->add(
          std::make_shared<sylar::ns::NameServerModule>());
    } else {
      SYLAR_LOG_ERROR(g_logger) << "invalid server type=" << i.type
                                << LexicalCast<TcpServerConf, std::string>()(i);
      _exit(0);
    }
    if (!i.name.empty()) {
      server->setName(i.name);
    }
    std::vector<Address::ptr> fails;
    if (!server->bind(address, fails, i.ssl)) {
      for (auto& x : fails) {
        SYLAR_LOG_ERROR(g_logger) << "bind address fail:" << *x;
      }
      _exit(0);
    }
    if (i.ssl) {
      if (!server->loadCertificates(i.cert_file, i.key_file)) {
        SYLAR_LOG_ERROR(g_logger)
            << "loadCertificates fail, cert_file=" << i.cert_file
            << " key_file=" << i.key_file;
      }
    }
    server->setConf(i);
    // server->start();
    m_servers[i.type].push_back(server);
    svrs.push_back(server);
  }

  if (!g_service_discovery_zk->getValue().empty()) {
    m_serviceDiscovery = std::make_shared<ZKServiceDiscovery>(
        g_service_discovery_zk->getValue());
    m_rockSDLoadBalance =
        std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);

  } else if (!g_service_discovery_redis->getValue().empty()) {
    m_serviceDiscovery = std::make_shared<RedisServiceDiscovery>(
        g_service_discovery_redis->getValue());
    m_rockSDLoadBalance =
        std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
  }

  if (m_rockSDLoadBalance) {
    m_rockSDLoadBalance->start();
    sleep(1);
  }

  for (auto& i : modules) {
    i->onServerReady();
  }

  for (auto& i : svrs) {
    i->start();
  }

  for (auto& i : modules) {
    i->onServerUp();
  }

  if (m_rockSDLoadBalance) {
    m_rockSDLoadBalance->doRegister();
  }

  modules.clear();

  return 0;
}

void Application::initEnv() {
  sylar::WorkerMgr::GetInstance()->init();
  FoxThreadMgr::GetInstance()->init();
  FoxThreadMgr::GetInstance()->start();
  RedisMgr::GetInstance();
  DnsMgr::GetInstance()->init();
  DnsMgr::GetInstance()->start();

  if (!g_service_discovery_zk->getValue().empty()) {
    m_serviceDiscovery = std::make_shared<ZKServiceDiscovery>(
        g_service_discovery_zk->getValue());
    m_rockSDLoadBalance =
        std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
  } else if (!g_service_discovery_redis->getValue().empty()) {
    m_serviceDiscovery = std::make_shared<RedisServiceDiscovery>(
        g_service_discovery_redis->getValue());
    m_rockSDLoadBalance =
        std::make_shared<RockSDLoadBalance>(m_serviceDiscovery);
  }

  if (m_rockSDLoadBalance) {
    m_rockSDLoadBalance->start();
  }
}

bool Application::getServer(const std::string& type,
                            std::vector<TcpServer::ptr>& svrs) {
  auto it = m_servers.find(type);
  if (it == m_servers.end()) {
    return false;
  }
  svrs = it->second;
  return true;
}

void Application::listAllServer(
    std::map<std::string, std::vector<TcpServer::ptr>>& servers) {
  servers = m_servers;
}

}  // namespace sylar
