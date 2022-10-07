#ifndef RSA_ALGORITHM_H
#define RSA_ALGORITHM_H

#define KEY_LENGTH       2048
#define PUBLIC_EXPONENT  59     //Public exponent should be a prime number.
#define PUBLIC_KEY_PEM   1
#define PRIVATE_KEY_PEM  0

#define LOG(x)               \
        cout << x << endl;   \

/*
 * @brief   create_RSA function creates public key and private key file
 *
 */


 RSA * read_RSA(RSA *keypair, int pem_type, char *file_name);


/*
 * @brief   private_decrypt function decrypt data.
 * @return  If It is fail, 
 */
 int private_decrypt(int flen, unsigned char* from, unsigned char *to, RSA* key, int padding);

/*
 * @brief   create_ecrypted_file function creates .bin file. It contains encrypted data.
 */
 void create_encrypted_file(char* encrypted, RSA * key_pair);
 
 char* decrypt();


#endif //RSA_ALGORITHM_H
