/* diagnostics.c 대체품(시뮬 전용). "mem:N" 경로는 디스크 대신 익명 파이프로 처리해서
   snapshot_write -> AI의 fopen/snapshot_read 흐름을 그대로 유지하면서 디스크 I/O를 없앤다.
   그 외 경로는 진짜 fopen. 열린 파일 수를 세어 AI의 fclose 누락도 잡는다. */
#define DIAGNOSTICS_IMPLEMENTATION
#include "../FinalExamGameCore/diagnostics.h"
#include <windows.h>
#include <io.h>
#include <fcntl.h>

static HANDLE pending[16]; /* 슬롯별로 쓰기가 끝나고 읽기를 기다리는 파이프 읽기 끝 */
int sim_open_files;

void diagnostics_begin(void) {}
int diagnostics_end(void) { return 0; }

FILE *checked_fopen(const char *path, const char *mode, const char *source, int line)
{
    FILE *f;
    (void)source; (void)line;
    if (strncmp(path, "mem:", 4) == 0) {
        int slot = atoi(path + 4) & 15;
        if (mode[0] == 'w') {
            HANDLE r, w;
            if (pending[slot]) { CloseHandle(pending[slot]); pending[slot] = NULL; }
            if (!CreatePipe(&r, &w, NULL, 1 << 16)) return NULL;
            pending[slot] = r;
            f = _fdopen(_open_osfhandle((intptr_t)w, _O_WRONLY | _O_TEXT), "w");
        } else {
            HANDLE r = pending[slot];
            if (!r) return NULL;
            pending[slot] = NULL;
            f = _fdopen(_open_osfhandle((intptr_t)r, _O_RDONLY | _O_TEXT), "r");
        }
    } else f = fopen(path, mode);
    if (f) ++sim_open_files;
    return f;
}

int checked_fopen_s(FILE **out, const char *path, const char *mode, const char *source, int line)
{
    *out = checked_fopen(path, mode, source, line);
    return *out ? 0 : 1;
}

int checked_fclose(FILE *stream, const char *source, int line)
{
    (void)source; (void)line;
    --sim_open_files;
    return fclose(stream);
}
