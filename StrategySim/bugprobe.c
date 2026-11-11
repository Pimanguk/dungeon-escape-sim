/* 코어 버그 탐침. 교수님 game.c / snapshot.c 를 그대로 링크해서 "Rule.txt대로 동작하는가"를 확인한다.
   A: 규칙 경계 상황을 하나씩 만들어 결과를 Rule.txt 기대값과 비교
   B: 7명이 무작위 명령(잘못된 enum 포함)을 내는 판을 수천 판 돌리며 매 턴 불변식 검사
   C: 맵 로더에 이상한 입력을 넣어 판정 확인
   D: 상태 파일을 망가뜨려 snapshot_read 와 내 player0 이 죽지 않는지 확인
   E: 무작위 그럴듯한 상태에서 내 player0 의 명령이 규칙상 취소(자해 피해)되는지 확인
   실행: probe.exe [--games N] [--seed S].  "FAIL"이 하나도 없으면 통과, "NOTE"는 버그는 아니지만 알아둘 점. */
#include <windows.h>
#include <limits.h>
#include "../FinalExamGameCore/game.h"

Command player0_choose_command(const char *);

static int fails, notes;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); putchar('\n'); ++fails; } } while (0)
#define NOTE(...) do { printf("NOTE: "); printf(__VA_ARGS__); putchar('\n'); ++notes; } while (0)

static unsigned long long rs = 88172645463325252ull;
static unsigned rnd(unsigned n) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)(rs % n); }

static const char *MAP = "../FinalExamGameCore/dungeonmap.dat";
static int load_map(Game *g)
{
    char err[256]; FILE *f = fopen(MAP, "r"); int ok;
    if (!f) { printf("map open fail\n"); exit(2); }
    ok = game_load(g, f, err, sizeof(err)); fclose(f);
    if (!ok) { printf("map load fail %s\n", err); exit(2); }
    return ok;
}

/* ---- A: 규칙 경계 상황 ---- */
static void arena(Game *g, Command c[PLAYER_COUNT])
{
    int i;
    memset(g, 0, sizeof(*g));
    g->size = 7; g->key_owner = g->winner = -1;
    g->key.x = g->key_start.x = 5; g->key.y = g->key_start.y = 5;
    g->exit.x = 6; g->exit.y = 6;
    memset(g->terrain, ' ', sizeof(g->terrain));
    g->terrain[6][6] = 'E';
    for (i = 0; i < PLAYER_COUNT; ++i) { g->players[i].spawn.x = i; g->players[i].rest = 100; c[i] = COMMAND_ATTACK_UP; }
}
static void place(Game *g, int id, int x, int y, int hp) { g->players[id].pos.x = x; g->players[id].pos.y = y; g->players[id].hp = hp; g->players[id].rest = 0; }

static void scripted(void)
{
    Game g; Command c[PLAYER_COUNT]; int i;

    /* A1 네 명이 2x2 칸을 시계방향으로 돈다: 사슬/순환 이동은 합법이라 아무도 취소되지 않아야 한다 */
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2); place(&g, 2, 2, 2, 2); place(&g, 3, 1, 2, 2);
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_DOWN; c[2] = COMMAND_LEFT; c[3] = COMMAND_UP;
    game_resolve(&g, c);
    CHECK(!g.cancelled[0] && !g.cancelled[1] && !g.cancelled[2] && !g.cancelled[3] && g.players[0].pos.x == 2 && g.players[0].pos.y == 1, "A1 4-cycle should succeed");

    /* A2 앞선 이동이 충돌로 취소되면 뒤따르는 이동도 연쇄 취소 (P0->P1칸, P1->X, P2->X) */
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2); place(&g, 2, 3, 0, 2);
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_RIGHT; c[2] = COMMAND_DOWN; /* P1 -> (3,1), P2 -> (3,1) 충돌 */
    game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.cancelled[1] && g.cancelled[2] && g.players[0].pos.x == 1, "A2 cascade cancel");

    /* A3 시체가 있는 칸으로 이동 가능 (시체는 칸을 점유하지 않는다) */
    arena(&g, c);
    place(&g, 0, 1, 1, 2); g.players[1].pos.x = 2; g.players[1].pos.y = 1; g.players[1].hp = 0; g.players[1].rest = 5;
    c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(!g.cancelled[0] && g.players[0].pos.x == 2, "A3 walking onto corpse cell");

    /* A4 상대가 비우는 칸에 열쇠를 버리기 가능 */
    arena(&g, c);
    place(&g, 0, 1, 1, 2); place(&g, 1, 2, 1, 2); g.key_owner = 0;
    c[0] = COMMAND_DROP_RIGHT; c[1] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(!g.cancelled[0] && !g.cancelled[1] && g.key_owner == -1 && g.key.x == 2 && g.key.y == 1, "A4 drop on vacated cell");

    /* A5 한 명이 열쇠를 집는 동안 다른 사람이 열쇠 칸으로 걸어 들어와도 둘 다 성공 */
    arena(&g, c);
    place(&g, 0, 4, 5, 2); place(&g, 1, 5, 4, 2);
    c[0] = COMMAND_PICK_RIGHT; c[1] = COMMAND_DOWN; game_resolve(&g, c);
    CHECK(g.key_owner == 0 && !g.cancelled[0] && !g.cancelled[1] && g.players[1].pos.y == 5, "A5 pick + walk onto key cell");

    /* A6 한 명이 여러 명에게 맞아도 HP는 최대 1만 줄어든다 */
    arena(&g, c);
    place(&g, 0, 3, 3, 2); place(&g, 1, 3, 4, 2); place(&g, 2, 2, 4, 2); place(&g, 3, 4, 4, 2);
    c[1] = c[2] = c[3] = COMMAND_ATTACK_UP; c[0] = COMMAND_ATTACK_UP;
    game_resolve(&g, c);
    CHECK(g.players[0].hp == 1, "A6 three attackers cost exactly 1 HP (got %d)", g.players[0].hp);

    /* A7 벽 모서리 너머 대각선 칸도 공격 가능 (Rule 7절: 모서리 차폐 없음) */
    arena(&g, c);
    place(&g, 0, 3, 3, 2); place(&g, 1, 2, 2, 2); g.terrain[2][3] = '#'; g.terrain[3][2] = '#';
    c[0] = COMMAND_ATTACK_UP; game_resolve(&g, c);
    CHECK(g.players[1].hp == 1, "A7 diagonal behind wall corner is hit");

    /* A8 모서리에서 필드 밖 공격: 크래시/피해 없음 */
    arena(&g, c); place(&g, 0, 0, 0, 2); place(&g, 1, 1, 0, 2); c[0] = COMMAND_ATTACK_LEFT; c[1] = COMMAND_ATTACK_UP;
    game_resolve(&g, c);
    CHECK(g.players[0].hp == 2 && g.players[1].hp == 2, "A8 out-of-bounds attack harmless");

    /* A9 열쇠 없이 출구로 이동: 취소 + 피해 */
    arena(&g, c); place(&g, 0, 5, 6, 2); c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.players[0].hp == 1 && g.winner == -1, "A9 exit w/o key");

    /* A10 열쇠 소유자가 출구로 이동하는 턴에 한 대 맞아도 HP2면 승리 */
    arena(&g, c); place(&g, 0, 5, 6, 2); place(&g, 1, 6, 5, 2); g.key_owner = 0;
    c[0] = COMMAND_RIGHT; c[1] = COMMAND_ATTACK_DOWN; game_resolve(&g, c);
    CHECK(g.winner == 0 && g.players[0].hp == 1, "A10 carrier survives hit and wins");

    /* A11 잘못된 enum 값들은 모두 취소 + 피해 */
    {
        int bad[] = {-1, -5, 16, 17, 999, INT_MAX, INT_MIN};
        for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); ++i) {
            arena(&g, c); place(&g, 0, 3, 3, 2); c[0] = (Command)bad[i]; game_resolve(&g, c);
            CHECK(g.cancelled[0] && g.players[0].hp == 1, "A11 bad enum %d", bad[i]);
        }
    }

    /* A12 휴식 턴 수 포화: 사망 64회 -> 2^63, 65회 이상 -> UINT64_MAX, 그 값에서도 감소 연산은 안전 */
    arena(&g, c); place(&g, 0, 2, 2, 1); g.players[0].deaths = 63; c[0] = (Command)-1;
    game_resolve(&g, c); CHECK(g.players[0].deaths == 64 && g.players[0].rest == (UINT64_C(1) << 63), "A12 64th death rest");
    arena(&g, c); place(&g, 0, 2, 2, 1); g.players[0].deaths = 64; c[0] = (Command)-1;
    game_resolve(&g, c); CHECK(g.players[0].deaths == 65 && g.players[0].rest == UINT64_MAX, "A12 65th death saturates");
    game_begin_turn(&g); CHECK(g.players[0].rest == UINT64_MAX - 1 && g.players[0].hp == 0, "A12 rest decrement");

    /* A13 승자가 정해진 뒤 다시 resolve 해도 상태가 바뀌지 않는다 */
    arena(&g, c); place(&g, 0, 5, 6, 2); g.key_owner = 0; c[0] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.winner == 0, "A13 setup");
    place(&g, 1, 2, 2, 2); c[1] = COMMAND_RIGHT; game_resolve(&g, c);
    CHECK(g.players[1].pos.x == 2, "A13 post-win resolve ignored");

    /* A14 출구로 버리기 / 같은 턴에 떨어진 열쇠 즉시 줍기 */
    arena(&g, c); place(&g, 0, 5, 6, 2); g.key_owner = 0; c[0] = COMMAND_DROP_RIGHT; game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.key_owner == 0, "A14 drop onto exit");
    arena(&g, c); place(&g, 0, 4, 5, 2); place(&g, 1, 5, 4, 2); g.key_owner = 1; g.key.x = 5; g.key.y = 4;
    c[0] = COMMAND_PICK_RIGHT; c[1] = COMMAND_DROP_UP; game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.key_owner == -1, "A14 cannot pick key being dropped this turn");

    /* A15 같은 칸을 향한 이동 + 열쇠 버리기 -> 둘 다 취소 */
    arena(&g, c); place(&g, 0, 1, 1, 2); place(&g, 1, 3, 1, 2); g.key_owner = 0;
    c[0] = COMMAND_DROP_RIGHT; c[1] = COMMAND_LEFT; game_resolve(&g, c);
    CHECK(g.cancelled[0] && g.cancelled[1], "A15 drop vs move same cell");

    /* A16 열쇠를 든 채로 자기 취소 피해로 죽으면 그 자리에 열쇠가 떨어진다 */
    arena(&g, c); place(&g, 0, 2, 2, 1); g.key_owner = 0; c[0] = COMMAND_PICK_UP; game_resolve(&g, c);
    CHECK(g.players[0].hp == 0 && g.key_owner == -1 && g.key.x == 2 && g.key.y == 2, "A16 carrier dies by own cancel");
}

/* ---- B: 무작위 판 + 불변식 ---- */
static unsigned inv_viol[16];
static void invariants(const Game *g, const int prev_deaths[PLAYER_COUNT], const int prev_hp[PLAYER_COUNT], int turn)
{
    int i, j;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        const PlayerState *p = &g->players[i];
        if (p->hp < 0 || p->hp > MAX_HP) { ++inv_viol[0]; printf("FAIL B hp range P%d=%d turn %d\n", i, p->hp, turn); ++fails; }
        if (p->deaths < (unsigned)prev_deaths[i]) { ++inv_viol[1]; printf("FAIL B deaths decreased P%d turn %d\n", i, turn); ++fails; }
        if (p->hp > 0) {
            if (p->pos.x < 0 || p->pos.y < 0 || p->pos.x >= g->size || p->pos.y >= g->size || g->terrain[p->pos.y][p->pos.x] == '#')
                { ++inv_viol[2]; printf("FAIL B P%d in wall/out turn %d\n", i, turn); ++fails; }
            for (j = i + 1; j < PLAYER_COUNT; ++j)
                if (g->players[j].hp > 0 && g->players[j].pos.x == p->pos.x && g->players[j].pos.y == p->pos.y)
                    { ++inv_viol[3]; printf("FAIL B P%d and P%d share a cell turn %d\n", i, j, turn); ++fails; }
            if (g->terrain[p->pos.y][p->pos.x] == 'E' && g->key_owner != i)
                { ++inv_viol[4]; printf("FAIL B P%d on exit w/o key turn %d\n", i, turn); ++fails; }
        } else if (prev_hp[i] > 0 && p->deaths >= 1 && p->deaths <= 64 && p->rest != (UINT64_C(1) << (p->deaths - 1)))
            { ++inv_viol[5]; printf("FAIL B P%d rest %llu != 2^(d-1) turn %d\n", i, (unsigned long long)p->rest, turn); ++fails; }
    }
    if (g->key_owner >= PLAYER_COUNT || g->key_owner < -1) { ++inv_viol[6]; printf("FAIL B key_owner range\n"); ++fails; }
    if (g->key_owner >= 0 && g->players[g->key_owner].hp <= 0) { ++inv_viol[7]; printf("FAIL B dead key owner turn %d\n", turn); ++fails; }
    if (g->key_owner < 0 && (g->terrain[g->key.y][g->key.x] == '#' || g->terrain[g->key.y][g->key.x] == 'E'))
        { ++inv_viol[8]; printf("FAIL B key on wall/exit turn %d\n", turn); ++fails; }
    if (g->winner >= 0 && (g->winner != g->key_owner || g->players[g->winner].hp <= 0 ||
        g->players[g->winner].pos.x != g->exit.x || g->players[g->winner].pos.y != g->exit.y))
        { ++inv_viol[9]; printf("FAIL B bad winner turn %d\n", turn); ++fails; }
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (prev_hp[i] > 0 && g->cancelled[i] && !g->damaged[i]) { ++inv_viol[10]; printf("FAIL B cancel without damage\n"); ++fails; }
}

static void random_games(int games)
{
    static Game g;
    long turns = 0, wins = 0, cancels = 0, deaths = 0, keydrops = 0;
    int gm;
    for (gm = 0; gm < games; ++gm) {
        load_map(&g);
        while (g.turn < 300 && g.winner < 0) {
            Command c[PLAYER_COUNT]; int i, pd[PLAYER_COUNT], ph[PLAYER_COUNT], had = g.key_owner;
            game_begin_turn(&g);
            for (i = 0; i < PLAYER_COUNT; ++i) {
                unsigned r = rnd(100);
                pd[i] = (int)g.players[i].deaths; ph[i] = g.players[i].hp;
                if (r < 3) c[i] = (Command)(int)((int)rnd(40) - 12);        /* 잘못된 값 */
                else if (r < 20) c[i] = (Command)(COMMAND_PICK_UP + rnd(8)); /* 줍기/버리기 아무거나 */
                else if (r < 60 && g.players[i].hp > 0) {                    /* 목표 지향: 열쇠 줍기 / 출구로 / 열쇠 쪽으로 (열쇠·승리 경로를 충분히 밟게 한다) */
                    Position me = g.players[i].pos, goal = g.key_owner == i ? g.exit : g.key;
                    static const int ddx[4] = {0, 0, -1, 1}, ddy[4] = {-1, 1, 0, 0};
                    int d, bestd = 0, bestv = 9999;
                    c[i] = (Command)rnd(COMMAND_ATTACK_RIGHT + 1);
                    for (d = 0; d < 4; ++d) {
                        if (g.key_owner == -1 && me.x + ddx[d] == g.key.x && me.y + ddy[d] == g.key.y) { c[i] = (Command)(COMMAND_PICK_UP + d); bestv = -1; break; }
                        { int v = abs(goal.x - (me.x + ddx[d])) + abs(goal.y - (me.y + ddy[d])); if (v < bestv) { bestv = v; bestd = d; } }
                    }
                    if (bestv >= 0) c[i] = (Command)bestd;
                    if (g.key_owner == i && rnd(15) == 0) c[i] = (Command)(COMMAND_DROP_UP + rnd(4));
                }
                else c[i] = (Command)rnd(COMMAND_ATTACK_RIGHT + 1);          /* 이동/공격 */
            }
            game_resolve(&g, c);
            invariants(&g, pd, ph, g.turn);
            for (i = 0; i < PLAYER_COUNT; ++i) { cancels += g.cancelled[i]; if (ph[i] > 0 && g.players[i].hp <= 0) ++deaths; }
            if (had >= 0 && g.key_owner < 0) ++keydrops;
            ++turns;
        }
        if (g.winner >= 0) ++wins;
    }
    printf("B random fuzz: %d games, %ld turns, %ld cancels, %ld deaths, %ld key releases, %ld wins, failures so far=%d\n", games, turns, cancels, deaths, keydrops, wins, fails);
}

/* ---- C: 맵 로더 ---- */
static int load_text(Game *g, const char *text, size_t len)
{
    char err[256]; FILE *f = NULL; int ok;
    if (tmpfile_s(&f) != 0) return -1;
    fwrite(text, 1, len, f); rewind(f);
    ok = game_load(g, f, err, sizeof(err)); fclose(f);
    return ok;
}
static void loader(void)
{
    static Game g;
    char buf[512];
    struct { const char *what; const char *text; int expect; } cases[] = {
        {"valid", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########\n", 1},
        {"no final newline", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########", 1},
        {"CRLF", "##########\r\n##P0@-()##\r\n##P1P2P3##\r\nH0P4P5P6##\r\n##########\r\n", 1},
        {"mixed LF/CRLF", "##########\n##P0@-()##\r\n##P1P2P3##\nH0P4P5P6##\r\n##########\n", 1},
        {"extra blank line at end", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########\n\n", 0},
        {"duplicate P1", "##########\n##P0@-()##\n##P1P1P3##\nH0P4P5P6##\n##########\n", 0},
        {"missing H0", "##########\n##P0@-()##\n##P1P2P3##\n  P4P5P6##\n##########\n", 0},
        {"tab char", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5\t ##\n##########\n", 0},
        {"unknown token", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5XX##\n##########\n", 0},
        {"two exits", "##########\n##P0@-()##\n##P1P2()##\nH0P4P5P6##\n##########\n", 0},
        {"not square", "##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n", 0},
        {"odd width line", "#########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########\n", 0},
        {"empty file", "", 0},
        {"only newline", "\n", 0},
        {"UTF-8 BOM", "\xEF\xBB\xBF##########\n##P0@-()##\n##P1P2P3##\nH0P4P5P6##\n##########\n", 0},
    };
    int i;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        int got = load_text(&g, cases[i].text, strlen(cases[i].text));
        if (got != cases[i].expect) {
            if (!strcmp(cases[i].what, "extra blank line at end"))
                NOTE("로더: 파일 끝에 빈 줄이 하나 더 있으면 로딩 오류(Rule.txt는 마지막 줄 개행 생략만 언급). 교수님 맵에 빈 줄이 없으면 문제 없음.");
            else { printf("FAIL C loader '%s' expect %d got %d\n", cases[i].what, cases[i].expect, got); ++fails; }
        }
    }
    memset(buf, '#', sizeof(buf)); buf[400] = '\n';
    CHECK(load_text(&g, buf, 401) == 0, "C long line should be rejected");
    { char t[] = "##\0##\n"; CHECK(load_text(&g, t, sizeof(t) - 1) == 0, "C NUL byte should be rejected"); }
    {
        char big[40 * 70]; int r; size_t p = 0;
        for (r = 0; r < 33; ++r) { memset(big + p, '#', 64); big[p + 64] = '\n'; p += 65; }
        CHECK(load_text(&g, big, p) == 0, "C 33 rows rejected");
    }
    load_map(&g);
    printf("C loader: %d cases checked; real map size=%d key=(%d,%d) exit=(%d,%d)\n", (int)(sizeof(cases) / sizeof(cases[0])), g.size, g.key.x, g.key.y, g.exit.x, g.exit.y);
}

/* ---- D: 상태 파일 변조 ---- */
static void write_file(const char *path, const char *data, size_t len) { FILE *f = fopen(path, "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); } }
static void snapshot_fuzz(int rounds)
{
    static Game g; const char *path = "mem:1"; char text[4096], mut[4096];   /* mem:N = sim_io.c 의 메모리 파이프(디스크 없이 빠르게) */
    int r, accepted = 0, bad_range = 0, bad_cmd = 0, leaks = 0;
    load_map(&g); g.turn = 7;
    for (r = 0; r < rounds; ++r) {
        FILE *f; size_t n, k; Snapshot s; int ok, nm, self = (int)rnd(PLAYER_COUNT);
        snapshot_write(&g, self, "mem:0");
        f = fopen("mem:0", "r"); n = fread(text, 1, sizeof(text), f); fclose(f);
        memcpy(mut, text, n);
        nm = (int)rnd(5);
        for (k = 0; k < (size_t)nm; ++k) mut[rnd((unsigned)n)] = (char)rnd(256);   /* 바이트 변조 */
        if (rnd(2)) { /* 숫자 헤더/플레이어 줄의 숫자 한 자리를 다른 숫자로: 파싱은 되지만 값이 이상한 파일 */
            size_t lim = n < 200 ? n : 200, p = rnd((unsigned)lim);
            while (p < lim && (mut[p] < '0' || mut[p] > '9')) ++p;
            if (p < lim) mut[p] = (char)('0' + rnd(10));
        }
        if (rnd(4) == 0) n = rnd((unsigned)n);                                       /* 잘라내기 */
        write_file(path, mut, n);
        f = fopen(path, "r"); ok = f ? snapshot_read(f, &s) : 0; if (f) fclose(f);
        write_file(path, mut, n);
        if (ok) {
            ++accepted;
            if (s.key.x < 0 || s.key.y < 0 || s.key.x >= s.size || s.key.y >= s.size || s.exit.x < 0 || s.exit.y < 0 ||
                s.exit.x >= s.size || s.exit.y >= s.size || s.key_start.x < 0 || s.key_start.x >= s.size || s.key_start.y < 0 || s.key_start.y >= s.size) ++bad_range;
        }
        {
            Command c = player0_choose_command(path);   /* 어떤 파일이어도 내 AI는 죽지 않고 합법 enum 을 돌려줘야 한다 */
            if ((int)c < 0 || (int)c >= COMMAND_COUNT) ++bad_cmd;
            if (_fcloseall() > 0) ++leaks;
        }
    }
    CHECK(bad_cmd == 0, "D player0 returned illegal enum %d times", bad_cmd);
    CHECK(leaks == 0, "D player0 leaked FILE* %d times", leaks);
    printf("D snapshot fuzz: %d rounds, snapshot_read accepted %d, accepted-with-out-of-range key/exit/key_start %d, player0 bad enum %d, FILE leaks %d\n", rounds, accepted, bad_range, bad_cmd, leaks);
    if (bad_range) NOTE("snapshot_read 는 key/exit/key_start 좌표가 맵 밖이어도 받아들인다(헤더 좌표 검증 없음). 정상 코어가 쓴 파일에선 안 생기지만, 학생 코드가 이 좌표로 배열을 읽으면 위험하다.");
}

/* ---- E: 그럴듯한 무작위 상태에서 내 player0 이 규칙상 취소(자해 피해)를 내는가 ---- */
static void p0_legality(int rounds)
{
    static Game g, base; const char *path = "mem:2";
    int r, total = 0, cancelled = 0, byreason[3] = {0};
    load_map(&base);
    for (r = 0; r < rounds; ++r) {
        int i, j, self = (int)rnd(PLAYER_COUNT); Command c[PLAYER_COUNT]; Command mine;
        memcpy(&g, &base, sizeof(g));
        g.turn = (int)rnd(150) + 1;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int x, y, tries = 0;
            do { x = (int)rnd(g.size); y = (int)rnd(g.size); ++tries; }
            while (tries < 100 && (g.terrain[y][x] != ' ' || (x == g.key.x && y == g.key.y)));
            g.players[i].pos.x = x; g.players[i].pos.y = y;
            g.players[i].hp = rnd(5) == 0 ? 0 : (int)rnd(2) + 1;
            g.players[i].deaths = rnd(4); g.players[i].rest = g.players[i].hp ? 0 : rnd(3);
        }
        for (i = 0; i < PLAYER_COUNT; ++i)
            for (j = 0; j < i; ++j)
                if (g.players[i].hp > 0 && g.players[j].hp > 0 && g.players[i].pos.x == g.players[j].pos.x && g.players[i].pos.y == g.players[j].pos.y) g.players[i].hp = 0;
        g.players[self].hp = (int)rnd(2) + 1;
        g.key_owner = -1;
        switch (rnd(3)) {
            case 1: g.key_owner = self; break;
            case 2: { int o = (int)rnd(PLAYER_COUNT); if (g.players[o].hp > 0) g.key_owner = o; break; }
            default: break;
        }
        if (g.key_owner < 0 && rnd(2)) { int x = (int)rnd(g.size), y = (int)rnd(g.size); if (g.terrain[y][x] == ' ') { g.key.x = x; g.key.y = y; } }
        snapshot_write(&g, self, path);
        mine = player0_choose_command(path);
        if (_fcloseall() > 0) { printf("FAIL E file leak\n"); ++fails; }
        for (i = 0; i < PLAYER_COUNT; ++i) c[i] = COMMAND_ATTACK_UP;   /* 나머지는 제자리 공격(무해) */
        c[self] = mine;
        ++total;
        game_resolve(&g, c);
        if (g.cancelled[self]) {
            ++cancelled;
            if (mine >= COMMAND_PICK_UP && mine <= COMMAND_DROP_RIGHT) ++byreason[0];
            else if (mine <= COMMAND_RIGHT) ++byreason[1];
            else ++byreason[2];
        }
    }
    printf("E player0 legality: %d random states, self-cancelled %d (%.2f%%)  [pick/drop %d, move %d, other %d]\n", total, cancelled, 100.0 * cancelled / total, byreason[0], byreason[1], byreason[2]);
    CHECK(cancelled * 100 <= total * 3, "E player0 issues illegal commands too often");
}

int main(int argc, char **argv)
{
    int games = 3000, i;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) rs = _strtoui64(argv[++i], NULL, 10) * 2654435761ull + 1;
        else { fprintf(stderr, "bad arg %s\n", argv[i]); return 1; }
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    scripted();   printf("A scripted rule cases done, failures so far %d\n", fails);
    random_games(games);
    loader();
    snapshot_fuzz(3000);
    p0_legality(20000);
    printf("\n== bugprobe result: %d FAIL, %d NOTE ==\n", fails, notes);
    return fails ? 1 : 0;
}
