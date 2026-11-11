/* 시뮬레이션 전용 상대 전략들. 제출용이 아니므로 헬퍼 함수를 자유롭게 쓴다.
   모든 전략은 실제 코어와 똑같이 상태 파일 경로만 받고, static 상태를 남기지 않는다. */
#include "../FinalExamGameCore/player_api.h"

static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
#define FAR 9999

/* 상태 파일을 읽는다. 실패하면 0. */
static int load(const char *path, Snapshot *s)
{
    FILE *f = fopen(path, "r");
    int ok;
    if (!f) return 0;
    ok = snapshot_read(f, s);
    fclose(f);
    return ok;
}

static int inb(const Snapshot *s, int x, int y) { return x >= 0 && y >= 0 && x < s->size && y < s->size; }

/* 걸어서 들어갈 수 있는 칸인가 (출구는 열쇠 소유자만) */
static int walk(const Snapshot *s, int x, int y, int carrier)
{
    return inb(s, x, y) && s->terrain[y][x] != '#' && (s->terrain[y][x] != 'E' || carrier);
}

/* (x,y)에 서 있는 살아 있는 플레이어 번호, 없으면 -1 */
static int occupant(const Snapshot *s, int x, int y)
{
    int i;
    for (i = 0; i < PLAYER_COUNT; ++i)
        if (s->players[i].hp > 0 && s->players[i].pos.x == x && s->players[i].pos.y == y) return i;
    return -1;
}

/* (tx,ty)에서 시작하는 BFS 거리표. 시작 칸은 지형과 무관하게 0. */
static void bfs(const Snapshot *s, int tx, int ty, int carrier, int dist[MAP_MAX][MAP_MAX])
{
    int qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX], h = 0, t = 0, x, y, d;
    for (y = 0; y < MAP_MAX; ++y) for (x = 0; x < MAP_MAX; ++x) dist[y][x] = FAR;
    dist[ty][tx] = 0; qx[t] = tx; qy[t++] = ty;
    while (h < t) {
        x = qx[h]; y = qy[h++];
        for (d = 0; d < 4; ++d) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!walk(s, nx, ny, carrier) || dist[ny][nx] != FAR) continue;
            dist[ny][nx] = dist[y][x] + 1; qx[t] = nx; qy[t++] = ny;
        }
    }
}

/* 거리표를 줄이는 빈 칸 방향. 없으면 -1. */
static int step(const Snapshot *s, int dist[MAP_MAX][MAP_MAX])
{
    Position me = s->players[s->self].pos;
    int carrier = s->key_owner == s->self, d, best = -1, bd = dist[me.y][me.x];
    for (d = 0; d < 4; ++d) {
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!walk(s, nx, ny, carrier) || occupant(s, nx, ny) >= 0) continue;
        if (dist[ny][nx] < bd) { bd = dist[ny][nx]; best = d; }
    }
    return best;
}

/* 내 위치에서 (tx,ty)를 맞히는 공격 방향 (8방향 인접일 때만), 아니면 -1 */
static int attack_dir(Position me, int tx, int ty)
{
    int ax = tx - me.x, ay = ty - me.y;
    if (ax < -1 || ax > 1 || ay < -1 || ay > 1 || (ax == 0 && ay == 0)) return -1;
    if (ay == -1) return 0;
    if (ay == 1) return 1;
    return ax < 0 ? 2 : 3;
}

/* 인접(8방향) 적 공격. 우선순위: 열쇠 소유자 > HP 1 > 아무나. */
static int attack_adjacent(const Snapshot *s)
{
    Position me = s->players[s->self].pos;
    int i, best = -1, bs = -1;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int d, sc;
        if (i == s->self || s->players[i].hp <= 0) continue;
        d = attack_dir(me, s->players[i].pos.x, s->players[i].pos.y);
        if (d < 0) continue;
        sc = (i == s->key_owner ? 10 : 0) + (s->players[i].hp == 1 ? 5 : 0);
        if (sc > bs) { bs = sc; best = d; }
    }
    return best < 0 ? -1 : COMMAND_ATTACK_UP + best;
}

/* 열쇠 줍기 가능하면 그 명령 */
static int try_pick(const Snapshot *s)
{
    Position me = s->players[s->self].pos;
    int d;
    if (s->key_owner != -1) return -1;
    for (d = 0; d < 4; ++d)
        if (me.x + DX[d] == s->key.x && me.y + DY[d] == s->key.y) return COMMAND_PICK_UP + d;
    return -1;
}

/* 가장 흔한 학생 AI: 열쇠로 직진, 들었으면 출구로, 남이 들었으면 쫓아가 공격 */
static Command rusher_logic(const Snapshot *s)
{
    int dist[MAP_MAX][MAP_MAX], c, d;
    Position me = s->players[s->self].pos;
    if (s->key_owner >= 0 && s->key_owner != s->self) {
        Position k = s->players[s->key_owner].pos;
        d = attack_dir(me, k.x, k.y);
        if (d >= 0) return (Command)(COMMAND_ATTACK_UP + d);
    }
    if ((c = try_pick(s)) >= 0) return (Command)c;
    if (s->key_owner == s->self) bfs(s, s->exit.x, s->exit.y, 1, dist);
    else if (s->key_owner == -1) bfs(s, s->key.x, s->key.y, 0, dist);
    else bfs(s, s->players[s->key_owner].pos.x, s->players[s->key_owner].pos.y, 0, dist);
    d = step(s, dist);
    if (d >= 0) return (Command)d;
    if ((c = attack_adjacent(s)) >= 0) return (Command)c;
    return COMMAND_ATTACK_UP;
}

Command strat_rusher(const char *path)
{
    Snapshot s;
    if (!load(path, &s)) return COMMAND_UP;
    return rusher_logic(&s);
}

/* 열쇠-출구 경로 위의 1칸 통로(좌우 또는 상하로만 열린 칸) 찾기.
   choke[y][x]=1 로 표시하고 개수를 반환한다. */
static int find_chokes(const Snapshot *s, int choke[MAP_MAX][MAP_MAX])
{
    int dk[MAP_MAX][MAP_MAX], de[MAP_MAX][MAP_MAX], x, y, n = 0, L;
    bfs(s, s->key_start.x, s->key_start.y, 1, dk);
    bfs(s, s->exit.x, s->exit.y, 1, de);
    L = dk[s->exit.y][s->exit.x];
    for (y = 0; y < s->size; ++y) for (x = 0; x < s->size; ++x) {
        int ns, ew;
        choke[y][x] = 0;
        if (s->terrain[y][x] != ' ' || dk[y][x] + de[y][x] > L + 6) continue;
        if (x == s->key_start.x && y == s->key_start.y) continue;
        ns = walk(s, x, y - 1, 1) + walk(s, x, y + 1, 1);
        ew = walk(s, x - 1, y, 1) + walk(s, x + 1, y, 1);
        if ((ns == 2 && ew == 0) || (ns == 0 && ew == 2)) { choke[y][x] = 1; ++n; }
    }
    return n;
}

/* 부상자 사냥 팀: HP가 가장 낮은 살아 있는 적(HP 1 우선, 같으면 가까운 쪽)을 찾아가 공격한다.
   제자리에서 기다리며 공격하지 않는다. 부상자가 없으면 돌진형으로 열쇠를 노린다. */
Command strat_wounded(const char *path)
{
    Snapshot s;
    int dme[MAP_MAX][MAP_MAX], dist[MAP_MAX][MAP_MAX];
    int i, best = -1, bhp = MAX_HP + 1, bd = FAR, c, d;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self) return rusher_logic(&s);
    if ((c = try_pick(&s)) >= 0) return (Command)c;
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c; /* 옆에 있으면 HP 낮은 쪽부터 친다 */
    bfs(&s, me.x, me.y, 0, dme);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position p = s.players[i].pos;
        int hp = s.players[i].hp;
        if (i == s.self || hp <= 0) continue;
        if (hp < bhp || (hp == bhp && dme[p.y][p.x] < bd)) { bhp = hp; bd = dme[p.y][p.x]; best = i; }
    }
    if (best < 0 || bhp >= MAX_HP) return rusher_logic(&s); /* 다친 적이 없다 */
    bfs(&s, s.players[best].pos.x, s.players[best].pos.y, 0, dist);
    d = step(&s, dist);
    return d >= 0 ? (Command)d : COMMAND_ATTACK_UP;
}

/* 대기자 사냥 팀: 통로에 서서 기다리는(움직이지 않는) 적을 찾아가 공격한다. */
Command strat_gatehunter(const char *path)
{
    Snapshot s;
    int choke[MAP_MAX][MAP_MAX], dme[MAP_MAX][MAP_MAX], dist[MAP_MAX][MAP_MAX];
    int i, best = -1, bd = FAR, c, d;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self) return rusher_logic(&s);
    if ((c = try_pick(&s)) >= 0) return (Command)c;
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    find_chokes(&s, choke);
    bfs(&s, me.x, me.y, 0, dme);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position p = s.players[i].pos;
        if (i == s.self || s.players[i].hp <= 0 || !choke[p.y][p.x]) continue;
        if (dme[p.y][p.x] < bd) { bd = dme[p.y][p.x]; best = i; }
    }
    if (best < 0) return rusher_logic(&s);
    bfs(&s, s.players[best].pos.x, s.players[best].pos.y, 0, dist);
    d = step(&s, dist);
    return d >= 0 ? (Command)d : COMMAND_ATTACK_UP;
}

/* 열쇠 소유자 사냥 팀: 열쇠는 노리지 않고 들고 있는 사람만 쫓는다. 없으면 돌진형. */
Command strat_hunter(const char *path)
{
    Snapshot s;
    int dist[MAP_MAX][MAP_MAX], c, d;
    if (!load(path, &s)) return COMMAND_UP;
    if (s.key_owner < 0 || s.key_owner == s.self) return rusher_logic(&s);
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    bfs(&s, s.players[s.key_owner].pos.x, s.players[s.key_owner].pos.y, 0, dist);
    d = step(&s, dist);
    return d >= 0 ? (Command)d : COMMAND_ATTACK_UP;
}

/* ===== 특정 전략(P0)을 겨냥한 카운터 전략 + 다양한 팀원 유형 (시뮬레이션 전용) ===== */

/* 칸 (x,y) 주변 8칸에 있는 살아 있는 적 수 (HP1 적은 공격 의지가 크다고 보고 같이 센다). */
static int danger_at(const Snapshot *s, int x, int y)
{
    int i, n = 0;
    for (i = 0; i < PLAYER_COUNT; ++i) {
        int ax, ay;
        if (i == s->self || s->players[i].hp <= 0) continue;
        ax = s->players[i].pos.x - x; ay = s->players[i].pos.y - y;
        if (ax >= -1 && ax <= 1 && ay >= -1 && ay <= 1) ++n;
    }
    return n;
}

/* 상태만으로 정한 결정적 난수 (static 상태 없이 실제 게임처럼 매 턴 독립) */
static unsigned hash_state(const Snapshot *s)
{
    unsigned h = (unsigned)s->turn * 2654435761u ^ (unsigned)s->self * 40503u;
    int i;
    for (i = 0; i < PLAYER_COUNT; ++i) h = h * 31u + (unsigned)(s->players[i].pos.x * 37 + s->players[i].pos.y);
    return h ^ (h >> 15);
}

/* safe: 팀원 유형 "조심스러운 주자". 목표(열쇠/출구)로 가되 적이 붙은 칸을 피하고, 옆에 오면 때린다. */
Command strat_safe(const char *path)
{
    Snapshot s;
    int dist[MAP_MAX][MAP_MAX], c, d, best = -1, bs;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner != s.self && (c = try_pick(&s)) >= 0) return (Command)c;
    if (s.key_owner == s.self) bfs(&s, s.exit.x, s.exit.y, 1, dist);
    else if (s.key_owner == -1) bfs(&s, s.key.x, s.key.y, 0, dist);
    else bfs(&s, s.players[s.key_owner].pos.x, s.players[s.key_owner].pos.y, 0, dist);
    bs = dist[me.y][me.x] * 10 + danger_at(&s, me.x, me.y) * (s.players[s.self].hp == 1 ? 60 : 25) + 8;
    for (d = 0; d < 4; ++d) {
        int nx = me.x + DX[d], ny = me.y + DY[d], sc;
        if (!walk(&s, nx, ny, s.key_owner == s.self) || occupant(&s, nx, ny) >= 0) continue;
        sc = dist[ny][nx] * 10 + danger_at(&s, nx, ny) * (s.players[s.self].hp == 1 ? 60 : 25);
        if (sc < bs) { bs = sc; best = d; }
    }
    if (best >= 0) return (Command)best;
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    return COMMAND_ATTACK_UP;
}

/* vulture: 열쇠에서 2칸 떨어져 기다리다가 누가 집으면 따라가 공격한다 (줍는 순간 노출되는 상대를 노린다). */
Command strat_vulture(const char *path)
{
    Snapshot s;
    int dist[MAP_MAX][MAP_MAX], c, d, best = -1, bd;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self) return rusher_logic(&s);
    if (s.key_owner >= 0) return strat_hunter(path);
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    bfs(&s, s.key.x, s.key.y, 0, dist);
    bd = dist[me.y][me.x];
    if (bd > 2) { d = step(&s, dist); return d >= 0 ? (Command)d : COMMAND_ATTACK_UP; }
    if (bd < 2) { /* 너무 가깝다: 더 먼 칸으로 */
        for (d = 0; d < 4; ++d) {
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (walk(&s, nx, ny, 0) && occupant(&s, nx, ny) < 0 && dist[ny][nx] > bd && dist[ny][nx] <= 2) best = d;
        }
        if (best >= 0) return (Command)best;
    }
    /* 열쇠 쪽 3칸을 계속 때리며 대기 */
    {
        int dd = -1;
        for (d = 0; d < 4; ++d) {
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (inb(&s, nx, ny) && dist[ny][nx] < bd) dd = d;
        }
        return COMMAND_ATTACK_UP + (dd >= 0 ? dd : 0);
    }
}

/* exitguard: 남이 열쇠를 들면 출구 바로 옆 칸을 차지하고 버틴다 (그 칸이 막히면 출구에 못 들어간다). */
Command strat_exitguard(const char *path)
{
    Snapshot s;
    int dist[MAP_MAX][MAP_MAX], dk[MAP_MAX][MAP_MAX], c, d, g = -1, bd = FAR;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self) return rusher_logic(&s);
    if (s.key_owner == -1) return rusher_logic(&s);       /* 열쇠가 바닥에 있으면 평범하게 줍는다 */
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    /* 출구 4방향 이웃 중 가장 가까운 칸 */
    bfs(&s, me.x, me.y, 0, dist);
    for (d = 0; d < 4; ++d) {
        int nx = s.exit.x + DX[d], ny = s.exit.y + DY[d];
        if (walk(&s, nx, ny, 0) && dist[ny][nx] < bd) { bd = dist[ny][nx]; g = d; }
    }
    if (g >= 0 && bd > 0) {
        int gx = s.exit.x + DX[g], gy = s.exit.y + DY[g];
        bfs(&s, gx, gy, 0, dist);
        d = step(&s, dist);
        if (d >= 0) return (Command)d;
    }
    /* 자리에 도착: 열쇠 소유자가 오는 방향(열쇠 시작점 쪽)을 때리며 버틴다 */
    bfs(&s, s.key_start.x, s.key_start.y, 0, dk);
    {
        int dd = 0, bb = FAR;
        for (d = 0; d < 4; ++d) {
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (inb(&s, nx, ny) && dk[ny][nx] < bb) { bb = dk[ny][nx]; dd = d; }
        }
        return COMMAND_ATTACK_UP + dd;
    }
}

/* camper: 처음부터 출구 바로 옆 칸을 차지하고 버틴다(교실에서 본 "캠핑하는 학생 AI"). 옆에 오는 적은 때리고, 자리에서는 열쇠가 올 방향을 때린다.
   열쇠를 들면 출구로 들어가고, 아무도 열쇠를 못 가져가는 영원한 교착을 막으려고 턴 140부터는 일반 돌진형으로 바뀐다. */
Command strat_camper(const char *path)
{
    Snapshot s;
    int dist[MAP_MAX][MAP_MAX], dk[MAP_MAX][MAP_MAX], c, d, g = -1, bd = FAR;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self || s.turn >= 140) return rusher_logic(&s);
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    if ((c = try_pick(&s)) >= 0) return (Command)c;
    bfs(&s, me.x, me.y, 0, dist);
    for (d = 0; d < 4; ++d) {
        int nx = s.exit.x + DX[d], ny = s.exit.y + DY[d];
        if (walk(&s, nx, ny, 0) && dist[ny][nx] < bd) { bd = dist[ny][nx]; g = d; }
    }
    if (g >= 0 && bd > 0) {
        int gx = s.exit.x + DX[g], gy = s.exit.y + DY[g];
        bfs(&s, gx, gy, 0, dist);
        d = step(&s, dist);
        if (d >= 0) return (Command)d;
    }
    bfs(&s, s.key.x, s.key.y, 0, dk);
    {
        int dd = 0, bb = FAR;
        for (d = 0; d < 4; ++d) {
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (inb(&s, nx, ny) && dk[ny][nx] < bb) { bb = dk[ny][nx]; dd = d; }
        }
        return COMMAND_ATTACK_UP + dd;
    }
}

/* brawler: 열쇠에 관심 없이 가장 가까운 적에게 붙어서 계속 때린다. 열쇠가 바로 옆이면 줍는다. */
Command strat_brawler(const char *path)
{
    Snapshot s;
    int dme[MAP_MAX][MAP_MAX], dist[MAP_MAX][MAP_MAX], i, best = -1, bd = FAR, c, d;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    if (s.key_owner == s.self) return rusher_logic(&s);
    if ((c = try_pick(&s)) >= 0) return (Command)c;
    if ((c = attack_adjacent(&s)) >= 0) return (Command)c;
    bfs(&s, me.x, me.y, 0, dme);
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position p = s.players[i].pos;
        if (i == s.self || s.players[i].hp <= 0) continue;
        if (dme[p.y][p.x] < bd) { bd = dme[p.y][p.x]; best = i; }
    }
    if (best < 0) return rusher_logic(&s);
    bfs(&s, s.players[best].pos.x, s.players[best].pos.y, 0, dist);
    d = step(&s, dist);
    return d >= 0 ? (Command)d : COMMAND_ATTACK_UP;
}

/* chaos: 아무 갈 수 있는 방향이나 무작위로. 코어 규칙 충돌을 많이 만들어 버그 탐지용으로도 쓴다. */
Command strat_chaos(const char *path)
{
    Snapshot s;
    unsigned h;
    int tries, d;
    Position me;
    if (!load(path, &s)) return COMMAND_UP;
    me = s.players[s.self].pos;
    h = hash_state(&s);
    if (try_pick(&s) >= 0 && h % 3) return (Command)try_pick(&s);
    for (tries = 0; tries < 8; ++tries, h = h * 1103515245u + 12345u) {
        d = (int)((h >> 8) % 4);
        if (walk(&s, me.x + DX[d], me.y + DY[d], s.key_owner == s.self)) return (h >> 4) % 4 ? (Command)d : (Command)(COMMAND_ATTACK_UP + d);
    }
    return COMMAND_ATTACK_UP;
}
