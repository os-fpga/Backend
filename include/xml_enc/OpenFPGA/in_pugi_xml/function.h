/*
 * function.h
 *
 *  Created on: May 18, 2022
 *      Author: raza.jafari
 */


#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "pugixml_util.hpp"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
}

using namespace std;

unsigned char* key_generation( const string passphrase, const int default_keysize_ , const unsigned   default_pbkdf2_iterations_, const unsigned   default_pbkdf2_saltlen_){
    unsigned char salt_value[default_pbkdf2_saltlen_];
    unsigned char *key = NULL, *iv = NULL;
    iv = new unsigned char [default_keysize_ / 16];
    std::fill(iv, iv + default_keysize_/16, 5121472); 

    std::fill(salt_value, salt_value + sizeof(salt_value), 'syed'); 
    key = new unsigned char [default_keysize_ / 8];
    if(!PKCS5_PBKDF2_HMAC(passphrase.c_str(), passphrase.length(), salt_value,
                              sizeof(salt_value), default_pbkdf2_iterations_,
                              EVP_sha256(),
                               default_keysize_ / 8, key)) {
        throw pugiutil::XmlError("Error in driving the .xmle file");
        	delete [] key;
            delete [] iv;
    }
    return key;
}

