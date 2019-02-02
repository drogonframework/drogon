#include <drogon/config.h>
#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else
#include "../lib/src/ssl_funcs/Sha1.h"
#endif
#include <stdio.h>
int main()
{
	unsigned char in[] = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
	unsigned char out[SHA_DIGEST_LENGTH] = {0};
	SHA1(in, strlen((const char *)in), out);
	/// fecfd28bbc9345891a66d7c1b8ff46e60192d284
	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		printf("%02x", out[i]);
	putchar('\n');
}
