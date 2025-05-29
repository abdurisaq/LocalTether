#ifndef SSL_CERTIFICATE_GENERATOR_H
#define SSL_CERTIFICATE_GENERATOR_H

#include <string>

namespace LocalTether::Utils {

class SslCertificateGenerator {
public:
    
    static bool EnsureSslFiles(
        const std::string& keyPath = "server.key",
        const std::string& certPath = "server.crt",
        const std::string& dhParamsPath = "dh.pem");

private:
    static bool generatePrivateKey(const std::string& keyPath, int bits = 2048);
    static bool generateCertificate(const std::string& certPath, const std::string& keyPath, int days = 365);
    static bool generateDhParams(const std::string& dhParamsPath, int bits = 2048);
    static void logOpenSslErrors(const std::string& contextMessage);
};

} 

#endif 