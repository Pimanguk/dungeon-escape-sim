/* FinalExam01 전략 시뮬레이터 (헤드리스).
   - ../FinalExam01 의 game.c / snapshot.c 규칙 코어를 수정 없이 그대로 링크한다. (규칙 100% 동일)
   - 우리 팀 P4 = ../FinalExam01/player4.c 의 player4_choose_command 가 항상 4번 자리(맵의 P4 시작점)에 선다.
   - 나머지 5개 자리(0,1,2,3,5)에는 상대 풀에서 뽑은 전략이 판마다 무작위로 섞여 선다. (--fixseat 이면 고정)
   - 리눅스/맥/WSL: 매 호출을 fork 한 자식 프로세스에서 실행해 실제 게임처럼 전역/static 상태가 유지되지 않게 한다.
     윈도우(MSVC): 같은 프로세스에서 호출하므로 static 상태를 쓰는 전략은 실제와 달라질 수 있다.
   사용: sim [--games N] [--seed S] [--map 파일] [--opp a,b,c,d,e] [--pool 이름들] [--fixseat] [--show] [--list]
        [--ours 이름]   우리 자리에 다른 전략을 세워 비교 (기본 ours)
   예) sim --games 100 --seed 1 --opp rusher,camper,hunter,safe,exitguard */
#include "game.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define SIM_PID() ((int)_getpid())
#else
#include <unistd.h>
#include <sys/wait.h>
#define SIM_PID() ((int)getpid())
#endif

typedef Command (*Strategy)(const char *);
Command player4_choose_command(const char *);
Command strat_rusher(const char *), strat_wounded(const char *), strat_gatehunter(const char *);
Command strat_hunter(const char *), strat_safe(const char *), strat_vulture(const char *);
Command strat_exitguard(const char *), strat_camper(const char *), strat_brawler(const char *), strat_chaos(const char *);
#ifdef WITH_OLD
Command old0_choose_command(const char *), old1_choose_command(const char *), old2_choose_command(const char *);
Command old3_choose_command(const char *), old4_choose_command(const char *), old5_choose_command(const char *);
#endif
/* 모든 칸이 같은 전략 호출 규약: 상태 파일 경로 -> Command */
typedef struct { const char *name; Strategy fn; int in_pool; } Entry;
static const Entry TABLE[] = {
    {"ours", player4_choose_command, 0},
    {"rusher", strat_rusher, 1}, {"wounded", strat_wounded, 1}, {"gatehunter", strat_gatehunter, 1},
    {"hunter", strat_hunter, 1}, {"safe", strat_safe, 1}, {"vulture", strat_vulture, 1},
    {"exitguard", strat_exitguard, 1}, {"camper", strat_camper, 1}, {"brawler", strat_brawler, 1},
    {"chaos", strat_chaos, 1},
#ifdef WITH_OLD
    /* 교수님 저장소 FinalExam00 의 이전 조 코드. build.sh 가 OLD_DIR 로 받아 이름만 바꿔 컴파일한다. */
    {"old0", old0_choose_command, 1}, {"old1", old1_choose_command, 1}, {"old2", old2_choose_command, 1},
    {"old3", old3_choose_command, 1}, {"old4", old4_choose_command, 1}, {"old5", old5_choose_command, 1},
#endif
};
#define NT ((int)(sizeof(TABLE) / sizeof(TABLE[0])))

static unsigned long long rng_state = 88172645463325252ULL;
static unsigned rnd(void)
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return (unsigned)(rng_state >> 11);
}

static int find(const char *name)
{
    int i;
    for (i = 0; i < NT; ++i) if (!strcmp(TABLE[i].name, name)) return i;
    return -1;
}

static Command call(Strategy fn, const char *snap)
{
#ifdef _WIN32
    return fn(snap);
#else
    pid_t pid = fork();
    int st = 0;
    if (pid == 0) {
        Command r;
        alarm(5);
        if (!freopen("/dev/null", "w", stderr)) {}
        r = fn(snap);
        _exit(20 + (int)r);
    }
    if (pid < 0) return fn(snap);
    waitpid(pid, &st, 0);
    if (WIFEXITED(st) && WEXITSTATUS(st) >= 20 && WEXITSTATUS(st) < 20 + COMMAND_COUNT) return (Command)(WEXITSTATUS(st) - 20);
    return (Command)-1; /* 크래시/시간초과 = 무효 명령 */
#endif
}

int main(int argc, char **argv)
{
    const char *map = "../FinalExam01/dungeonmap.dat";
    const char *oppArg = NULL, *oursName = "ours";
    int games = 30, seed = 1, fixseat = 0, show = 0, i, g;
    int poolIdx[64], npool = 0, oursIdx;
    long rankHist[8] = {0}, wins = 0, top3 = 0, draws = 0, rankSum = 0, finished = 0, turnSum = 0, invalid = 0;
    Game base;
    char err[256];
    FILE *f;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--map") && i + 1 < argc) map = argv[++i];
        else if (!strcmp(argv[i], "--opp") && i + 1 < argc) oppArg = argv[++i];
        else if (!strcmp(argv[i], "--ours") && i + 1 < argc) oursName = argv[++i];
        else if (!strcmp(argv[i], "--fixseat")) fixseat = 1;
        else if (!strcmp(argv[i], "--show")) show = 1;
        else if (!strcmp(argv[i], "--list")) { for (g = 0; g < NT; ++g) printf("%s\n", TABLE[g].name); return 0; }
        else { fprintf(stderr, "알 수 없는 옵션: %s (--list 로 전략 이름 확인)\n", argv[i]); return 1; }
    }
    oursIdx = find(oursName);
    if (oursIdx < 0) { fprintf(stderr, "전략 이름 없음: %s\n", oursName); return 1; }
    for (i = 0; i < NT; ++i) if (TABLE[i].in_pool) poolIdx[npool++] = i;
    f = fopen(map, "r");
    if (!f || !game_load(&base, f, err, sizeof err)) { fprintf(stderr, "맵 오류(%s): %s\n", map, f ? err : "열 수 없음"); return 1; }
    fclose(f);
    rng_state ^= (unsigned long long)seed * 0x9E3779B97F4A7C15ULL;
    for (i = 0; i < 10; ++i) rnd();

    for (g = 0; g < games; ++g) {
        int team[PLAYER_COUNT], slots[5] = {0, 1, 2, 3, 5}, opp[5], n, k;
        Game game;
        char dir_snap[PLAYER_COUNT][160];
        int bad = 0;
        /* 상대 5명 뽑기 */
        if (oppArg) {
            char buf[256], *tok;
            strncpy(buf, oppArg, sizeof buf - 1); buf[sizeof buf - 1] = 0;
            n = 0;
            for (tok = strtok(buf, ","); tok && n < 5; tok = strtok(NULL, ",")) {
                opp[n] = find(tok);
                if (opp[n] < 0) { fprintf(stderr, "전략 이름 없음: %s\n", tok); return 1; }
                ++n;
            }
            if (n != 5) { fprintf(stderr, "--opp 는 정확히 5개 이름이 필요합니다.\n"); return 1; }
        } else {
            int used[64] = {0};
            for (n = 0; n < 5; ++n) {
                do k = (int)(rnd() % (unsigned)npool); while (used[k] && npool >= 5);
                used[k] = 1; opp[n] = poolIdx[k];
            }
        }
        if (!fixseat)
            for (k = 4; k > 0; --k) { int j = (int)(rnd() % (unsigned)(k + 1)), t = opp[k]; opp[k] = opp[j]; opp[j] = t; }
        for (k = 0; k < 5; ++k) team[slots[k]] = opp[k];
        team[4] = oursIdx;

        game = base;
        for (i = 0; i < PLAYER_COUNT; ++i) snprintf(dir_snap[i], sizeof dir_snap[i], "_sim_%d_p%d.dat", SIM_PID(), i);
        while (!game.game_over && game.turn < 1200) {
            Command cmd[PLAYER_COUNT];
            game_begin_turn(&game);
            for (i = 0; i < PLAYER_COUNT; ++i) snapshot_write(&game, i, dir_snap[i]);
            for (i = 0; i < PLAYER_COUNT; ++i) {
                cmd[i] = (Command)-1;
                if (game.players[i].hp <= 0 || game.players[i].rank) continue;
                cmd[i] = call(TABLE[team[i]].fn, dir_snap[i]);
                if ((int)cmd[i] < 0 && i == 4) ++bad;
            }
            game_resolve(&game, cmd);
            if (show) {
                printf("T%d key=%d", game.turn, game.key_owner);
                for (i = 0; i < PLAYER_COUNT; ++i) printf(" | %s(%d,%d)hp%d", TABLE[team[i]].name, game.players[i].pos.x, game.players[i].pos.y, game.players[i].hp);
                puts("");
            }
        }
        invalid += bad;
        turnSum += game.turn;
        if (game.draw) { ++draws; }
        else {
            int r = game.players[4].rank;
            if (r < 1 || r > 6) r = 7;
            rankHist[r]++;
            ++finished;
            rankSum += r;
            if (r == 1) ++wins;
            if (r <= 3) ++top3;
        }
        printf("game %3d turns %4d %s ours=%s rank=%d  seats:", g + 1, game.turn, game.draw ? "DRAW" : "    ",
               TABLE[oursIdx].name, game.players[4].rank);
        for (i = 0; i < PLAYER_COUNT; ++i) printf(" P%d=%s", i, TABLE[team[i]].name);
        puts("");
        fflush(stdout);
    }
    for (i = 0; i < PLAYER_COUNT; ++i) { char p[160]; snprintf(p, sizeof p, "_sim_%d_p%d.dat", SIM_PID(), i); remove(p); }
    printf("\n== 요약 (%s, %d판, seed %d) ==\n", TABLE[oursIdx].name, games, seed);
    printf("무승부 %ld판 | 평균 턴 %.0f | 우리 무효명령(크래시/시간초과) %ld회\n", draws, games ? (double)turnSum / games : 0.0, invalid);
    if (finished) {
        printf("1위 %.1f%% | 3위 이내 %.1f%% | 평균 순위 %.2f | 꼴찌(6위) %.1f%%\n", 100.0 * wins / finished, 100.0 * top3 / finished,
               (double)rankSum / finished, 100.0 * rankHist[6] / finished);
        printf("순위 분포:");
        for (i = 1; i <= 6; ++i) printf(" %d위=%ld", i, rankHist[i]);
        if (rankHist[7]) printf(" 미확정=%ld", rankHist[7]);
        puts("");
    }
    return 0;
}
