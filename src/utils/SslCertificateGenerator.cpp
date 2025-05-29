#include "utils/SslCertificateGenerator.h"
#include "utils/Logger.h"  

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/param_build.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/bn.h>  

#include <cstdio>  
#include <filesystem>  
#include <random>      
#include <fstream>     

namespace LT = LocalTether;

namespace LocalTether::Utils {

 
 
 
bool fileExists(const std::string& path) {
    if (std::filesystem::exists(path)) {  
        return true;
    }
     
    std::ifstream f(path.c_str());
    return f.good();
}


void SslCertificateGenerator::logOpenSslErrors(const std::string& contextMessage) {
    unsigned long errCode;
    char errBuf[256];
    LT::Utils::Logger::GetInstance().Error("OpenSSL Error in " + contextMessage + ":");
    while ((errCode = ERR_get_error()) != 0) {
        ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
        LT::Utils::Logger::GetInstance().Error(std::string("  - ") + errBuf);
    }
}

bool SslCertificateGenerator::generatePrivateKey(const std::string& keyPath, int bits) {
    LT::Utils::Logger::GetInstance().Info("Generating private key: " + keyPath);
    EVP_PKEY_CTX *pctx = nullptr;
    EVP_PKEY *pkey = nullptr;
    BIO *bio = nullptr;
    bool success = true;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) {
        logOpenSslErrors("EVP_PKEY_CTX_new_id for RSA in generatePrivateKey");
        success = false;
    }

    if (success) {
        if (EVP_PKEY_keygen_init(pctx) <= 0) {
            logOpenSslErrors("EVP_PKEY_keygen_init in generatePrivateKey");
            success = false;
        }
    }

    if (success) {
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, bits) <= 0) {
            logOpenSslErrors("EVP_PKEY_CTX_set_rsa_keygen_bits in generatePrivateKey");
            success = false;
        }
    }

    if (success) {
        if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
            logOpenSslErrors("EVP_PKEY_keygen in generatePrivateKey");
            success = false;
        }
    }

    if (success) {
        bio = BIO_new_file(keyPath.c_str(), "wb");
        if (!bio) {
            LT::Utils::Logger::GetInstance().Error("BIO_new_file failed in generatePrivateKey for: " + keyPath);
             
            success = false;
        }
    }

    if (success) {
        if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
            logOpenSslErrors("PEM_write_bio_PrivateKey in generatePrivateKey");
            success = false;
        }
    }

    if (success) {
        LT::Utils::Logger::GetInstance().Info("Private key generated successfully: " + keyPath);
    } else {
        LT::Utils::Logger::GetInstance().Error("Failed to generate private key: " + keyPath);
    }

    if (bio) BIO_free_all(bio);
    if (pkey) EVP_PKEY_free(pkey);
    if (pctx) EVP_PKEY_CTX_free(pctx);
    return success;
}

bool SslCertificateGenerator::generateCertificate(const std::string& certPath, const std::string& keyPath, int days) {
    LT::Utils::Logger::GetInstance().Info("Generating self-signed certificate: " + certPath);
    X509 *x509 = nullptr;
    EVP_PKEY *pkey = nullptr;
    BIO *key_bio = nullptr;
    BIO *cert_bio = nullptr;
    X509_NAME *name = nullptr;
    BIGNUM *bn_serial = nullptr;
    bool success = true;

    key_bio = BIO_new_file(keyPath.c_str(), "rb");
    if (!key_bio) {
        LT::Utils::Logger::GetInstance().Error("Failed to open private key file for reading: " + keyPath);
        success = false;
    }

    if (success) {
        pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
        if (!pkey) {
            logOpenSslErrors("PEM_read_bio_PrivateKey in generateCertificate");
            LT::Utils::Logger::GetInstance().Error("Failed to read private key: " + keyPath);
            success = false;
        }
    }

    if (success) {
        x509 = X509_new();
        if (!x509) {
            logOpenSslErrors("X509_new in generateCertificate");
            success = false;
        }
    }

    if (success) {
        if (X509_set_version(x509, 2L) != 1) { 
            logOpenSslErrors("X509_set_version in generateCertificate");
            success = false;
        }
    }

    if (success) {
        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t serial_number_val = dist(rng);
        if (serial_number_val == 0) serial_number_val = 1;

        ASN1_INTEGER *serial_asn1 = X509_get_serialNumber(x509);  
        bn_serial = BN_new();
         
        if (!bn_serial || !BN_set_word(bn_serial, serial_number_val)) {
            BN_free(bn_serial);  
            bn_serial = BN_new();  
             
             
            unsigned long smaller_serial = (unsigned long)(dist(rng) % 0xFFFFFFFFUL) + 1;
            if (!bn_serial || !BN_set_word(bn_serial, smaller_serial)) {
                logOpenSslErrors("BN_set_word for serial in generateCertificate");
                success = false;
            }
        }
        if (success && !BN_to_ASN1_INTEGER(bn_serial, serial_asn1)) {
            logOpenSslErrors("BN_to_ASN1_INTEGER for serial in generateCertificate");
            success = false;
        }
    }


    if (success) {
        if (!X509_gmtime_adj(X509_getm_notBefore(x509), 0)) {  
            logOpenSslErrors("X509_gmtime_adj for notBefore in generateCertificate");
            success = false;
        }
    }
    if (success) {
        if (!X509_gmtime_adj(X509_getm_notAfter(x509), (long)days * 24 * 60 * 60)) {  
            logOpenSslErrors("X509_gmtime_adj for notAfter in generateCertificate");
            success = false;
        }
    }

    if (success) {
        if (X509_set_pubkey(x509, pkey) != 1) {
            logOpenSslErrors("X509_set_pubkey in generateCertificate");
            success = false;
        }
    }

    if (success) {
        name = X509_get_subject_name(x509);
        if (!name) {
            logOpenSslErrors("X509_get_subject_name in generateCertificate");
            success = false;
        }
    }
    if (success) {
        if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"localhost", -1, -1, 0) != 1) {
            logOpenSslErrors("X509_NAME_add_entry_by_txt for CN in generateCertificate");
            success = false;
        }
    }
    if (success) {
        if (X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (const unsigned char *)"LocalTetherDev", -1, -1, 0) != 1) {
            logOpenSslErrors("X509_NAME_add_entry_by_txt for O in generateCertificate");
            success = false;
        }
    }
    if (success) {
        if (X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (const unsigned char *)"Development", -1, -1, 0) != 1) {
            logOpenSslErrors("X509_NAME_add_entry_by_txt for OU in generateCertificate");
            success = false;
        }
    }

    if (success) {  
        if (X509_set_issuer_name(x509, name) != 1) {
            logOpenSslErrors("X509_set_issuer_name in generateCertificate");
            success = false;
        }
    }

    if (success) {  
        if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
            logOpenSslErrors("X509_sign in generateCertificate");
            success = false;
        }
    }

    if (success) {
        cert_bio = BIO_new_file(certPath.c_str(), "wb");
        if (!cert_bio) {
            LT::Utils::Logger::GetInstance().Error("BIO_new_file failed for certificate in generateCertificate for: " + certPath);
            success = false;
        }
    }
    if (success) {
        if (PEM_write_bio_X509(cert_bio, x509) != 1) {
            logOpenSslErrors("PEM_write_bio_X509 in generateCertificate");
            success = false;
        }
    }

    if (success) {
        LT::Utils::Logger::GetInstance().Info("Self-signed certificate generated successfully: " + certPath);
    } else {
        LT::Utils::Logger::GetInstance().Error("Failed to generate certificate: " + certPath);
    }

    if (bn_serial) BN_free(bn_serial);
    if (cert_bio) BIO_free_all(cert_bio);
    if (key_bio) BIO_free_all(key_bio);
    if (pkey) EVP_PKEY_free(pkey);
    if (x509) X509_free(x509);
    return success;
}

bool SslCertificateGenerator::generateDhParams(const std::string& dhParamsPath, int bits) {
    LT::Utils::Logger::GetInstance().Info("Generating DH parameters using EVP_PKEY API: " + dhParamsPath + " (this may take a moment)...");
    EVP_PKEY *pkey_params = nullptr;
    EVP_PKEY_CTX *pctx = nullptr;
    BIO *bio = nullptr;
    bool success = true;

     
     
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, nullptr);
    if (!pctx) {
        logOpenSslErrors("EVP_PKEY_CTX_new_id for DH in generateDhParams");
        success = false;
    }

    if (success) {
         
        if (EVP_PKEY_paramgen_init(pctx) <= 0) {
            logOpenSslErrors("EVP_PKEY_paramgen_init in generateDhParams");
            success = false;
        }
    }

    if (success) {
         
        if (EVP_PKEY_CTX_set_dh_paramgen_prime_len(pctx, bits) <= 0) {
            logOpenSslErrors("EVP_PKEY_CTX_set_dh_paramgen_prime_len in generateDhParams");
            success = false;
        }
    }
    
    if (success) {
         
         
        if (EVP_PKEY_CTX_set_dh_paramgen_generator(pctx, DH_GENERATOR_2) <= 0) {
            logOpenSslErrors("EVP_PKEY_CTX_set_dh_paramgen_generator in generateDhParams");
            success = false;
        }
    }

    if (success) {
         
        if (EVP_PKEY_paramgen(pctx, &pkey_params) <= 0) {
            logOpenSslErrors("EVP_PKEY_paramgen in generateDhParams");
            success = false;
        }
    }

     
     
     
     
     

    if (success) {
        bio = BIO_new_file(dhParamsPath.c_str(), "wb");
        if (!bio) {
            LT::Utils::Logger::GetInstance().Error("BIO_new_file failed for DH params in generateDhParams for: " + dhParamsPath);
            success = false;
        }
    }
    if (success) {
         
         
        if (PEM_write_bio_Parameters(bio, pkey_params) != 1) {
            logOpenSslErrors("PEM_write_bio_Parameters in generateDhParams");
            success = false;
        }
    }

    if (success) {
        LT::Utils::Logger::GetInstance().Info("DH parameters generated successfully: " + dhParamsPath);
    } else {
        LT::Utils::Logger::GetInstance().Error("Failed to generate DH parameters: " + dhParamsPath);
    }

    if (bio) BIO_free_all(bio);
    if (pkey_params) EVP_PKEY_free(pkey_params);
    if (pctx) EVP_PKEY_CTX_free(pctx);
    return success;
}


bool SslCertificateGenerator::EnsureSslFiles(
    const std::string& keyPath,
    const std::string& certPath,
    const std::string& dhParamsPath) {

    bool keyOk = fileExists(keyPath);
    bool certOk = fileExists(certPath);
    bool dhOk = fileExists(dhParamsPath);

    if (keyOk && certOk && dhOk) {
        LT::Utils::Logger::GetInstance().Info("All SSL files (key, cert, dhparams) already exist.");
        return true;
    }

    if (!keyOk) {
        if (!generatePrivateKey(keyPath)) {
            return false;
        }
    } else {
        LT::Utils::Logger::GetInstance().Info("Private key file already exists: " + keyPath);
    }

     
    keyOk = fileExists(keyPath); 
    if (!keyOk) {
        LT::Utils::Logger::GetInstance().Error("Private key still missing after generation attempt. Cannot generate certificate.");
        return false;
    }

    if (!certOk) {
        if (!generateCertificate(certPath, keyPath)) {
            return false;
        }
    } else {
        LT::Utils::Logger::GetInstance().Info("Certificate file already exists: " + certPath);
    }

    if (!dhOk) {
        if (!generateDhParams(dhParamsPath)) {
            return false;
        }
    } else {
        LT::Utils::Logger::GetInstance().Info("DH parameters file already exists: " + dhParamsPath);
    }

     
    return fileExists(keyPath) && fileExists(certPath) && fileExists(dhParamsPath);
}

}  