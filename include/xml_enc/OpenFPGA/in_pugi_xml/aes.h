#ifndef AES_H
#define AES_H
#include "function.h"
#include "openssl_rsa.h"
#define AES_BLOCK_SIZE 256
const int BUFSIZE = 4096;

// using namespace std;

const string default_encryption_cipher_ = "aes";
const int default_keysize_ = 256;
const int default_blocksize_ = 128;
const string default_encryption_mode_ = "cbc";
const unsigned default_pbkdf2_iterations_ = 1000;
const unsigned default_pbkdf2_saltlen_ = 8;

class Enc_Dec
{
public:
	unsigned char *key = NULL, *iv = NULL;
	Enc_Dec(const string &sourcefile) //, const string &destfile
	{
		OpenSSL_add_all_algorithms();
		const char *var = decrypt();
		key = key_generation(var, default_keysize_, default_pbkdf2_iterations_, default_pbkdf2_saltlen_);
		//decrypt_file(sourcefile, destfile);
	}

public:
	std::string decrypt_file(const string &sourcefile) // const string &destfile
	{
		int rc = 1;
		std::string final;
		EVP_CIPHER_CTX *dec_ctx = EVP_CIPHER_CTX_new();
		EVP_CIPHER_CTX_init(dec_ctx);
		char ciphername[100] = {0};
		const EVP_CIPHER *ciph;
		unsigned char inbuf[BUFSIZE], outbuf[BUFSIZE + AES_BLOCK_SIZE << 1]; // specific to AES
		fstream ofile;
		ifstream ifile;
		int bytes_read, bytes_decrypted,
			total_bytes_read = 0, total_bytes_decrypted = 0;
		// 1. Open input file
		ifile.open(sourcefile.c_str(), ios::in | ios::binary);
		if (!ifile.is_open())
		{
			cerr << "Cannot open input file " << sourcefile << endl;
			return "0";
		}

		// 2. Check that input file is of the type we expect
		//    by checking for magic string at header of file
		// char magic[128] = {0};

		// 2. Check that output file can be opened and written to
		//ofile.open(destfile, ios::out | ios::binary | ios::trunc);
		//if (!ofile.is_open())
		//{
		//	cerr << "Cannot open input file " << sourcefile << endl;
	//		return rc;
	//	}

		// 3. Derive key from passphrase, create salt and IV
		// unsigned char salt_value[default_pbkdf2_saltlen_];

		// 4. Initialize encryption engine / context / etc.
		snprintf(ciphername, 99, "%s-%d-%s",
				 default_encryption_cipher_.c_str(),
				 default_keysize_, default_encryption_mode_.c_str()); // FIXME
		if (!(ciph = EVP_get_cipherbyname(ciphername)))
		{
			cerr << "Cannot find algorithm " << ciphername << endl;
			goto free_data;
		}
		EVP_CIPHER_CTX_init(dec_ctx);
		if (!EVP_DecryptInit_ex(dec_ctx, ciph, NULL, key, iv))
		{
			// returns 0 for failure (wtf?)
			cerr << "Cannot initialize decryption cipher " << ciphername << endl;
			goto free_data;
		}

		// 5.1 Read source blocks, decrypt, write to output stream
		while (!ifile.eof())
		{
			ifile.read((char *)inbuf, BUFSIZE);
			bytes_read = (int)ifile.gcount();
			if (bytes_read > 0)
			{
				if (!EVP_DecryptUpdate(dec_ctx, outbuf, &bytes_decrypted,
									   inbuf, bytes_read))
				{
					cerr << "Error decrypting chunk at byte " << total_bytes_decrypted << endl;
					goto free_data;
				}
				//            assert(bytes_decrypted > 0); // this is not necessarily true
				if (bytes_decrypted > 0){
					//ofile.write((char *)outbuf, bytes_decrypted);
					 std::string str1( outbuf, outbuf+bytes_decrypted );
					 final =final+str1;
				}

				total_bytes_read += bytes_read;
				total_bytes_decrypted = bytes_decrypted;
			}
			bytes_read = bytes_decrypted = 0;
		}
		// 5.2 Encrypt remaining data and write final block of output
		EVP_DecryptFinal_ex(dec_ctx, outbuf, &bytes_decrypted);
		if (bytes_decrypted > 0)
		{
			//ofile.write((char *)outbuf, bytes_decrypted);
			std::string str1( outbuf, outbuf+bytes_decrypted );
			final =final+str1;
		}

		ifile.close();
		rc = 0;
	free_data:
		delete[] key;
		delete[] iv;
		return final;
	}

	int usage(const char *programname)
	{
		cerr << "Usage: " << programname << "  Requires file for Encryption or Decryption" << endl;
		return 1;
	}
};

#endif
