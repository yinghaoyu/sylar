#include "crypto_util.h"
#include <stdio.h>
#include <iostream>

namespace sylar {

namespace {

// struct CryptoInit {
//     CryptoInit() {
//         OpenSSL_add_all_ciphers();
//     }
// };

}

int32_t CryptoUtil::AES256Ecb(const void* key, const void* in, int32_t in_len,
                              void* out, bool encode) {
  int32_t len = 0;
  return Crypto(EVP_aes_256_ecb(), encode, (const uint8_t*)key, nullptr,
                (const uint8_t*)in, in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::AES128Ecb(const void* key, const void* in, int32_t in_len,
                              void* out, bool encode) {
  int32_t len = 0;
  return Crypto(EVP_aes_128_ecb(), encode, (const uint8_t*)key, nullptr,
                (const uint8_t*)in, in_len, (uint8_t*)out, &len);
}

int32_t CryptoUtil::AES256Cbc(const void* key, const void* iv, const void* in,
                              int32_t in_len, void* out, bool encode) {
  int32_t len = 0;
  return Crypto(EVP_aes_256_cbc(), encode, (const uint8_t*)key,
                (const uint8_t*)iv, (const uint8_t*)in, in_len, (uint8_t*)out,
                &len);
}

int32_t CryptoUtil::AES128Cbc(const void* key, const void* iv, const void* in,
                              int32_t in_len, void* out, bool encode) {
  int32_t len = 0;
  return Crypto(EVP_aes_128_cbc(), encode, (const uint8_t*)key,
                (const uint8_t*)iv, (const uint8_t*)in, in_len, (uint8_t*)out,
                &len);
}

int32_t CryptoUtil::Crypto(const EVP_CIPHER* cipher, bool enc, const void* key,
                           const void* iv, const void* in, int32_t in_len,
                           void* out, int32_t* out_len) {
  int tmp_len = 0;
  bool has_error = false;
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  do {
    // static CryptoInit s_crypto_init;
    EVP_CIPHER_CTX_init(ctx);
    EVP_CipherInit_ex(ctx, cipher, nullptr, (const uint8_t*)key,
                      (const uint8_t*)iv, enc);
    if (EVP_CipherUpdate(ctx, (uint8_t*)out, &tmp_len, (const uint8_t*)in,
                         in_len) != 1) {
      has_error = true;
      break;
    }
    *out_len = tmp_len;
    if (EVP_CipherFinal_ex(ctx, (uint8_t*)out + tmp_len, &tmp_len) != 1) {
      has_error = true;
      break;
    }
    *out_len += tmp_len;
  } while (0);
  EVP_CIPHER_CTX_cleanup(ctx);
  if (has_error) {
    return -1;
  }
  return *out_len;
}

int32_t RSACipher::GenerateKey(const std::string& pubkey_file,
                               const std::string& prikey_file,
                               uint32_t length) {
  int rt = 0;
  FILE* fp = nullptr;
  EVP_PKEY_CTX* ctx = nullptr;
  EVP_PKEY* pkey = nullptr;
  do {
    // 创建EVP_PKEY_CTX上下文对象
    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
      rt = -1;
      break;
    }

    // 初始化密钥生成操作
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
      rt = -2;
      break;
    }

    // 设置密钥长度
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, length) <= 0) {
      rt = -3;
      break;
    }

    // 执行密钥生成操作
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
      rt = -4;
      break;
    }

    // 写入公钥文件
    fp = fopen(pubkey_file.c_str(), "w+");
    if (!fp) {
      rt = -6;
      break;
    }
    PEM_write_PUBKEY(fp, pkey);
    fclose(fp);
    fp = nullptr;

    // 写入私钥文件
    fp = fopen(prikey_file.c_str(), "w+");
    if (!fp) {
      rt = -7;
      break;
    }
    PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(fp);
    fp = nullptr;

  } while (false);
  if (fp) {
    fclose(fp);
  }
  if (pkey) {
    EVP_PKEY_free(pkey);
  }
  if (ctx) {
    EVP_PKEY_CTX_free(ctx);
  }
  return rt;
}

RSACipher::ptr RSACipher::Create(const std::string& pubkey_file,
                                 const std::string& prikey_file) {
  FILE* fp = nullptr;
  do {
    RSACipher::ptr rt = std::make_shared<RSACipher>();
    fp = fopen(pubkey_file.c_str(), "r+");
    if (!fp) {
      break;
    }
    rt->m_pubkey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
    if (!rt->m_pubkey) {
      break;
    }

    EVP_PKEY_print_public_fp(stdout, rt->m_pubkey, 0, nullptr);
    std::cout << "====" << std::endl;

    std::string tmp;
    tmp.resize(1024);

    int len = 0;
    fseek(fp, 0, 0);
    do {
      len = fread(&tmp[0], 1, tmp.size(), fp);
      if (len > 0) {
        rt->m_pubkeyStr.append(tmp.c_str(), len);
      }
    } while (len > 0);
    fclose(fp);
    fp = nullptr;

    fp = fopen(prikey_file.c_str(), "r+");
    if (!fp) {
      break;
    }
    rt->m_prikey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    if (!rt->m_prikey) {
      break;
    }

    EVP_PKEY_print_private_fp(stdout, rt->m_prikey, 0, nullptr);
    std::cout << "====" << std::endl;
    fseek(fp, 0, 0);
    do {
      len = fread(&tmp[0], 1, tmp.size(), fp);
      if (len > 0) {
        rt->m_prikeyStr.append(tmp.c_str(), len);
      }
    } while (len > 0);
    fclose(fp);
    fp = nullptr;
    return rt;
  } while (false);
  if (fp) {
    fclose(fp);
  }
  return nullptr;
}

RSACipher::RSACipher() : m_pubkey(nullptr), m_prikey(nullptr) {}

RSACipher::~RSACipher() {
  if (m_pubkey) {
    EVP_PKEY_free(m_pubkey);
  }
  if (m_prikey) {
    EVP_PKEY_free(m_prikey);
  }
}

int32_t RSACipher::privateEncrypt(const void* from, int flen, void* to,
                                  int padding) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_prikey, nullptr);
  if (!ctx || EVP_PKEY_encrypt_init(ctx) <= 0) {
    return -1;
  }

  // 设置填充模式
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, padding) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  size_t outlen = 0;
  if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, (const unsigned char*)from,
                       flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  if (EVP_PKEY_encrypt(ctx, (unsigned char*)to, &outlen,
                       (const unsigned char*)from, flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  EVP_PKEY_CTX_free(ctx);
  return outlen;
}

int32_t RSACipher::publicEncrypt(const void* from, int flen, void* to,
                                 int padding) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_pubkey, nullptr);
  if (!ctx || EVP_PKEY_encrypt_init(ctx) <= 0) {
    return -1;
  }

  // 设置填充模式
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, padding) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  size_t outlen = 0;
  if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, (const unsigned char*)from,
                       flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  if (EVP_PKEY_encrypt(ctx, (unsigned char*)to, &outlen,
                       (const unsigned char*)from, flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  EVP_PKEY_CTX_free(ctx);
  return outlen;
}

int32_t RSACipher::privateDecrypt(const void* from, int flen, void* to,
                                  int padding) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_prikey, nullptr);
  if (!ctx || EVP_PKEY_decrypt_init(ctx) <= 0) {
    return -1;
  }

  // 设置填充模式
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, padding) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  size_t outlen = 0;
  if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, (const unsigned char*)from,
                       flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  if (EVP_PKEY_decrypt(ctx, (unsigned char*)to, &outlen,
                       (const unsigned char*)from, flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  EVP_PKEY_CTX_free(ctx);
  return outlen;
}

int32_t RSACipher::publicDecrypt(const void* from, int flen, void* to,
                                 int padding) {
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_pubkey, nullptr);
  if (!ctx || EVP_PKEY_decrypt_init(ctx) <= 0) {
    return -1;
  }

  // 设置填充模式
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, padding) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  size_t outlen = 0;
  if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, (const unsigned char*)from,
                       flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  if (EVP_PKEY_decrypt(ctx, (unsigned char*)to, &outlen,
                       (const unsigned char*)from, flen) <= 0) {
    EVP_PKEY_CTX_free(ctx);
    return -1;
  }

  EVP_PKEY_CTX_free(ctx);
  return outlen;
}

int32_t RSACipher::privateEncrypt(const void* from, int flen, std::string& to,
                                  int padding) {
  // TODO resize
  int32_t len = privateEncrypt(from, flen, &to[0], padding);
  if (len >= 0) {
    to.resize(len);
  }
  return len;
}

int32_t RSACipher::publicEncrypt(const void* from, int flen, std::string& to,
                                 int padding) {
  // TODO resize
  int32_t len = publicEncrypt(from, flen, &to[0], padding);
  if (len >= 0) {
    to.resize(len);
  }
  return len;
}

int32_t RSACipher::privateDecrypt(const void* from, int flen, std::string& to,
                                  int padding) {
  // TODO resize
  int32_t len = privateDecrypt(from, flen, &to[0], padding);
  if (len >= 0) {
    to.resize(len);
  }
  return len;
}

int32_t RSACipher::publicDecrypt(const void* from, int flen, std::string& to,
                                 int padding) {
  // TODO resize
  int32_t len = publicDecrypt(from, flen, &to[0], padding);
  if (len >= 0) {
    to.resize(len);
  }
  return len;
}

int32_t RSACipher::getPubRSASize() {
  return m_pubkey ? EVP_PKEY_size(m_pubkey) : -1;
}

int32_t RSACipher::getPriRSASize() {
  return m_prikey ? EVP_PKEY_size(m_prikey) : -1;
}

}  // namespace sylar
