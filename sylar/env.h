#ifndef __SYLAR_ENV_H__
#define __SYLAR_ENV_H__

#include <map>
#include <vector>
#include "sylar/singleton.h"
#include "sylar/thread.h"

namespace sylar {

class Env {
 public:
  typedef RWMutex RWMutexType;
  bool init(int argc, char** argv);

  void add(const std::string& key, const std::string& val);
  bool has(const std::string& key);
  void del(const std::string& key);
  std::string get(const std::string& key,
                  const std::string& default_value = "");

  void addHelp(const std::string& key, const std::string& desc);
  void removeHelp(const std::string& key);
  void printHelp();

  const std::string& getExe() const { return m_exe; }
  const std::string& getCwd() const { return m_cwd; }

  bool setEnv(const std::string& key, const std::string& val);
  std::string getEnv(const std::string& key,
                     const std::string& default_value = "");

  std::string getAbsolutePath(const std::string& path) const;
  std::string getAbsoluteWorkPath(const std::string& path) const;

  std::string getConfigPath();

 private:
  RWMutexType m_mutex;
  std::map<std::string, std::string> m_args;                 // 参数列表
  std::vector<std::pair<std::string, std::string>> m_helps;  // 帮助列表

  std::string m_program;  // 当前程序名
  std::string m_exe;      // 当前程序的绝对路径
  std::string m_cwd;      // 当前程序的工作目录
};

typedef sylar::Singleton<Env> EnvMgr;

}  // namespace sylar

#endif
