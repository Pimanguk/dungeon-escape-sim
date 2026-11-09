# Dungeon Escape 기말 시뮬레이터 (팀 공유용)

교수님 코어(`FinalExamGameCore`, MIT)를 그대로 링크해서 전략을 수천 판 돌려 보는 도구예요.

- `FinalExamGameCore/` : 교수님 코어 + **`player0.c`는 자리표시자**(예제 AI 복사본). 자기 팀 전략을 여기에 넣어요.
- `StrategySim/` : 시뮬레이터. `전략추가방법.md`부터 읽으세요.
  - `custom/team1~5.c` : 팀원 전략을 붙여넣는 칸 (`playerN_choose_command` -> `strat_teamN`으로 이름만 바꿈)
  - `opponents.c` : 연습 상대 전략 (rusher, wounded, gatehunter, hunter, safe, vulture, exitguard, brawler, chaos)
  - `bugprobe.c` : 코어 버그 탐침. 결과는 `코어_버그검토.md`

## 시작
```
cd StrategySim
.\build.bat                    # Visual Studio 필요 (vcvars64.bat 경로는 build.bat 안)
.\sim.exe --games 300 --seed 7 --scenario counter
.\sim.exe --games 500 --seed 7 --teams ours,team1,team2,team3,team4,team5   # 6팀 직접 지정
```
`ours`는 `FinalExamGameCore\player0.c`예요. 같은 `--seed`로 돌리면 같은 판이라 전략을 고친 효과를 비교할 수 있어요.
