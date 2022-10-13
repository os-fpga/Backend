#ifndef RSA_ALGORITHM_H
#define RSA_ALGORITHM_H

#define KEY_LENGTH       2048
#define PUBLIC_EXPONENT  59     //Public exponent should be a prime number.

/*
 * @brief   private_decrypt function decrypt data present in "from array" of length flen and stores the data in "to".
 * @return  1 if it fails, 
 */
 int private_decrypt(int flen, unsigned char* from, unsigned char *to, RSA* key, int padding);
/*
 * @brief  The decrypt function reads the hard coded file named vpr_e.bin and the priviate key embeded in the code.
 * @return  decrypted key back
 */
 
 char* decrypt(std::string bin_file);


#endif //RSA_ALGORITHM_H
