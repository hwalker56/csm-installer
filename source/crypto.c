#include <stdio.h>
#include <string.h>

#include "crypto.h"

static mbedtls_aes_context titlekeyctx[3];

void SetupCommonKeys(void) {
	for (int i = 0; i < 3; i++)
		mbedtls_aes_setkey_dec(&titlekeyctx[i], CommonKeys[i], 128);
}

void GetTitleKey(tik* p_tik, aeskey out) {
	aesiv iv = {};

	uint8_t commonKeyIndex = p_tik->reserved[0xb];
	if (commonKeyIndex > 0x02) {
		printf("Unknown common key index!? (0x%hhx)\n", commonKeyIndex);
		commonKeyIndex = 0;
	}

	iv.titleid = p_tik->titleid;
	mbedtls_aes_crypt_cbc(&titlekeyctx[commonKeyIndex], MBEDTLS_AES_DECRYPT, sizeof(aeskey),
						  iv.full, p_tik->cipher_title_key, out);
}

void ChangeCommonKey(tik* p_tik, uint8_t index) {
	aeskey in = {};
	aesiv  iv = {};

	if (index > 0x02)
		return;

	GetTitleKey(p_tik, in);
	iv.titleid = p_tik->titleid;
	mbedtls_aes_crypt_cbc(&titlekeyctx[index], MBEDTLS_AES_ENCRYPT, sizeof(aeskey),
						  iv.full, in, p_tik->cipher_title_key);
	p_tik->reserved[0xb] = index;
}

void SetupTitleContentContext(tik* p_tik, int16_t index, mbedtls_aes_context* ctx, int mode) {
	aeskey titlekey = {};

	GetTitleKey(p_tik, titlekey);
	(mode == MBEDTLS_AES_ENCRYPT ? mbedtls_aes_setkey_enc : mbedtls_aes_setkey_dec)(ctx, titlekey, 128);
}

int DecryptTitleContent(tik* p_tik, uint16_t index, void* content, size_t csize, void* out, void* iv) {
	mbedtls_aes_context title = {};
	aesiv                 _iv = {};
	void              *ptr_iv = iv ?: _iv.full;

	_iv.index = index;
	SetupTitleContentContext(p_tik, index, &title, MBEDTLS_AES_DECRYPT);
	return mbedtls_aes_crypt_cbc(&title, MBEDTLS_AES_DECRYPT, csize, ptr_iv, content, out);
}

bool CheckHash(void* ptr, size_t len, sha1 expected) {
	sha1 hash = {};

	mbedtls_sha1_ret(ptr, len, hash);
	return !memcmp(hash, expected, sizeof(sha1));
}
