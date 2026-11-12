/* 전략 시뮬레이터. 교수님 코어(game.c/snapshot.c)를 그대로 링크해서 규칙은 100% 동일하다.
   차이점: (1) AI를 별도 프로세스 대신 같은 프로세스에서 호출해 빠르게 수천 판을 돌린다.
          (2) H0는 영구 사망 처리(최종 대회와 동일하게 6명만 경쟁). 지도에 H0 표시가 없어도 읽는다(임시 지도에 H0를 넣어 코어에 넘긴다).
          (3) 매 판 P0~P5 시작 위치를 무작위로 섞는다(교수님 공지 반영). --fixseat 를 주면 맵에 적힌 P0 자리에 고정한다.
   (4) 맵은 기본으로 교수님 맵(FinalExamGameCore\dungeonmap.dat). --map 으로 다른 맵 파일을 줄 수 있다(StrategySim\maps 에 예시).
   (5) 열쇠 위치: 기본은 맵에 적힌 자리. --keycells "x,y;x,y;..." 로 후보 칸을 주면 판마다 그 중 하나에 놓고,
       지도 옆에 <지도파일>.keycells 가 있으면 옵션 없이도 그 칸들을 쓴다. --keyzones / --keyzone N 은 네 모서리 구역에 놓는다.
   사용: sim.exe [--games N] [--seed S] [--scenario 이름|all] [--map 파일] [--fixseat] [--keycells ..] [--byseat] [--show] [--watch ms]
   --byseat : 끝에 시작 자리별 / 열쇠 칸별 승률표를 낸다.
   --teams a,b,c,d,e,f : 6팀을 이름으로 직접 지정 (첫 번째가 승률 기준 팀, 예: ours,team1,team2,safe,vulture,rusher)
   P0 자리(ours)는 FinalExamGameCore\player0.c 의 player0_choose_command 이다. 거기에 내 전략을 넣으면 된다.
   팀원 전략 넣기: custom\team1.c ~ team5.c 의 strat_teamN 을 팀원의 playerN_choose_command 로 교체(함수 이름만 strat_teamN 으로),
   보조 함수/전역은 static 으로, 고친 뒤 build.bat 로 다시 빌드. 같은 --seed 로 돌리면 같은 판이라 비교가 공정하다.
   주의: 시뮬레이터는 전략을 같은 프로그램 안에서 이어 부르므로 static/전역으로 이전 턴을 기억하는 전략은 실제 게임(매 턴 새 프로세스)과 결과가 다르다.
   메타(상대 5명 구성)는 아래 SCEN 표의 한 줄이다. 이름 앞에 *를 붙인 "*all" 같은 항목은 판마다 POOLS 에서 무작위로 뽑는다. */
#include <windows.h>
#include <time.h>
#include "../FinalExamGameCore/game.h"

typedef Command (*Strategy)(const char *);
extern int sim_open_files; /* sim_io.c */
Command player0_choose_command(const char *);
Command player1_choose_command(const char *); /* 교수님 예제 AI */
Command strat_rusher(const char *), strat_wounded(const char *);
Command strat_gatehunter(const char *), strat_hunter(const char *);
Command strat_safe(const char *), strat_vulture(const char *), strat_exitguard(const char *);
Command strat_brawler(const char *), strat_chaos(const char *), strat_camper(const char *);
/* 팀원 전략 칸: custom\team1.c ~ team5.c 에 들어 있다 */
Command strat_team1(const char *), strat_team2(const char *), strat_team3(const char *);
Command strat_team4(const char *), strat_team5(const char *);

/* 전략 이름표. 새 전략을 만들면 여기에 {"이름", 함수} 한 줄만 추가하면 시나리오에서 이름으로 쓸 수 있다. */
typedef struct { const char *name; Strategy fn; } Entry;
static const Entry STRATS[] = {
    {"ours", player0_choose_command}, {"sample", player1_choose_command},
    {"rusher", strat_rusher}, {"wounded", strat_wounded},
    {"gatehunter", strat_gatehunter}, {"hunter", strat_hunter},
    {"safe", strat_safe}, {"vulture", strat_vulture}, {"exitguard", strat_exitguard},
    {"brawler", strat_brawler}, {"chaos", strat_chaos}, {"camper", strat_camper},
    {"team1", strat_team1}, {"team2", strat_team2}, {"team3", strat_team3},
    {"team4", strat_team4}, {"team5", strat_team5},
};
#define NSTRAT ((int)(sizeof(STRATS) / sizeof(STRATS[0])))

/* 상대 5명 구성(= 메타). 내 전략(ours = player0.c)은 항상 P0 자리, 상대 5명은 P1~P5 자리에 무작위로 섞여 선다.
   새 메타를 넣으려면 아래 맨 끝 "내 메타" 줄을 고치거나 한 줄 복사해서 추가한다.
   이름은 위 STRATS 표의 이름만 쓸 수 있다. 시나리오 이름은 메뉴의 --scenario 에 그대로 쓰이니 겹치지 않게 짓는다. */
typedef struct { const char *name; const char *opp[5]; } Scenario;
static const Scenario SCEN[] = {
    {"meta(user)",   {"wounded", "wounded", "wounded", "gatehunter", "rusher"}},
    {"all-wounded",  {"wounded", "wounded", "wounded", "wounded", "wounded"}},
    {"wound+hunt",   {"wounded", "wounded", "gatehunter", "gatehunter", "hunter"}},
    {"rushers",      {"rusher", "rusher", "rusher", "hunter", "hunter"}},
    {"samples",      {"sample", "sample", "sample", "sample", "sample"}},
    {"mixed",        {"wounded", "sample", "gatehunter", "hunter", "rusher"}},
    {"mirror",       {"ours", "ours", "wounded", "wounded", "rusher"}},
    {"counter",      {"vulture", "exitguard", "brawler", "hunter", "rusher"}},   /* P0을 노리는 카운터 5명 */
    {"counter-gate", {"exitguard", "exitguard", "vulture", "vulture", "brawler"}},
    {"safe-mix",     {"safe", "safe", "wounded", "rusher", "sample"}},
    {"chaos",        {"chaos", "chaos", "chaos", "chaos", "chaos"}},
    {"teams",        {"team1", "team2", "team3", "team4", "team5"}},   /* 팀원 전략 5개 (custom\team1~5.c) */
    {"my-meta",      {"team1", "team2", "wounded", "gatehunter", "rusher"}}, /* 내가 짠 메타: 이 줄을 마음대로 고친다 */
    /* ---- 2026-11 추가: 더 다양한 상황. "*이름"은 판마다 그 풀에서 무작위로 뽑는다(아래 POOLS). 위 11개 + samples/teams 는 그대로. ---- */
    {"rand-all",     {"*all", "*all", "*all", "*all", "*all"}},               /* 누가 올지 모르는 일반 반 */
    {"rand-hard",    {"*hard", "*hard", "*hard", "*hard", "*hard"}},          /* P0을 노리는 쪽으로 편향 */
    {"rand-soft",    {"*soft", "*soft", "*soft", "*soft", "*soft"}},          /* 소극적인 팀 위주 */
    {"hunters",      {"hunter", "hunter", "hunter", "gatehunter", "gatehunter"}},
    {"brawlers",     {"brawler", "brawler", "brawler", "brawler", "brawler"}},
    {"vultures",     {"vulture", "vulture", "vulture", "exitguard", "exitguard"}},
    {"safes",        {"safe", "safe", "safe", "safe", "safe"}},
    {"key-war",      {"rusher", "rusher", "brawler", "vulture", "hunter"}},   /* 열쇠 쟁탈이 치열한 반 */
    {"one-rusher",   {"wounded", "wounded", "safe", "safe", "rusher"}},
    /* ---- 캠핑 상대(교실에서 본 상황): camper = 처음부터 출구 옆을 차지하고 버틴다 ---- */
    {"campers",      {"camper", "camper", "camper", "camper", "camper"}},
    {"camp-3",       {"camper", "camper", "camper", "rusher", "wounded"}},
    {"camp-2",       {"camper", "camper", "rusher", "wounded", "hunter"}},
    {"camp-1",       {"camper", "rusher", "wounded", "gatehunter", "safe"}},
    {"camp-rand",    {"*camp", "*camp", "*camp", "*camp", "*camp"}},
};
#define NSCEN ((int)(sizeof(SCEN) / sizeof(SCEN[0])))

/* 열쇠 소유자의 공격을 "안 한 것"으로 만든다: 복사본으로 이번 턴 이동만 풀어 최종 위치를 구한 뒤(이동은 공격 방향과 무관),
   소유자 부채꼴에 아무도 들지 않는 공격 방향을 고른다. 네 방향 모두 누군가 맞는 경우는 가장 적게 맞는 방향(드묾, 세어서 보고). */
static long key_attack_leaks = 0;
static Command quiet_attack(const Game *g, const Command cmd[PLAYER_COUNT], int owner)
{
    Game t = *g; Command c[PLAYER_COUNT]; int d, j, best = 0, bestn = 99;
    memcpy(c, cmd, sizeof(c)); c[owner] = COMMAND_ATTACK_UP;
    game_resolve(&t, c);
    for (d = 0; d < 4; ++d) {
        int n = 0; Position a = g->players[owner].pos;
        for (j = 0; j < PLAYER_COUNT; ++j) {
            Position e, ctr = {a.x + (d == 2 ? -1 : d == 3 ? 1 : 0), a.y + (d == 0 ? -1 : d == 1 ? 1 : 0)};
            if (j == owner || g->players[j].hp <= 0) continue;
            e = t.players[j].pos;
            if (d < 2 ? (e.y == ctr.y && e.x >= ctr.x - 1 && e.x <= ctr.x + 1) : (e.x == ctr.x && e.y >= ctr.y - 1 && e.y <= ctr.y + 1)) ++n;
        }
        if (n < bestn) { bestn = n; best = d; }
    }
    if (bestn > 0) ++key_attack_leaks;
    return (Command)(COMMAND_ATTACK_UP + best);
}

static unsigned long long rng_state = 1;
static unsigned rnd(unsigned n) /* xorshift64 */
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return (unsigned)(rng_state % n);
}

/* "*이름" 풀: 판마다 이 중 하나를 무작위로 뽑는다. 같은 이름이 여러 번 들어 있으면 그만큼 자주 나온다. */
typedef struct { const char *name; const char *pick[12]; int n; } Pool;
static const Pool POOLS[] = {
    {"*all",  {"rusher", "wounded", "gatehunter", "hunter", "safe", "vulture", "exitguard", "brawler", "sample"}, 9},
    {"*hard", {"vulture", "exitguard", "brawler", "hunter", "gatehunter", "rusher", "safe"}, 7},
    {"*soft", {"wounded", "safe", "safe", "sample", "gatehunter", "rusher"}, 6},
    {"*camp", {"camper", "camper", "rusher", "wounded", "hunter", "gatehunter", "exitguard", "safe"}, 8},
};
static const char *pool_pick(const char *name)
{
    int i;
    if (name[0] != '*') return name;
    for (i = 0; i < (int)(sizeof(POOLS) / sizeof(POOLS[0])); ++i)
        if (!strcmp(POOLS[i].name, name)) return POOLS[i].pick[rnd((unsigned)POOLS[i].n)];
    fprintf(stderr, "unknown pool %s\n", name); exit(1);
}

static Strategy find(const char *name)
{
    int i;
    for (i = 0; i < NSTRAT; ++i) if (!strcmp(STRATS[i].name, name)) return STRATS[i].fn;
    fprintf(stderr, "unknown strategy %s\n", name); exit(1);
}

static int no_key_attack = 1;     /* 열쇠 소유자의 공격 금지(교수님 패치 예정 규칙). --keyattack 으로 끈다 */
static long p0_key_attack_blocked = 0; /* 그중 P0이 낸 것 */
static long key_attack_blocked = 0; /* 막힌 공격 명령 수(상대 전략 포함) */
static int fix_seat = 0; /* --fixseat: 자리를 섞지 않고 맵에 적힌 P0 자리에 내가 선다(교실 시연처럼 자리가 정해진 경우) */
/* --byseat: P0이 앉은 시작 자리(맵의 P0~P5 자리 6곳)와 열쇠 구역(0 좌상 ~ 3 우하, 4 = 맵에 적힌 자리)별 승률 집계 */
static int by_seat = 0, last_seat = 0, last_zone = 4, seat_pos_ok = 0;
static Position seat_pos[NPC_COUNT];
static long seat_n[NPC_COUNT][5], seat_w[NPC_COUNT][5];

/* 시작 위치 섞기: 맵에 적힌 P0~P5 스폰 자리 6곳은 그대로 두고, 어느 플레이어가 어느 자리에 서는지만 섞는다.
   (교수님 방식: 스폰 좌표는 맵 고정, 플레이어 배정만 랜덤) */
static int shuffle_spawns(Game *g)
{
    Position spot[NPC_COUNT];
    int i;
    for (i = 0; i < NPC_COUNT; ++i) spot[i] = g->players[i].spawn;
    if (!seat_pos_ok) { for (i = 0; i < NPC_COUNT; ++i) seat_pos[i] = spot[i]; seat_pos_ok = 1; }
    last_zone = 4;
    for (i = 0; i < NPC_COUNT; ++i) {
        int j = fix_seat ? i : i + (int)rnd(NPC_COUNT - i);
        Position tmp = spot[i]; spot[i] = spot[j]; spot[j] = tmp;
        g->players[i].pos = g->players[i].spawn = spot[i];
    }
    for (i = 0; i < NPC_COUNT; ++i) if (seat_pos[i].x == g->players[0].spawn.x && seat_pos[i].y == g->players[0].spawn.y) last_seat = i;
    /* H0 제거: 영원히 쉬는 시체로 만든다 */
    g->players[HUMAN_PLAYER].hp = 0;
    g->players[HUMAN_PLAYER].rest = UINT64_MAX;
    return 1;
}

/* 열쇠 구역: 교수님이 열쇠를 서로 다른 4구역 중 한 곳에서 시작시킬 수 있다고 해서 만든 옵션.
   네 모서리(좌상, 우상, 좌하, 우하)에서 가장 가까운 시작 자리의 바로 옆 바닥 칸 중 하나에 열쇠를 놓는다.
   --keyzones = 판마다 무작위 구역, --keyzone N = 고정(0 좌상, 1 우상, 2 좌하, 3 우하). 안 주면 맵에 적힌 열쇠 자리 그대로. */
static int key_zone = -2; /* -2 끔, -1 무작위, 0~3 고정 */
/* --keycells "x,y;x,y;..." : 열쇠가 놓일 후보 칸을 직접 지정(판마다 이 중 하나를 무작위로 고른다, 최대 16칸).
   예: v1 지도의 y4·y8 줄 블록의 출구쪽 옆 4칸 --keycells "4,4;8,4;4,8;8,8" (순서: 좌상, 우상, 좌하, 우하 = --byseat 표의 열쇠 열 순서).
   지도 파일 옆에 <지도파일>.keycells (한 줄 "x,y;x,y;...")가 있으면 옵션 없이도 그 칸들을 쓴다(maps\v1_lecture1_topleft.dat.keycells). */
static int key_cells[16][2], n_key_cells = 0;
static int iabs(int v) { return v < 0 ? -v : v; }
static void place_key(Game *g)
{
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    int z = key_zone < 0 ? (int)rnd(4) : key_zone, best = -1, bd = 1 << 30, i, d, n = 0, cand[4][2];
    int tx = (z & 1) ? g->size - 1 : 0, ty = (z & 2) ? g->size - 1 : 0;
    last_zone = z;
    if (n_key_cells > 0) { /* --keycells: 지정한 칸들 중 한 곳 */
        i = (int)rnd((unsigned)n_key_cells);
        g->key.x = g->key_start.x = key_cells[i][0];
        g->key.y = g->key_start.y = key_cells[i][1];
        last_zone = i & 3;
        return;
    }
    for (i = 0; i < NPC_COUNT; ++i) {
        int dd = iabs(g->players[i].spawn.x - tx) + iabs(g->players[i].spawn.y - ty);
        if (dd < bd) { bd = dd; best = i; }
    }
    for (d = 0; d < 4; ++d) {
        int x = g->players[best].spawn.x + dx[d], y = g->players[best].spawn.y + dy[d], k, on_spawn = 0;
        if (x < 0 || y < 0 || x >= g->size || y >= g->size || g->terrain[y][x] != ' ') continue;
        for (k = 0; k < PLAYER_COUNT; ++k) if (g->players[k].spawn.x == x && g->players[k].spawn.y == y) on_spawn = 1;
        if (!on_spawn) { cand[n][0] = x; cand[n][1] = y; ++n; }
    }
    if (!n) return;
    i = (int)rnd((unsigned)n);
    g->key.x = g->key_start.x = cand[i][0];
    g->key.y = g->key_start.y = cand[i][1];
}

/* 지도에 H0 표시가 없으면(H0를 완전히 뺀 지도) 코어 로더가 요구하는 H0를 빈 바닥 칸 하나에 넣은 임시 지도를 %TEMP%에 만들어 그 경로를 돌려준다.
   시뮬레이터는 H0를 영구 사망 처리(경쟁 제외)하므로 경기에는 영향이 없다. H0가 이미 있으면 원래 경로를 그대로 돌려준다. */
static const char *with_h0(const char *path, char *out, size_t cap)
{
    static char buf[MAP_MAX][MAP_MAX * 2 + 4];
    FILE *f = fopen(path, "r"), *w;
    int n = 0, i, x, has = 0, done = 0;
    const char *tmp = getenv("TEMP");
    if (!f) return path;
    while (n < MAP_MAX && fgets(buf[n], (int)sizeof(buf[n]), f)) ++n;
    fclose(f);
    for (i = 0; i < n; ++i) { int len = (int)strlen(buf[i]); for (x = 0; x + 1 < len; x += 2) if (buf[i][x] == 'H' && buf[i][x + 1] == '0') has = 1; }
    if (has || n < 3) return path;
    for (i = 1; i < n - 1 && !done; ++i) {
        int len = (int)strlen(buf[i]);
        for (x = 2; x + 3 < len && !done; x += 2)
            if (buf[i][x] == ' ' && buf[i][x + 1] == ' ') { buf[i][x] = 'H'; buf[i][x + 1] = '0'; done = 1; }
    }
    if (!done) return path;
    snprintf(out, cap, "%s\\sim_tmp_map.dat", tmp ? tmp : ".");
    w = fopen(out, "w");
    if (!w) return path;
    for (i = 0; i < n; ++i) fputs(buf[i], w);
    fclose(w);
    return out;
}

/* "x,y;x,y;..." 를 열쇠 후보 칸 목록(key_cells)에 읽는다. --keycells 값과 지도 옆 .keycells 파일에서 같이 쓴다. */
static void parse_keycells(const char *q)
{
    int cx, cy, used = 0;
    while (n_key_cells < 16 && sscanf(q, "%d,%d%n", &cx, &cy, &used) == 2) {
        key_cells[n_key_cells][0] = cx; key_cells[n_key_cells][1] = cy; ++n_key_cells;
        q += used; while (*q == ';' || *q == ' ') ++q;
    }
}

/* --show 용 텍스트 보드 (K숫자 = 열쇠 소유자) */
static void show(const Game *g, const char *names[NPC_COUNT])
{
    int x, y, i;
    printf("turn %d key_owner %d\n", g->turn, g->key_owner);
    for (y = 0; y < g->size; ++y) {
        for (x = 0; x < g->size; ++x) {
            const char *t = g->terrain[y][x] == '#' ? "##" : g->terrain[y][x] == 'E' ? "()" : "  ";
            char buf[3] = {0};
            if (g->key_owner < 0 && g->key.x == x && g->key.y == y) t = "@-";
            for (i = 0; i < NPC_COUNT; ++i)
                if (g->players[i].hp > 0 && g->players[i].pos.x == x && g->players[i].pos.y == y) {
                    buf[0] = g->key_owner == i ? 'K' : 'P'; buf[1] = (char)('0' + i); t = buf;
                }
            if (g->attack[y][x] && !strcmp(t, "  ")) t = "**";
            fputs(t, stdout);
        }
        putchar('\n');
    }
    for (i = 0; i < NPC_COUNT; ++i)
        printf(" P%d(%s) hp%d d%u r%llu%s\n", i, names[i], g->players[i].hp, g->players[i].deaths,
            (unsigned long long)g->players[i].rest, g->cancelled[i] ? " [취소]" : g->damaged[i] ? " [피격]" : "");
    putchar('\n');
}

typedef struct {
    int games, wins, draws, turns_sum, deaths, cancels; double max_ms; int win_by[NSTRAT];
    int held, carrier_deaths, lost_never_held, lost_dead; /* 패배 원인 분석 */
    int cancel_move, cancel_pick; /* 취소된 P0 명령 종류 */
    int hit_cancel, hit_attack, die_cancel, die_attack; /* P0이 처음 맞은 원인(HP2->1) / 죽은 원인(HP1->0): 명령 취소 vs 공격 */
} Stats;

static int strat_index(const char *name) { int i; for (i = 0; i < NSTRAT; ++i) if (!strcmp(STRATS[i].name, name)) return i; return -1; }

static long key_drops, key_rule_violations; /* 열쇠 드롭 규칙 검사 횟수 / 위반 횟수 */
static int watch_ms = -1; /* --watch: 교수님 display.c로 턴마다 렌더링 (ms 간격), -1이면 끔 */

/* 한 판 진행. 반환: 승자 seat (-1 무승부) */
static int play(Game *g, Strategy seat[NPC_COUNT], const char *names[NPC_COUNT], int max_turns, int verbose, Stats *st)
{
    char path[MAX_PATH];
    LARGE_INTEGER f, a, b;
    int ever_held = 0;
    QueryPerformanceFrequency(&f);
    while (g->turn < max_turns && g->winner < 0) {
        Command cmd[PLAYER_COUNT];
        int i, had_key;
        game_begin_turn(g);
        had_key = g->key_owner == 0;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            cmd[i] = (Command)-1;
            snprintf(path, sizeof(path), "mem:%d", i);
            if (!snapshot_write(g, i, path)) { fputs("snapshot write failed\n", stderr); exit(1); }
        }
        for (i = 0; i < NPC_COUNT; ++i) {
            double ms; int open_before;
            if (g->players[i].hp <= 0) continue;
            snprintf(path, sizeof(path), "mem:%d", i);
            open_before = sim_open_files; /* AI 호출 전후로 열린 파일 수가 늘면 fclose 누락 */
            QueryPerformanceCounter(&a);
            cmd[i] = seat[i](path);
            QueryPerformanceCounter(&b);
            ms = (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart;
            if (i == 0 && ms > st->max_ms) st->max_ms = ms;
            if (sim_open_files != open_before) {
                fprintf(stderr, "P%d(%s) FILE LEAK turn=%d\n", i, names[i], g->turn);
                sim_open_files = open_before;
            }
        }
        {
            /* 규칙 검사 (Rule.txt 8절): 열쇠 소유자가 죽으면 죽은 칸에 열쇠가 떨어진다. 출구에서 죽으면 최초 @- 자리로. */
            int owner = g->key_owner;
            /* 교수님 패치 예정: 열쇠를 든 사람은 공격할 수 없다(공격해도 아무 일도 일어나지 않음, 피해도 없음).
               코어 game.c는 그대로 두고, 소유자의 공격을 "아무도 맞지 않는 방향"으로 돌려 효과가 없게 한다. --keyattack 이면 예전 규칙. */
            if (no_key_attack && owner >= 0 && cmd[owner] >= COMMAND_ATTACK_UP && cmd[owner] <= COMMAND_ATTACK_RIGHT) { cmd[owner] = quiet_attack(g, cmd, owner); ++key_attack_blocked; if (owner == 0) ++p0_key_attack_blocked; }
            { int hp0 = g->players[0].hp; game_resolve(g, cmd);
              if (hp0 > 0 && g->players[0].hp < hp0) { /* P0이 피해를 입은 턴: 취소가 섞였으면 취소, 아니면 공격 */
                  if (hp0 == MAX_HP) { if (g->cancelled[0]) ++st->hit_cancel; else ++st->hit_attack; }
                  else { if (g->cancelled[0]) ++st->die_cancel; else ++st->die_attack; } } }
            if (owner >= 0 && g->players[owner].hp <= 0) {
                Position p = g->players[owner].pos, want = (p.x == g->exit.x && p.y == g->exit.y) ? g->key_start : p;
                ++key_drops;
                if (g->key_owner != -1 || g->key.x != want.x || g->key.y != want.y) {
                    ++key_rule_violations;
                    fprintf(stderr, "KEY RULE VIOLATION turn %d: P%d died at (%d,%d), key at (%d,%d) owner %d\n",
                        g->turn, owner, p.x, p.y, g->key.x, g->key.y, g->key_owner);
                }
            }
        }
        if (g->cancelled[0]) { ++st->cancels; if (cmd[0] < COMMAND_ATTACK_UP) ++st->cancel_move; else if (cmd[0] >= COMMAND_PICK_UP && cmd[0] <= COMMAND_PICK_RIGHT) ++st->cancel_pick; }
        if (g->key_owner == 0) ever_held = 1;
        if (had_key && g->players[0].hp <= 0) ++st->carrier_deaths;
        if (verbose) show(g, names);
        if (watch_ms >= 0) {
            /* 실제 게임(main.c)과 같은 화면·타이밍: 공격이 있으면 1초 효과 화면, 나머지는 결과 화면 */
            char legend[256]; int attacks = 0, n = 0;
            for (i = 0; i < NPC_COUNT; ++i)
                n += snprintf(legend + n, sizeof(legend) - n, "P%d=%s%s ", i, names[i], i == 0 ? "(P0)" : "");
            for (i = 0; i < PLAYER_COUNT; ++i)
                if (g->last[i] >= COMMAND_ATTACK_UP && g->last[i] <= COMMAND_ATTACK_RIGHT && !g->cancelled[i] &&
                    (g->players[i].hp > 0 || g->damaged[i])) attacks = 1;
            if (attacks && watch_ms > 0) {
                display_game(g, legend, 1);
                Sleep(watch_ms / 2);
            }
            display_game(g, legend, 0);
            if (watch_ms > 0) Sleep(attacks ? watch_ms - watch_ms / 2 : watch_ms);
        }
    }
    st->deaths += (int)g->players[0].deaths;
    st->held += ever_held;
    if (g->winner > 0) {
        if (!ever_held) ++st->lost_never_held;
        if (g->players[0].hp <= 0) ++st->lost_dead;
    }
    return g->winner;
}

int main(int argc, char **argv)
{
    /* 맵은 교수님 맵 하나로 고정 (실제 시험 환경과 같다). 바꾸고 싶으면 아래 상수만 고친다. */
    const char *MAP_FILE = "../FinalExamGameCore/dungeonmap.dat";
    int games = 200, max_turns = 200, verbose = 0, i, sc, gm;
    const char *only = "all", *report_path = NULL, *label = "", *ours = "ours";
    const char *teams_arg = NULL; Scenario custom; const Scenario *scs = SCEN; int nsc = NSCEN;
    FILE *rep = NULL;
    unsigned long long seed = (unsigned long long)time(NULL);
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = _strtoui64(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--scenario") && i + 1 < argc) only = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc) MAP_FILE = argv[++i]; /* 다른 지도로 시험: maps\m1_open4.dat 등 */
        else if (!strcmp(argv[i], "--keyattack")) no_key_attack = 0; /* 예전 규칙: 열쇠 소유자도 공격 가능 */
        else if (!strcmp(argv[i], "--fixseat")) fix_seat = 1; /* P0 자리를 맵에 적힌 자리로 고정 */
        else if (!strcmp(argv[i], "--byseat")) by_seat = 1; /* 끝에 시작 자리별/열쇠 구역별 승률표를 낸다 */
        else if (!strcmp(argv[i], "--keycells") && i + 1 < argc) { /* 열쇠 후보 칸을 직접 지정: "1,5;11,5;1,8;11,8" */
            const char *q = argv[++i]; int cx, cy, used = 0;
            while (n_key_cells < 16 && sscanf(q, "%d,%d%n", &cx, &cy, &used) == 2) {
                key_cells[n_key_cells][0] = cx; key_cells[n_key_cells][1] = cy; ++n_key_cells;
                q += used; while (*q == ';' || *q == ' ') ++q;
            }
            key_zone = -1;
        }
        else if (!strcmp(argv[i], "--keyzones")) key_zone = -1; /* 열쇠를 판마다 무작위 모서리 구역에 놓는다 */
        else if (!strcmp(argv[i], "--keyzone") && i + 1 < argc) key_zone = atoi(argv[++i]) & 3; /* 열쇠 구역 고정: 0 좌상, 1 우상, 2 좌하, 3 우하 */
        else if (!strcmp(argv[i], "--max-turns") && i + 1 < argc) max_turns = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--show")) verbose = 1;
        else if (!strcmp(argv[i], "--watch") && i + 1 < argc) watch_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) report_path = argv[++i];
        else if (!strcmp(argv[i], "--label") && i + 1 < argc) label = argv[++i];
        else if (!strcmp(argv[i], "--teams") && i + 1 < argc) teams_arg = argv[++i]; /* 6팀 직접 지정: 이름6개를 쉼표로 (첫 번째가 기준 팀) */
        else if (!strcmp(argv[i], "--ours") && i + 1 < argc) ours = argv[++i]; /* P0 자리에 다른 전략 (기준선 비교용) */
        else { fprintf(stderr, "bad arg %s\n", argv[i]); return 1; }
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* H0가 없는 지도도 읽을 수 있게 하고, 지도 옆에 <지도파일>.keycells 가 있으면 열쇠 후보 칸을 거기서 읽는다(--keycells 가 우선) */
        static char map_tmp[512], kc_path[600];
        FILE *kf;
        snprintf(kc_path, sizeof(kc_path), "%s.keycells", MAP_FILE);
        MAP_FILE = with_h0(MAP_FILE, map_tmp, sizeof(map_tmp));
        if (n_key_cells == 0 && (kf = fopen(kc_path, "r")) != NULL) {
            char line[256];
            if (fgets(line, (int)sizeof(line), kf)) { parse_keycells(line); if (n_key_cells > 0 && key_zone == -2) key_zone = -1; }
            fclose(kf);
        }
    }
    if (teams_arg) {
        static char buf[512]; char *tok, *ctx = NULL; int n = 0; const char *nm[6];
        snprintf(buf, sizeof(buf), "%s", teams_arg);
        for (tok = strtok_s(buf, ",", &ctx); tok && n < 7; tok = strtok_s(NULL, ",", &ctx)) { if (n < 6) nm[n] = tok; ++n; }
        if (n != 6) { fprintf(stderr, "--teams 에는 팀 이름 6개를 쉼표로 적어야 합니다 (받은 개수 %d)\n", n); return 1; }
        for (i = 0; i < 6; ++i) find(nm[i]); /* 모르는 이름이면 여기서 종료 */
        ours = nm[0];
        custom.name = "custom"; for (i = 0; i < 5; ++i) custom.opp[i] = nm[i + 1];
        scs = &custom; nsc = 1; only = "all";
    }
    if (watch_ms >= 0) display_init(0);
    rng_state = seed ? seed : 1;
    printf("seed=%llu games/scenario=%d map=%s (고정, 스폰 자리 고정 / 플레이어 배정만 랜덤)\n", seed, games, MAP_FILE);
    printf("%-12s %6s %6s %6s %7s %7s %8s  winners\n", "scenario", "win%", "draw%", "avgT", "deaths", "cancel", "max_ms");
    /* --report: 실행할 때마다 마크다운 기록표를 파일 끝에 덧붙인다 */
    if (report_path) {
        char when[32]; time_t now = time(NULL); struct tm lt;
        localtime_s(&lt, &now); strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &lt);
        rep = fopen(report_path, "a");
        if (!rep) { fprintf(stderr, "cannot open report %s\n", report_path); return 1; }
        fprintf(rep, "\n## %s %s\n\n- P0 전략: %s / 맵: %s (고정) / 시나리오당 %d판 / seed %llu / 최대 %d턴 / 스폰 자리 고정, 플레이어 배정만 랜덤 / H0 제외\n\n",
            when, label, strcmp(ours, "ours") ? ours : "player0.c (P0)", MAP_FILE, games, seed, max_turns);
        fprintf(rep, "| 시나리오 | 상대 구성 | P0 승률 | 무승부 | 평균 승리 턴 | 판당 사망 | 판당 취소 | 최대 ms | 승자 분포 |\n");
        fprintf(rep, "|---|---|---:|---:|---:|---:|---:|---:|---|\n");
    }
    for (sc = 0; sc < nsc; ++sc) {
        Stats st;
        if (strcmp(only, "all") && strcmp(only, scs[sc].name)) continue;
        memset(&st, 0, sizeof(st));
        for (gm = 0; gm < games; ++gm) {
            static Game g;
            Strategy seat[NPC_COUNT]; const char *names[NPC_COUNT]; int w, k;
            FILE *f = fopen(MAP_FILE, "r"); char err[256];
            if (!f) { fprintf(stderr, "map open fail %s\n", MAP_FILE); return 1; }
            if (!game_load(&g, f, err, sizeof(err))) { fclose(f); fprintf(stderr, "map load fail %s: %s\n", MAP_FILE, err); return 1; }
            fclose(f);
            if (!shuffle_spawns(&g)) return 1;
            if (key_zone > -2) place_key(&g);
            /* 상대 5명을 P1~P5 자리에 무작위 배치 */
            names[0] = ours;
            for (k = 0; k < 5; ++k) names[k + 1] = pool_pick(scs[sc].opp[k]);
            for (k = 1; k < NPC_COUNT; ++k) { int j = k + (int)rnd(NPC_COUNT - k); const char *t = names[k]; names[k] = names[j]; names[j] = t; }
            for (k = 0; k < NPC_COUNT; ++k) seat[k] = find(names[k]);
            w = play(&g, seat, names, max_turns, verbose, &st);
            ++st.games;
            if (by_seat && strcmp(scs[sc].name, "safes") && strcmp(scs[sc].name, "samples") && strcmp(scs[sc].name, "teams") && strcmp(scs[sc].name, "campers")) {
                ++seat_n[last_seat][last_zone];
                if (w == 0) ++seat_w[last_seat][last_zone];
            }
            if (w < 0) ++st.draws;
            else { st.win_by[strat_index(names[w])]++; if (w == 0) { ++st.wins; st.turns_sum += g.turn; } }
            if (verbose || watch_ms >= 0) printf("== result: %s (%s)\n", w < 0 ? "DRAW" : "WIN", w < 0 ? "-" : names[w]);
        }
        printf("%-12s %5.1f%% %5.1f%% %6.1f %7.2f %7.2f %8.2f ", scs[sc].name, 100.0 * st.wins / st.games,
            100.0 * st.draws / st.games, st.wins ? (double)st.turns_sum / st.wins : 0.0,
            (double)st.deaths / st.games, (double)st.cancels / st.games, st.max_ms);
        for (i = 0; i < NSTRAT; ++i) if (st.win_by[i]) printf(" %s:%d", STRATS[i].name, st.win_by[i]);
        printf("  | held:%d carrierDeath:%d lossNeverHeld:%d lossWhileDead:%d", st.held, st.carrier_deaths, st.lost_never_held, st.lost_dead);
        printf(" | 취소 이동:%d 줍기:%d | 첫피격 취소:%d 공격:%d 사망 취소:%d 공격:%d", st.cancel_move, st.cancel_pick, st.hit_cancel, st.hit_attack, st.die_cancel, st.die_attack);
        putchar('\n');
        if (rep) {
            fprintf(rep, "| %s | %s, %s, %s, %s, %s | **%.1f%%** | %.1f%% | %.1f | %.2f | %.2f | %.2f |", scs[sc].name,
                scs[sc].opp[0], scs[sc].opp[1], scs[sc].opp[2], scs[sc].opp[3], scs[sc].opp[4],
                100.0 * st.wins / st.games, 100.0 * st.draws / st.games, st.wins ? (double)st.turns_sum / st.wins : 0.0,
                (double)st.deaths / st.games, (double)st.cancels / st.games, st.max_ms);
            for (i = 0; i < NSTRAT; ++i) if (st.win_by[i]) fprintf(rep, " %s %d", STRATS[i].name, st.win_by[i]);
            fprintf(rep, " |\n");
        }
    }
    if (rep) fclose(rep);
    if (by_seat) {
        int s2, z2;
        printf("\n== 시작 자리별 승률 (safes/samples/teams/campers 제외한 시나리오 합산)\n");
        printf("자리(x,y)   판수   승률 | 열쇠 좌상  우상  좌하  우하  맵\n");
        for (s2 = 0; s2 < NPC_COUNT; ++s2) {
            long n = 0, wv = 0;
            for (z2 = 0; z2 < 5; ++z2) { n += seat_n[s2][z2]; wv += seat_w[s2][z2]; }
            printf("(%2d,%2d) %8ld %5.1f%% |", seat_pos[s2].x, seat_pos[s2].y, n, n ? 100.0 * wv / n : 0.0);
            for (z2 = 0; z2 < 5; ++z2) { if (seat_n[s2][z2]) printf(" %5.1f", 100.0 * seat_w[s2][z2] / seat_n[s2][z2]); else printf("     -"); }
            putchar('\n');
        }
    }
    printf("key-drop rule check: %ld drops, %ld violations\n", key_drops, key_rule_violations);
    printf("열쇠 소유자 공격 금지: %s, 막힌 공격 명령 %ld개 (그중 P0 %ld개, 사방이 막혀 못 피한 것 %ld개)\n", no_key_attack ? "켬" : "끔", key_attack_blocked, p0_key_attack_blocked, key_attack_leaks);
    return 0;
}
