#include <iostream>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include"obfuscate.h"
#define KEY_LENGTH       2048
#define PUBLIC_EXPONENT  59     //Public exponent should be a prime number.
#define PUBLIC_KEY_PEM   1
#define PRIVATE_KEY_PEM  0

#define LOG(x)               \
        cout << x << endl;   \

using namespace std;


void write_RSA(RSA * keypair, int pem_type, char *file_name) {

    RSA   *rsa = NULL;
    BIO  *fp  = NULL;

    if(pem_type == PUBLIC_KEY_PEM) {
        fp = BIO_new_file(file_name, "w");
        PEM_write_bio_RSAPublicKey(fp, keypair);
        BIO_free(fp);
    }
    else if(pem_type == PRIVATE_KEY_PEM) {
        fp = BIO_new_file(file_name, "w");
        PEM_write_bio_RSAPrivateKey(fp, keypair, NULL, NULL, NULL, NULL, NULL);
        BIO_free(fp);  
    }    
}

RSA * read_RSA(RSA * keypair, int pem_type, char *file_name) {

    RSA   *rsa = NULL;
    BIO  *fp  = NULL;

    if(pem_type == PUBLIC_KEY_PEM) {
        fp = BIO_new_file(file_name, "rb");
        PEM_read_bio_RSAPublicKey(fp, &rsa, NULL, NULL);
        BIO_free(fp);

    }
    else if(pem_type == PRIVATE_KEY_PEM) {
    	fp = BIO_new_file(file_name, "rb");
        PEM_read_bio_RSAPrivateKey(fp, &rsa, NULL, NULL);
        BIO_free(fp);

    }

    return rsa;
}

int public_encrypt(int flen, unsigned char* from, unsigned char* to, RSA* key, int padding) {
    
    int result = RSA_public_encrypt(flen, from, to, key, padding);
    return result;
}

int private_decrypt(int flen, unsigned char* from, unsigned char* to, RSA* key, int padding) {

    int result = RSA_private_decrypt(flen, from, to, key, padding);
    return result;
}

void create_encrypted_file(char* encrypted, RSA* key_pair) {

    FILE* encrypted_file = fopen("encrypted_file.bin", "w");
    fwrite(encrypted, sizeof(*encrypted), RSA_size(key_pair), encrypted_file);
    fclose(encrypted_file);
}

char* decrypt(){
LOG("decryption has been started.");
    
   // RSA *private_key;
    RSA *public_key;

    //char message[KEY_LENGTH / 8] = "Plain text";
    char *encrypt = NULL;
    char *decrypt = NULL;

    RSA *keypair = NULL;
    BIGNUM *bne = NULL;
    int ret = 0;

    char private_key_pem[12] = "private_key";
    keypair = RSA_new();
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
        std::cerr << "Error: unable to create key bio" << std::endl ;

    }
    RSA *private_key = PEM_read_bio_RSAPrivateKey(keybio, NULL, NULL, NULL) ;
    if (!private_key) {
        std::cerr << "Error: unable to parse private key" << std::endl ;
    }

    //cout<<(rsa)<<endl;

   int m_size=(KEY_LENGTH + KEY_LENGTH+ KEY_LENGTH+ KEY_LENGTH);
   encrypt=(char *)malloc(m_size);
   FILE* encrypted_file = fopen("encrypted_file.bin", "r");
    int number = fread(encrypt, 1, m_size, encrypted_file);
     cout<<number<<endl;
    fclose(encrypted_file);
   // cout<<encrypt<<endl;
  // cout<<(sizeof (encrypt))<<endl;
    //private_key = read_RSA(keypair, PRIVATE_KEY_PEM, private_key_pem);
    decrypt = (char *)malloc(m_size);
    int decrypt_length = private_decrypt(number, (unsigned char*)encrypt, (unsigned char*)decrypt, private_key, RSA_PKCS1_OAEP_PADDING);
    if(decrypt_length == -1) {
        LOG("An error occurred in private_decrypt() method");
    }
    LOG("Data has been decrypted.");
/*
    FILE *decrypted_file = fopen("decrypted_file.txt", "w");
    fwrite(decrypt, sizeof(*decrypt), decrypt_length - 1, decrypted_file);
    fclose(decrypted_file);
    LOG("Decrypted file has been created.");*/
    return decrypt;
    
}




void Encrypt ( ){
    LOG("Encryption has been started.");
    RSA *public_key;
    int Block_size= 100;
    char message[Block_size];
    for (int i=0; i<Block_size;i++){
      message[i]= (std::rand() % 256) ;
    }
    cout<<message;
    message[Block_size]= '\0';
    char *encrypt = NULL;
    RSA *keypair = RSA_new();
    char public_key_pem[11]  = "public_key";
    public_key  = read_RSA(keypair, PUBLIC_KEY_PEM, public_key_pem);
    encrypt = (char*)malloc(RSA_size(public_key));
    int encrypt_length = public_encrypt(strlen(message) + 1, (unsigned char*)message, (unsigned char*)encrypt, public_key, RSA_PKCS1_OAEP_PADDING);
     cout<<(sizeof (encrypt))<<endl;
    if(encrypt_length == -1) {
        LOG("An error occurred in public_encrypt() method");
    }
    else{
        LOG("Data has been encrypted.");
        create_encrypted_file(encrypt, public_key);
        LOG("Encrypted file has been created.");
    }
}
