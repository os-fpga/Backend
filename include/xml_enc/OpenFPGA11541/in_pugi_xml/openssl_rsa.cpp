#include <iostream>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "obfuscate.h"
#include "pugixml_util.hpp"
#define KEY_LENGTH       2048
#define PUBLIC_EXPONENT  59     //Public exponent should be a prime number .

using namespace std;

int private_decrypt(int flen, unsigned char* from, unsigned char* to, RSA* key, int padding) {
    int result = RSA_private_decrypt(flen, from, to, key, padding);
    return result;
}


char* decrypt(std::string bin_file){


   // RSA *private_key;
    RSA *public_key;
    char *encrypt = NULL;
    char *decrypt = NULL;
    RSA *keypair = NULL;
    BIGNUM *bne = NULL;
    int ret = 0;
    keypair = RSA_new();
    // The function AY_OBFUSCATE will make the key obfuscated in object file.
    // This char array will not appear directly in char file. It appears as encrypted binary
   const char *private_key_=AY_OBFUSCATE( "-----BEGIN RSA PRIVATE KEY-----\n"
                                "MIIEogIBAAKCAQEAyBJdPfoRZgH7JZtP1LBRBILEOM/x/h2g1389DvPvbcDcYgwi\n"
                                "AiTBNjqwUIqGw9dQUt2ReArzvdoz77Zp06569defw8RvFgxh9/G2YkRqf6rBW2mU\n"
                                "zQlLlbA10R6w/io687O1Dm3LSpn2YHa8ZuymL1OZVch5600gxp5du0L6WkxaqkQx\n"
                                "mdLmOTtfSxQn1x2SQh8w7ciuTncAhTOz9ma6OCj2ybggDQv+WVaA6MZAu1vmQxaa\n"
                                "c4xqbgqRutQxn8wvaw2Bwe5EK5PnuOTKqdrwdhh1iCXV2+E53XqUWaMhwlPW1owM\n"
                                "uJhBHJ/uOMcJgiyC7p14rQeHmXcJCvehpfP5gQIBOwKCAQEAqY1ktrE1y5mLEtpm\n"
                                "XXcMQJGH6rjnHMarBLnuUhh8zdMrlCz+cqFRTFRytRGZQiufMIezr35vGmIjVf5i\n"
                                "XJyK6l/z3k+wlNrD1nYPtxLtx0+fhd/77tgmBVzshdSV/nHj24tG/zX+uLaLXsQU\n"
                                "0Lcxt0svgRq1bEoKa5M5vRG1ylk+DBI1zlLbOzD7TAZxoJrdKoftbKpSUakaK2oa\n"
                                "CtTeUS/KOm6hnpwL3FCy7LdQw1i2cgE6+6AQCI+1eO3jZQbgasPt+wSnleHKlHkk\n"
                                "tdW05KOXb7hNS9ZXrpK7FAobMEejbBc7gOEikI8HYbLN3fJwKABP7xOa/kfWNo6/\n"
                                "6ioXEwKBgQDkc2HEOwFfT0DqUr//buTRicgy1cRNyA1j+hcfePw0peFI+KOtceyz\n"
                                "sL9ygTydMmRZt+eSyQzMszSwesC5tPv3205h4qfBJGQfWJd642nbb8HQYB3DeKsM\n"
                                "u8V+BOSBD5eS6lPMXT3PXXAyMcQn0SFP6aZoMi63JPpf2OHyBO6x7QKBgQDgMuFr\n"
                                "UcGtLl9g62tGKrU2n8DWkqwJoO7e8XuKPW9WRvWv089XvB96GmDKoiH3b0IRaw7b\n"
                                "qopsFyIQO56Zq+vxTzQWakYjrV2rQHausm/Jz5Wba+HLqHGSd40QnmDIS69pfAhb\n"
                                "bDxOFJtwwc6c2TnsHXGTcBi4SG+S0vyZ+DoDZQKBgH/HC0sLTt6HcmkHO6jnPuYD\n"
                                "TUN750ngtQyHisOAaloxY/kV6rwhWQC1TLUylw4pM8oLvkUVVULDsP7piisfzgQf\n"
                                "i0xxwaAYsX4B0o5/MofArX06GVNMK5qlxT3OqzJ936BGUZUEbFWcZc35H5zHaWlx\n"
                                "VGWrQS4HqmmtYADbtTx3AoGAKcy9jYAOagz8FmijL8s3ev9pYGlyg/b0IONCbDbY\n"
                                "AxGRmkXc5PevFsPVTNR3JXQzXl25Aedjkfuijd+5xd736+e3RUOK6EdfkL3qvMYm\n"
                                "MqSIXg/GTQVe7+rmO4FTIQEYBqZPqOjKKJdeEK77IZT5cXH2zWMARRHYDltngIC4\n"
                                "YBcCgYEAqo93SQDw4SeeqtNfdmp3QAGAw4AfM7aQ/tsTK3HV8qhohzKQRA+PGi61\n"
                                "FHVwlnX+f+yITKPeBWQkrxqlW48w0EKG444LnZbA/ma3tbXSXZPkbFYxZEXnGAh8\n"
                                "e/SlpJN+m8Lf/Zf41Lh8JCrKNqg7aMxd+unKTx38Wa2wjIOr7yc=\n"
                                "-----END RSA PRIVATE KEY-----\n");
std::string rsa(private_key_);
    BIO *keybio = BIO_new_mem_buf((char*) rsa.data(), -1) ;
    if (keybio == NULL) {
        throw pugiutil::XmlError("Error: unable to read");
    }
    RSA *private_key = PEM_read_bio_RSAPrivateKey(keybio, NULL, NULL, NULL) ;
    if (!private_key) {
        throw pugiutil::XmlError("Error: unable to read");
    }
   encrypt=(char *)malloc(KEY_LENGTH);

   std::string result = bin_file + "/vpr_e.bin";
   const char * c_result = result.c_str();
   FILE* encrypted_file = fopen(c_result, "r");
   if( encrypted_file== NULL) {
         throw pugiutil::XmlError("Unable to read vpr_e.bin  ");
    }


    int number = fread(encrypt, 1, KEY_LENGTH, encrypted_file);
     cout<<number<<endl;
    fclose(encrypted_file);
    decrypt = (char *)malloc(KEY_LENGTH);
    int decrypt_length = private_decrypt(number, (unsigned char*)encrypt, (unsigned char*)decrypt, private_key, RSA_PKCS1_OAEP_PADDING);
    if(decrypt_length == -1) {
         throw pugiutil::XmlError("An error occurred in reading .xmle ");
    }
return decrypt;
}
