#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sylar {

class IPAddress;
class Address {
 public:
  typedef std::shared_ptr<Address> ptr;

  // 创建Address
  // addr sockaddr指针
  // arrlen sockaddr的长度
  static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

  // 通过host地址返回对应条件的所有Address
  // result 保存满足条件的Address
  // host 域名,服务器名等.举例: www.google.com[:80] (方括号为可选内容)
  // family 协议族(AF_INT, AF_INT6, AF_UNIX)
  // type socketl类型SOCK_STREAM、SOCK_DGRAM 等
  // protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
  // 返回是否转换成功
  static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family = AF_INET, int type = 0, int protocol = 0);

  static Address::ptr LookupAny(const std::string& host, int family = AF_INET,
                                int type = 0, int protocol = 0);

  static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                                                       int family = AF_INET,
                                                       int type = 0,
                                                       int protocol = 0);

  // 返回本机所有网卡的<网卡名, 地址, 子网掩码位数>
  // result 保存本机所有地址
  // family 协议族(AF_INT, AF_INT6, AF_UNIX)
  // 返回是否获取成功
  static bool GetInterfaceAddresses(
      std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
      int family = AF_INET);

  // 获取指定网卡的地址和子网掩码位数
  // result 保存指定网卡所有地址
  // iface 网卡名称
  // family 协议族(AF_INT, AF_INT6, AF_UNIX)
  //  返回是否获取成功
  static bool GetInterfaceAddresses(
      std::vector<std::pair<Address::ptr, uint32_t>>& result,
      const std::string& iface, int family = AF_INET);

  virtual ~Address() {}

  int getFamily() const;

  virtual const sockaddr* getAddr() const = 0;
  virtual sockaddr* getAddr() = 0;
  virtual socklen_t getAddrLen() const = 0;

  virtual std::ostream& insert(std::ostream& os) const = 0;
  std::string toString() const;

  bool operator<(const Address& rhs) const;
  bool operator==(const Address& rhs) const;
  bool operator!=(const Address& rhs) const;
};

class IPAddress : public Address {
 public:
  typedef std::shared_ptr<IPAddress> ptr;

  static IPAddress::ptr Create(const char* address, uint16_t port = 0);

  //获取该地址的广播地址
  // prefix_len 子网掩码位数
  //调用成功返回IPAddress,失败返回nullptr
  virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;

  // 获取该地址的网段
  // prefix_len 子网掩码位数
  //调用成功返回IPAddress,失败返回nullptr
  virtual IPAddress::ptr networdAddress(uint32_t prefix_len) = 0;

  //获取子网掩码地址
  // prefix_len 子网掩码位数
  // 调用成功返回IPAddress,失败返回nullptr
  virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

  virtual uint32_t getPort() const = 0;
  virtual void setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
 public:
  typedef std::shared_ptr<IPv4Address> ptr;

  // 使用点分十进制地址创建IPv4Address
  static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

  // 通过sockaddr_in构造IPv4Address
  IPv4Address(const sockaddr_in& address);

  // 通过二进制地址构造IPv4Address
  IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  std::ostream& insert(std::ostream& os) const override;

  IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
  IPAddress::ptr networdAddress(uint32_t prefix_len) override;
  IPAddress::ptr subnetMask(uint32_t prefix_len) override;
  uint32_t getPort() const override;
  void setPort(uint16_t v) override;

 private:
  sockaddr_in m_addr;
};

class IPv6Address : public IPAddress {
 public:
  typedef std::shared_ptr<IPv6Address> ptr;

  // 通过IPv6地址字符串构造IPv6Address
  static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

  IPv6Address();

  // 通过sockaddr_in6构造IPv6Address
  IPv6Address(const sockaddr_in6& address);

  // 通过IPv6二进制地址构造IPv6Address
  IPv6Address(const uint8_t address[16], uint16_t port = 0);

  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  std::ostream& insert(std::ostream& os) const override;

  IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
  IPAddress::ptr networdAddress(uint32_t prefix_len) override;
  IPAddress::ptr subnetMask(uint32_t prefix_len) override;
  uint32_t getPort() const override;
  void setPort(uint16_t v) override;

 private:
  sockaddr_in6 m_addr;
};

class UnixAddress : public Address {
 public:
  typedef std::shared_ptr<UnixAddress> ptr;
  UnixAddress();

  // 通过路径构造UnixAddress
  UnixAddress(const std::string& path);

  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  void setAddrLen(uint32_t v);
  std::string getPath() const;
  std::ostream& insert(std::ostream& os) const override;

 private:
  sockaddr_un m_addr;
  socklen_t m_length;
};

class UnknownAddress : public Address {
 public:
  typedef std::shared_ptr<UnknownAddress> ptr;
  UnknownAddress(int family);
  UnknownAddress(const sockaddr& addr);
  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  std::ostream& insert(std::ostream& os) const override;

 private:
  sockaddr m_addr;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);

}  // namespace sylar

#endif
