/* 전략 시뮬레이터. 교수님 코어(game.c/snapshot.c)를 그대로 링크해서 규칙은 100% 동일하다.
   차이점: (1) AI를 별도 프로세스 대신 같은 프로세스에서 호출해 빠르게 수천 판을 돌린다.
          (2) H0는 영구 사망 처리(최종 대회와 동일하게 6명만 경쟁).
          (3) 매 판 P0~P5 시작 위치를 무작위로 섞는다(교수님 공지 반영).
   (4) 맵은 기본으로 교수님 맵(FinalExamGameCore\dungeonmap.dat). --map 으로 다른 맵 파일을 줄 수 있다(StrategySim\maps 에 예시 5개).
       스폰 자리는 맵에 적힌 대로 고정이고, 어느 플레이어가 어느 자리에 서는지만 판마다 바뀐다.
   사용: sim.exe [--games N] [--seed S] [--scenario 이름|all] [--map 파일] [--show] [--watch ms]
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

/* 시작 위치 섞기: 맵에 적힌 P0~P5 스폰 자리 6곳은 그대로 두고, 어느 플레이어가 어느 자리에 서는지만 섞는다.
   (교수님 방식: 스폰 좌표는 맵 고정, 플레이어 배정만 랜덤) */
static int shuffle_spawns(Game *g)
{
    Position spot[NPC_COUNT];
    int i;
    for (i = 0; i < NPC_COUNT; ++i) spot[i] = g->players[i].spawn;
    for (i = 0; i < NPC_COUNT; ++i) {
        int j = i + (int)rnd(NPC_COUNT - i);
        Position tmp = spot[i]; spot[i] = spot[j]; spot[j] = tmp;
        g->players[i].pos = g->players[i].spawn = spot[i];
    }
    /* H0 제거: 영원히 쉬는 시체로 만든다 */
    g->players[HUMAN_PLAYER].hp = 0;
    g->players[HUMAN_PLAYER].rest = UINT64_MAX;
    return 1;
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
            /* 상대 5명을 P1~P5 자리에 무작위 배치 */
            names[0] = ours;
            for (k = 0; k < 5; ++k) names[k + 1] = pool_pick(scs[sc].opp[k]);
            for (k = 1; k < NPC_COUNT; ++k) { int j = k + (int)rnd(NPC_COUNT - k); const char *t = names[k]; names[k] = names[j]; names[j] = t; }
            for (k = 0; k < NPC_COUNT; ++k) seat[k] = find(names[k]);
            w = play(&g, seat, names, max_turns, verbose, &st);
            ++st.games;
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
    printf("key-drop rule check: %ld drops, %ld violations\n", key_drops, key_rule_violations);
    return 0;
}
