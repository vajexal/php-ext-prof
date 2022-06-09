#include "gzencode.h"

#include <zlib.h>

#define windowBits 15
#define GZIP_ENCODING 16
// copypaste from php_zlib
#define ZLIB_BUFFER_SIZE_GUESS(in_len) (((size_t) ((double) in_len * (double) 1.015)) + 10 + 8 + 4 + 1)

static voidpf zlib_alloc(voidpf opaque, uInt items, uInt size) {
    return (voidpf)safe_emalloc(items, size, 0);
}

static void zlib_free(voidpf opaque, voidpf address) {
    efree((void *)address);
}

zend_string *gzencode(const char *buf, size_t len) {
    int status;
    z_stream stream;
    zend_string *out;

    memset(&stream, 0, sizeof(z_stream));
    stream.zalloc = zlib_alloc;
    stream.zfree = zlib_free;

    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits | GZIP_ENCODING, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
        return NULL;
    }

    out = zend_string_alloc(ZLIB_BUFFER_SIZE_GUESS(len), 0);

    stream.next_in = (Bytef *)buf;
    stream.next_out = (Bytef *)ZSTR_VAL(out);
    stream.avail_in = len;
    stream.avail_out = ZSTR_LEN(out);

    status = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (status != Z_STREAM_END) {
        zend_string_efree(out);
        return NULL;
    }

    out = zend_string_truncate(out, stream.total_out, 0);
    ZSTR_VAL(out)[ZSTR_LEN(out)] = '\0';
    return out;
}
