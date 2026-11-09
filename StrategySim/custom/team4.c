/* 팀원 전략 칸 4번. 기본값은 교수님 예제 AI(player1)를 그대로 부른다.
   (아무것도 붙여넣지 않아도 빌드와 실행이 되게 하려는 임시 내용이다.)
   사용법은 StrategySim\전략추가방법.md 참고: 팀원의 playerN_choose_command 함수를
   아래 strat_team4 으로 이름만 바꿔서 통째로 붙여넣으면 된다. */
#include "../../FinalExamGameCore/player_api.h"

Command player1_choose_command(const char *state_path);

/* 여기를 팀원 전략으로 교체한다 (함수 이름은 strat_team4 그대로 유지) */
Command strat_team4(const char *state_path)
{
    return player1_choose_command(state_path);
}
