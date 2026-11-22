#include "player_api.h"

/* 화면에 표시되는 이름 */
char player4_name[PLAYER_NAME_BYTES] = "P4";

/* ===== P4 v4 : 명령 16개를 위험/이득 점수로 평가하는 AI =====
   핵심
   1) 열쇠 경쟁은 "내 도착 턴 vs 상대 도착 턴(부활 대기 포함)"으로 정한다.
   2) 남이 열쇠를 들면: 소유자의 출구 최단 경로를 계산해 내가 먼저 도착할 수 있는 칸을 고른다.
      - 그 칸에 서서 막기(차단): 우회 거리가 늘어나는 길목일수록 가치가 높다. 움직이지 않는 사람 칸으로
        이동하면 이동이 취소되고 이동한 쪽만 피해 1이라는 규칙을 이용한다.
      - 또는 경로 칸 옆에 서서 그 칸을 부채꼴로 덮는다(공격은 이동 후 위치를 친다).
   3) 내가 열쇠를 들면: 막은 적을 벽으로 보고 우회로를 찾고, 공격하지 않는다.
      HP 2로 출구 바로 앞이면 위험을 무시하고 들어간다(그 턴 피해는 최대 1).
   4) 위협은 현실적으로: 적 공격은 8칸 중 3칸만 맞히므로 확률로 계산하고, 같은 칸으로 들어가는 충돌도 확률로 계산한다.
   매 턴 별도 프로세스이므로 이전 턴 상태는 저장하지 않는다. */

#ifndef P4_FN
#define P4_FN player4_choose_command
#endif
#ifndef P4_GO_BIAS
#define P4_GO_BIAS 1       /* 열쇠 경쟁에서 이 값만큼 불리해도 간다 */
#endif
#ifndef P4_IDLE_DIV
#define P4_IDLE_DIV 3      /* 열쇠가 이 턴 방치될 때마다 허용 폭 +1 */
#endif
#ifndef P4_PATIENCE
#define P4_PATIENCE 40     /* 이 턴마다 허용 폭 +1 */
#endif
#ifndef P4_STAGE_R
#define P4_STAGE_R 2       /* 열쇠 대기 거리 */
#endif
#ifndef P4_ATK_P
#define P4_ATK_P 50        /* 적이 사정권의 나를 공격할 확률(%) */
#endif
#ifndef P4_W
#define P4_W 3             /* 거리 1칸당 점수 */
#endif
#ifndef P4_HIT_COST2
#define P4_HIT_COST2 14    /* HP 2일 때 피격 비용 */
#endif
#ifndef P4_HIT_COST1
#define P4_HIT_COST1 60    /* HP 1일 때 피격 비용 */
#endif
#ifndef P4_ATK_VAL
#define P4_ATK_VAL 14      /* 공격 성공 기본 가치 */
#endif
#ifndef P4_CARRIER_MUL
#define P4_CARRIER_MUL 3.5 /* 열쇠 소유자 공격 가치 배율 */
#endif
#ifndef P4_BUMP_VAL
#define P4_BUMP_VAL 45     /* 소유자가 내 칸으로 들어오다 취소될 때의 가치 */
#endif
#ifndef P4_BLOCK_W
#define P4_BLOCK_W 6       /* 우회 거리 1칸당 차단 가치 */
#endif
#ifndef P4_AMBUSH_V
#define P4_AMBUSH_V 24     /* 경로 칸 옆 매복 기본 가치 */
#endif
#ifndef P4_CHASE_TURN
#define P4_CHASE_TURN 150  /* 이 턴 이후 소유자가 안 나가면 직접 쫓는다 */
#endif

#define P4_INF 9999
#define P4_N MAP_MAX

static const int p4dx[4] = {0, 0, -1, 1};
static const int p4dy[4] = {-1, 1, 0, 0};

static int p4_in(const Snapshot *s, int x, int y)
{
    return x >= 0 && y >= 0 && x < s->size && y < s->size;
}

static int p4_can(const Snapshot *s, int x, int y, int allow_exit)
{
    if (!p4_in(s, x, y)) return 0;
    if (s->terrain[y][x] == '#') return 0;
    if (s->terrain[y][x] == 'E' && !allow_exit) return 0;
    return 1;
}

/* BFS. blk 가 NULL 이 아니면 blk 가 1 인 칸은 벽 취급, (bx,by) 도 벽 취급(음수면 없음) */
static void p4_bfs(const Snapshot *s, Position src, int allow_exit, unsigned char blk[P4_N][P4_N],
                   int bx, int by, int dist[P4_N][P4_N])
{
    Position q[P4_N * P4_N];
    int head = 0, tail = 0, x, y, d;
    for (y = 0; y < P4_N; ++y)
        for (x = 0; x < P4_N; ++x) dist[y][x] = P4_INF;
    if (!p4_in(s, src.x, src.y)) return;
    dist[src.y][src.x] = 0;
    q[tail++] = src;
    while (head < tail) {
        Position c = q[head++];
        for (d = 0; d < 4; ++d) {
            int nx = c.x + p4dx[d], ny = c.y + p4dy[d];
            if (!p4_can(s, nx, ny, allow_exit) || dist[ny][nx] != P4_INF) continue;
            if (nx == bx && ny == by) continue;
            if (blk && blk[ny][nx]) continue;
            dist[ny][nx] = dist[c.y][c.x] + 1;
            q[tail].x = nx; q[tail].y = ny; ++tail;
        }
    }
}

static int p4_cheb(int ax, int ay, int bx, int by)
{
    int dx = abs(ax - bx), dy = abs(ay - by);
    return dx > dy ? dx : dy;
}

/* 공격 방향 d 의 부채꼴(정면+양쪽 대각선)에 (px,py)가 들어가는가. 기준점 (ox,oy) */
static int p4_in_fan(int ox, int oy, int d, int px, int py)
{
    int ax = px - ox, ay = py - oy;
    if (d == 0) return ay == -1 && abs(ax) <= 1;
    if (d == 1) return ay == 1 && abs(ax) <= 1;
    if (d == 2) return ax == -1 && abs(ay) <= 1;
    return ax == 1 && abs(ay) <= 1;
}

Command P4_FN(const char *state_path)
{
    static const int dummy = 0; /* (static 상태는 쓰지 않는다. 읽기 전용 상수) */
    Snapshot s;
    FILE *fp;
    int dK[P4_N][P4_N], dE[P4_N][P4_N], dEb[P4_N][P4_N], dMe[P4_N][P4_N], dT[P4_N][P4_N], dC[P4_N][P4_N];
    int dB[P4_N][P4_N];
    unsigned char occ[P4_N][P4_N];
    double pd[P4_N][P4_N];
    double hit[P4_N][P4_N];
    double pe[PLAYER_COUNT][P4_N][P4_N];
    int alive[PLAYER_COUNT];
    Position me, carrierPos = {0, 0};
    int i, x, y, d, c, carrying, keyFloor, carrier, myhp, nalive = 0, finished = 0;
    int mode; /* 0 key go, 1 ambush near key, 2 intercept carrier, 3 carry */
    double hitCost, bestScore = -1e18;
    Command best = COMMAND_ATTACK_UP;
    int lvl[P4_N * 2];
    (void)dummy;

    fp = fopen(state_path, "r");
    if (!fp) return COMMAND_ATTACK_UP;
    i = snapshot_read(fp, &s);
    fclose(fp);
    if (!i) return COMMAND_ATTACK_UP;
    if (s.size < 3 || s.size > MAP_MAX || s.self < 0 || s.self >= PLAYER_COUNT) return COMMAND_ATTACK_UP;

    me = s.players[s.self].pos;
    myhp = s.players[s.self].hp;
    carrier = s.key_owner;
    carrying = carrier == s.self;
    keyFloor = carrier < 0;
    hitCost = myhp >= 2 ? P4_HIT_COST2 : P4_HIT_COST1;
    if (carrying) hitCost *= 1.5;

    memset(occ, 0, sizeof(occ));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (s.players[i].rank > 0) ++finished;
        alive[i] = (i != s.self && s.players[i].hp > 0 && s.players[i].rank == 0);
        if (alive[i]) {
            ++nalive;
            occ[s.players[i].pos.y][s.players[i].pos.x] = 1;
        }
    }
    if (carrier >= 0) carrierPos = s.players[carrier].pos;

    p4_bfs(&s, s.key, 0, NULL, -1, -1, dK);
    p4_bfs(&s, s.exit, 1, NULL, -1, -1, dE);
    p4_bfs(&s, s.exit, 1, occ, -1, -1, dEb);   /* 적을 벽으로 본 출구 거리 */
    p4_bfs(&s, me, carrying, NULL, -1, -1, dMe);
    if (carrier >= 0 && carrier != s.self) p4_bfs(&s, carrierPos, 1, NULL, -1, -1, dC);
    else p4_bfs(&s, me, 0, NULL, -1, -1, dC);

    /* ---------- 적 행동 예측 ---------- */
    memset(pd, 0, sizeof(pd));
    memset(hit, 0, sizeof(hit));
    memset(pe, 0, sizeof(pe));
    for (i = 0; i < PLAYER_COUNT; ++i) {
        Position e;
        int k, bestd = P4_INF, nbest = 0, nvalid = 0, here;
        int valid[4], dist[4], ecarry;
        double w[5];
        if (!alive[i]) continue;
        e = s.players[i].pos;
        ecarry = (carrier == i);
        for (d = 0; d < 4; ++d) {
            int nx = e.x + p4dx[d], ny = e.y + p4dy[d];
            valid[d] = p4_can(&s, nx, ny, ecarry);
            dist[d] = P4_INF;
            if (!valid[d]) continue;
            ++nvalid;
            if (ecarry) dist[d] = dE[ny][nx];
            else if (keyFloor) dist[d] = dK[ny][nx];
            else dist[d] = dC[ny][nx] <= 3 ? dC[ny][nx] : dE[ny][nx];
            if (dist[d] < bestd) bestd = dist[d];
        }
        for (d = 0; d < 4; ++d) if (valid[d] && dist[d] == bestd) ++nbest;
        if (ecarry) here = dE[e.y][e.x];
        else if (keyFloor) here = dK[e.y][e.x];
        else here = dC[e.y][e.x] <= 3 ? dC[e.y][e.x] : dE[e.y][e.x];
        for (k = 0; k < 5; ++k) w[k] = 0.0;
        if (nvalid == 0) w[4] = 1.0;
        else if (ecarry) {                       /* 소유자는 출구로 간다 */
            w[4] = 0.05;
            for (d = 0; d < 4; ++d) {
                if (!valid[d]) continue;
                if (dist[d] == bestd && bestd < here) w[d] = 0.85 / (nbest ? nbest : 1);
                else w[d] = 0.10 / nvalid;
            }
        } else if (keyFloor && here == 1) {      /* 열쇠 줍는 자리 */
            w[4] = 0.85;
            for (d = 0; d < 4; ++d) if (valid[d]) w[d] = 0.15 / nvalid;
        } else {
            w[4] = 0.28;
            for (d = 0; d < 4; ++d) {
                if (!valid[d]) continue;
                if (dist[d] == bestd && bestd < here) w[d] = 0.57 / (nbest ? nbest : 1);
                else w[d] = 0.15 / nvalid;
            }
        }
        for (k = 0; k < 5; ++k) {
            int nx = e.x, ny = e.y;
            if (w[k] <= 0.0) continue;
            if (k < 4) { nx += p4dx[k]; ny += p4dy[k]; }
            pd[ny][nx] += w[k];
            pe[i][ny][nx] += w[k];
        }
        if (!ecarry) {   /* 열쇠 소유자는 공격할 수 없다. 나머지는 8칸 중 일부를 확률로 친다 */
            double pa = P4_ATK_P / 100.0;
            for (y = e.y - 1; y <= e.y + 1; ++y)
                for (x = e.x - 1; x <= e.x + 1; ++x) {
                    if (!p4_in(&s, x, y) || (x == e.x && y == e.y) || s.terrain[y][x] == '#') continue;
                    hit[y][x] = 1.0 - (1.0 - hit[y][x]) * (1.0 - pa * 0.6);
                }
        }
    }

    /* ---------- 모드와 목표 ---------- */
    if (carrying) {
        mode = 3;
        memcpy(dT, dEb[me.y][me.x] < P4_INF ? dEb : dE, sizeof(dT));
    } else if (keyFloor) {
        int myEta = dK[me.y][me.x] < 1 ? 2 : dK[me.y][me.x], minEta = P4_INF, bonus, idle;
        for (i = 0; i < PLAYER_COUNT; ++i) {
            int eta;
            if (i == s.self || s.players[i].rank != 0) continue;
            if (s.players[i].hp > 0) {
                eta = dK[s.players[i].pos.y][s.players[i].pos.x];
                if (eta < 1) eta = 2;
            } else {
                int rest = (int)(s.players[i].rest > 50 ? 50 : s.players[i].rest);
                eta = rest + 1 + dK[s.players[i].spawn.y][s.players[i].spawn.x];
            }
            if (eta < minEta) minEta = eta;
        }
        idle = KEY_RESPAWN_TURNS - s.key_respawn_remaining;
        if (idle < 0) idle = 0;
        bonus = P4_GO_BIAS + idle / P4_IDLE_DIV + s.turn / P4_PATIENCE + finished;
        mode = (minEta - myEta + bonus >= 0 || nalive == 0) ? 0 : 1;
        memcpy(dT, dK, sizeof(dT));
    } else {
        /* 남이 열쇠를 들었다: 소유자의 출구 최단 경로 위에서 먼저 자리 잡기 */
        int dEc = dE[carrierPos.y][carrierPos.x];
        double bestV = -1e18;
        int sx = -1, sy = -1, px, py;
        mode = 2;
        memset(lvl, 0, sizeof(lvl));
        for (py = 0; py < s.size; ++py)
            for (px = 0; px < s.size; ++px)
                if (dC[py][px] < P4_INF && dE[py][px] < P4_INF && dC[py][px] + dE[py][px] == dEc && s.terrain[py][px] != 'E'
                    && !(px == carrierPos.x && py == carrierPos.y) && dC[py][px] < P4_N * 2)
                    lvl[dC[py][px]]++;
        if (s.turn < P4_CHASE_TURN && dEc < P4_INF) {
            for (py = 0; py < s.size; ++py)
                for (px = 0; px < s.size; ++px) {
                    int tc, gain, nb;
                    if (dC[py][px] >= P4_INF || dE[py][px] >= P4_INF || dC[py][px] + dE[py][px] != dEc) continue;
                    if (s.terrain[py][px] == 'E' || (px == carrierPos.x && py == carrierPos.y)) continue;
                    tc = dC[py][px];
                    /* (a) 그 칸 위에 서서 막기: 막았을 때 소유자의 우회 거리 증가 */
                    if (dMe[py][px] <= tc - 1) {
                        p4_bfs(&s, carrierPos, 1, NULL, px, py, dB);
                        gain = dB[s.exit.y][s.exit.x] >= P4_INF ? 10 : dB[s.exit.y][s.exit.x] - dEc;
                        if (gain > 0) {
                            double v = P4_BLOCK_W * (gain > 8 ? 8 : gain) - 1.0 * dMe[py][px] - 0.7 * (tc - 1 - dMe[py][px]);
                            if (v > bestV) { bestV = v; sx = px; sy = py; }
                        }
                    }
                    /* (b) 경로 칸 옆에 서서 그 칸을 부채꼴로 덮기 */
                    for (nb = 0; nb < 8; ++nb) {
                        int ox = px + (nb < 4 ? p4dx[nb] : (nb == 4 || nb == 6 ? -1 : 1));
                        int oy = py + (nb < 4 ? p4dy[nb] : (nb == 4 || nb == 5 ? -1 : 1));
                        double v;
                        if (!p4_can(&s, ox, oy, 0)) continue;
                        if (dMe[oy][ox] > tc - 1) continue;
                        v = P4_AMBUSH_V / (double)(lvl[tc] > 0 ? lvl[tc] : 1) - 1.0 * dMe[oy][ox] - 0.7 * (tc - 1 - dMe[oy][ox]);
                        if (v > bestV) { bestV = v; sx = ox; sy = oy; }
                    }
                }
        }
        if (sx >= 0) {
            Position t; t.x = sx; t.y = sy;
            p4_bfs(&s, t, 0, NULL, -1, -1, dT);
        } else {
            memcpy(dT, dC, sizeof(dT));   /* 선점 불가: 직접 쫓는다 */
        }
    }

    /* ---------- 16개 명령 평가 ---------- */
    for (c = 0; c < COMMAND_COUNT; ++c) {
        int nx = me.x, ny = me.y, valid = 1, moving = 0;
        double score = 0.0, gain = 0.0, risk = 0.0;
        if (c < 4) {
            nx += p4dx[c]; ny += p4dy[c];
            moving = 1;
            valid = p4_can(&s, nx, ny, carrying);
        } else if (c < 8) {
            valid = !carrying;
        } else if (c < 12) {
            int px = me.x + p4dx[c - 8], py = me.y + p4dy[c - 8];
            valid = keyFloor && !carrying && px == s.key.x && py == s.key.y;
        } else {
            valid = 0; /* 열쇠 버리기는 쓰지 않는다 */
        }
        if (!valid) continue;

        {
            int dd = dT[ny][nx];
            if (dd >= P4_INF) dd = 200;
            if (mode == 1) dd = abs(dd - P4_STAGE_R);
            else if (mode == 0) dd = abs(dd - 1);
            score -= P4_W * dd;
        }

        if (mode == 3 && moving && s.terrain[ny][nx] == 'E') {
            score += 100000.0;                  /* 출구 진입: HP 2면 그 턴 피해는 최대 1이라 생존 */
            if (myhp >= 2) risk = 0.0; else risk += hitCost * hit[ny][nx];
        } else {
            risk += hitCost * hit[ny][nx];
            if (moving) risk += hitCost * (pd[ny][nx] > 1.0 ? 1.0 : pd[ny][nx]) * 0.9;
        }

        if (c >= 8 && c < 12) {                 /* 열쇠 줍기 */
            double conf = 0.0;
            for (i = 0; i < PLAYER_COUNT; ++i) {
                if (!alive[i]) continue;
                if (abs(s.players[i].pos.x - s.key.x) + abs(s.players[i].pos.y - s.key.y) == 1)
                    conf = 1.0 - (1.0 - conf) * 0.3;
            }
            gain += 500.0 * (1.0 - conf) - hitCost * conf;
        }

        if (c >= 4 && c < 8) {                  /* 공격 + (제자리이므로) 막기 */
            for (i = 0; i < PLAYER_COUNT; ++i) {
                double pin = 0.0, v;
                if (!alive[i]) continue;
                for (y = me.y - 1; y <= me.y + 1; ++y)
                    for (x = me.x - 1; x <= me.x + 1; ++x)
                        if (p4_in(&s, x, y) && p4_in_fan(me.x, me.y, c - 4, x, y)) pin += pe[i][y][x];
                v = P4_ATK_VAL;
                if (s.players[i].hp == 1) v *= 2.5;
                if (carrier == i) v *= P4_CARRIER_MUL;
                gain += pin * v;
            }
            if (carrier >= 0 && carrier != s.self && alive[carrier])
                gain += pe[carrier][me.y][me.x] * P4_BUMP_VAL;   /* 내 칸으로 들어오려다 취소되는 소유자 */
        }
        score += gain - risk;
        if (score > bestScore) { bestScore = score; best = (Command)c; }
    }
    return best;
}
