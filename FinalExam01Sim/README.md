# FinalExam01 전략 시뮬레이터

교수님 `FinalExam01` 코어(`../FinalExam01/game.c`, `snapshot.c`)를 **수정 없이 그대로 링크**해서 규칙이 실제 게임과 같습니다.
화면 출력·500ms 대기가 없어서 수십~수백 판을 빠르게 돌릴 수 있습니다.

- 우리 팀 **P4** = `../FinalExam01/player4.c` 의 `player4_choose_command` 이고 항상 4번 자리(맵의 `P4` 시작점)에 섭니다.
- 나머지 자리(0,1,2,3,5)에는 상대 5명이 판마다 무작위로 섞여 섭니다. (`--fixseat` 이면 고정)
- `../FinalExam01/player4.c` 를 고치고 다시 빌드하면 바로 승률이 바뀝니다. **제출은 이 파일 하나**입니다.

## 빌드

| 환경 | 명령 |
|---|---|
| 리눅스 / 맥 / WSL | `./build.sh` |
| 리눅스 + 이전 조 코드까지 상대로 사용 | `OLD_DIR=/경로/DigitalContentsProgramming2/FinalExam00 ./build.sh` |
| 윈도우(VS x64 Native Tools 프롬프트) | `build.bat` (이전 조 코드는 지원 안 함, 직접 실행해 보지 못함) |

`OLD_DIR` 은 교수님 저장소의 `FinalExam00` 폴더입니다. **읽기만** 하고 `build/` 안에 이름을 `old0~old5` 로 바꾼 복사본을 만들어 컴파일하므로 원본은 바뀌지 않습니다.

## 실행

```
./build/sim --games 100 --seed 1                      # 상대 풀에서 무작위 5명 (기본)
./build/sim --games 100 --seed 1 --opp old0,old1,old2,old3,old5    # 이전 조 5팀과 대결 (OLD_DIR 빌드 필요)
./build/sim --games 50 --opp rusher,camper,hunter,safe,exitguard
./build/sim --ours old4 --games 100 --seed 1          # 우리 자리에 다른 전략을 세워 비교 (예: 기존 P4 코드)
./build/sim --games 1 --show                           # 한 판을 턴별로 출력
./build/sim --list                                     # 쓸 수 있는 전략 이름
```

같은 `--seed` 로 돌리면 같은 판이라 코드 변경 전후를 공정하게 비교할 수 있습니다.

## 결과 읽는 법

`1위` = 승률, `3위 이내`, `평균 순위`, 순위 분포를 냅니다.
`미확정` 은 1200턴 안에 끝나지 않은 판입니다. (실제 게임에는 이 한도가 없고, 일부가 탈출한 뒤 남은 인원이 서로 눈치만 보는 교착입니다.)

## 주의

- 리눅스/맥/WSL 은 매 호출을 별도 프로세스(fork)로 실행해 실제 게임처럼 전역/static 상태가 이어지지 않습니다. 윈도우는 같은 프로세스에서 부르므로 static 으로 이전 턴을 기억하는 코드는 결과가 달라집니다.
- 상대 풀(`opponents.c`)은 기존 `StrategySim/opponents.c` 의 일반 전략입니다. 실제 대회 상대와는 다르므로 승률은 참고용입니다.
- 시뮬레이터는 실제 1.5초 제한을 지키지 않습니다(리눅스는 5초에 강제 종료). 느린 코드는 실제 PC에서 시간 초과가 날 수 있습니다.
