#!/bin/sh
# FinalExam01 전략 시뮬레이터 빌드 (리눅스 / 맥 / WSL).  필요: gcc
#   ./build.sh                     -> 일반 상대 풀(rusher, camper ...)만 사용
#   OLD_DIR=/경로/FinalExam00 ./build.sh   -> 교수님 저장소 FinalExam00 의 이전 조 코드(old0~old5)를 상대 풀에 추가
#        (원본은 읽기만 한다. build/ 폴더에 복사본을 만들어 이름만 old0~old5 로 바꿔 컴파일한다.)
set -e
cd "$(dirname "$0")"
CORE=../FinalExam01
mkdir -p build
EXTRA_OBJS=""
DEFS=""
if [ -n "$OLD_DIR" ]; then
  for i in 0 1 2 3 4 5; do
    sed -e '1s/^\xEF\xBB\xBF//' -e "s/player${i}_choose_command/old${i}_choose_command/g" -e "s/player${i}_name/old${i}_name/g" \
        "$OLD_DIR/player$i.c" > build/old$i.c
    gcc -O2 -w -I "$CORE" -c build/old$i.c -o build/old$i.o
    objcopy --keep-global-symbol=old${i}_choose_command build/old$i.o build/old${i}_l.o
    EXTRA_OBJS="$EXTRA_OBJS build/old${i}_l.o"
  done
  DEFS="-DWITH_OLD"
fi
gcc -O2 -w -I "$CORE" $DEFS sim.c opponents.c "$CORE/game.c" "$CORE/snapshot.c" "$CORE/diagnostics.c" "$CORE/player4.c" $EXTRA_OBJS -o build/sim
echo "빌드 완료: FinalExam01Sim/build/sim   (실행 예: ./build/sim --games 50 --seed 1)"
