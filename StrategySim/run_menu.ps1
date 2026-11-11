# 시뮬레이션 메뉴: 시나리오 한 판을 턴별 리플레이 파일로 저장해 메모장으로 열거나, 전체 기록표를 만든다.
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!(Test-Path .\sim.exe) -or (Get-Item .\sim.c).LastWriteTime -gt (Get-Item .\sim.exe).LastWriteTime) { cmd /c .\build.bat | Out-Null }  # sim.c 가 더 새로우면 자동으로 다시 빌드
New-Item -ItemType Directory -Force replays | Out-Null

# 지도 선택: 기본은 수업 시연 1 재현 지도(v1). 열쇠 후보 칸은 지도 옆 <지도파일>.keycells 에서 자동으로 읽는다(없으면 지도에 적힌 열쇠 자리).
$maps = @(
    @('maps/v1_lecture1_topleft.dat',  '수업 시연 1 재현: 시작 6곳=네 모서리+가운데 위아래, 출구 가운데(6,6), 열쇠는 (4,4)(8,4)(4,8)(8,8) 중 무작위'),
    @('maps/v2_lecture2_topright.dat', '수업 시연 2 재현: 가운데 가로 벽, 출구는 아래 방'),
    @('../FinalExamGameCore/dungeonmap.dat', '교수님 기존 맵'),
    @('maps/m1_open4.dat',   '출구 4방향 개방'),
    @('maps/m2_arena.dat',   '열린 지도(벽 없음)'),
    @('maps/m3_flankLR.dat', '출구 좌우가 막힘'),
    @('maps/m4_flankUD.dat', '출구 상하가 막힘')
)
$map = $maps[0][0]

$scen = @(
    @('meta(user)', '부상자 사냥 3 + 대기자 사냥 + 돌진 (예상 메타)'),
    @('all-wounded', '부상자 사냥 5'),
    @('wound+hunt', '부상자 사냥 2 + 대기자 사냥 2 + 소유자 추격'),
    @('rushers',    '돌진 3 + 소유자 추격 2'),
    @('samples',    '교수님 예제 AI 5명'),
    @('mixed',      '부상자 사냥 + 예제 AI + 대기자 사냥 + 소유자 추격 + 돌진'),
    @('mirror',     '내 전략 복제본 2명 + 부상자 사냥 2 + 돌진'),
    @('counter',      'P0을 노리는 카운터 5명 (열쇠 근처 매복 + 출구 막기 + 난투 + 소유자 추격 + 돌진)'),
    @('counter-gate', '출구 막기 2 + 열쇠 매복 2 + 난투 (P0 길을 막는 메타)'),
    @('safe-mix',     '조심스러운 주자 2 + 부상자 사냥 + 돌진 + 예제 AI'),
    @('chaos',        '무작위 5명 (규칙 충돌 많이 생김)'),
    @('teams',      '팀원 전략 5개 (custom 폴더 team1~5.c)'),
    @('my-meta',    '내가 sim.c에 적어 둔 메타 (team1,team2 + 부상자 사냥 + 대기자 사냥 + 돌진)'),
    @('rand-all',   '판마다 상대 5명을 전략 9종에서 무작위로'),
    @('rand-hard',  '판마다 상대 5명을 P0을 노리는 쪽 위주로 무작위'),
    @('rand-soft',  '판마다 상대 5명을 소극적인 팀 위주로 무작위'),
    @('hunters',    '소유자 사냥 3 + 대기자 사냥 2'),
    @('brawlers',   '난투형 5명'),
    @('vultures',   '열쇠 매복 3 + 출구 막기 2'),
    @('key-war',    '열쇠 쟁탈이 치열한 반 (돌진 2 + 난투 + 매복 + 소유자 사냥)'),
    @('one-rusher', '돌진 1 + 부상자 사냥 2 + 조심형 2'),
    @('camp-1',     '출구 옆 캠퍼 1명 + 돌진 + 부상자 사냥 + 대기자 사냥 + 조심형'),
    @('camp-2',     '출구 옆 캠퍼 2명 + 돌진 + 부상자 사냥 + 소유자 사냥'),
    @('camp-3',     '출구 옆 캠퍼 3명 + 돌진 + 부상자 사냥'),
    @('camp-rand',  '판마다 상대 5명을 캠퍼 위주로 무작위')
)

while ($true) {
    Write-Host ''
    Write-Host ("===== Dungeon Escape 시뮬레이션 (내 전략 = P0, 지도: {0}, P0 시작 자리와 상대 배정은 매 판 랜덤, H0 없음) =====" -f $map)
    for ($i = 0; $i -lt $scen.Count; $i++) { Write-Host ("  {0}. 한 판 보기 - {1}" -f ($i + 1), $scen[$i][1]) }
    Write-Host '  a. 전체 기록표 만들기 (시나리오마다 300판, results.md에 추가)'
    Write-Host '  b. player0.c / custom 폴더 / sim.c 고친 뒤 다시 빌드'
    Write-Host '  c. 6팀 직접 지정해서 돌리기 (첫 번째 팀이 기준, 예: ours,safe,vulture,exitguard,brawler,team1)'
    Write-Host '  d. 코어 버그 탐침 (probe.exe: 규칙 경계 상황 + 무작위 판 + 맵/상태파일 변조)'
    Write-Host '  e. 지도 바꾸기 (수업 시연 1·2 재현, 기존 맵, 출구 변형 지도)'
    Write-Host '  f. 시작 자리별 / 열쇠 칸별 승률표 (현재 지도, 시나리오 전체 합산, 150판씩)'
    Write-Host '  0. 종료'
    $c = Read-Host '번호 또는 글자'
    if ($c -eq '0') { break }
    if ($c -eq 'b') { cmd /c .\build.bat; continue }
    if ($c -eq 'e') {
        for ($i = 0; $i -lt $maps.Count; $i++) { Write-Host ("  {0}. {1}" -f ($i + 1), $maps[$i][1]) }
        $m = Read-Host '지도 번호'
        $mi = 0
        if ([int]::TryParse($m, [ref]$mi) -and $mi -ge 1 -and $mi -le $maps.Count) { $map = $maps[$mi - 1][0] } else { Write-Host '잘못된 번호' }
        continue
    }
    if ($c -eq 'f') {
        cmd /c ".\sim.exe --map $map --games 150 --seed 7 --byseat"
        continue
    }
    if ($c -eq 'c') {
        Write-Host '쓸 수 있는 이름: ours sample rusher wounded gatehunter hunter safe vulture exitguard brawler chaos team1~team5'
        $t = Read-Host '팀 6개를 쉼표로 (첫 번째가 승률 기준 팀)'
        $g = Read-Host '몇 판? (엔터=500)'
        if ($g -notmatch '^\d+$') { $g = 500 }
        cmd /c ".\sim.exe --map $map --games $g --seed 7 --teams $t"
        continue
    }
    if ($c -eq 'd') { if (!(Test-Path .\probe.exe)) { cmd /c .\build_probe.bat | Out-Null }; cmd /c ".\probe.exe --games 2000"; continue }
    if ($c -eq 'a') {
        $label = Read-Host '이번 기록 이름 (예: v2 경비 비켜주기)'
        cmd /c ".\sim.exe --map $map --games 300 --seed 7 --label ""$label - $map"" --report results.md"
        Start-Process notepad .\results.md
        continue
    }
    $n = 0
    if (![int]::TryParse($c, [ref]$n) -or $n -lt 1 -or $n -gt $scen.Count) { Write-Host '잘못된 번호'; continue }
    $name = $scen[$n - 1][0]
    $s = Read-Host 'seed 번호 (엔터=랜덤, 같은 번호를 넣으면 같은 판을 다시 봄)'
    $seed = if ($s -match '^\d+$') { [int]$s } else { Get-Random -Minimum 1 -Maximum 1000000 }
    $v = Read-Host '보기 방식: 1=화면에서 턴마다 재생, 2=텍스트 파일로 저장 (엔터=1)'
    if ($v -ne '2') {
        $ms = Read-Host '턴 간격 ms (엔터=2000, 실제 게임과 같음)'
        if ($ms -notmatch '^\d+$') { $ms = 2000 }
        # 교수님 display.c 화면 그대로 렌더링: 공격 효과(노란 배경/**), 피격(빨간 배경), 사망(!!)
        & .\sim.exe --map $map --games 1 --seed $seed --scenario $name --watch $ms
        Write-Host ("seed {0} (이 번호로 같은 판을 다시 볼 수 있음)" -f $seed)
        Read-Host '엔터를 누르면 메뉴로 돌아갑니다' | Out-Null
        continue
    }
    $safe = $name -replace '[()+]', '_'
    $file = "replays\{0}_{1}_seed{2}.txt" -f $safe, (Get-Date -Format 'MMdd_HHmmss'), $seed
    # cmd 리다이렉트로 UTF-8 바이트를 그대로 저장 (같은 seed로 다시 돌리면 같은 판이 나온다)
    cmd /c ".\sim.exe --map $map --games 1 --seed $seed --scenario ""$name"" --show > ""$file"""
    $result = Select-String -Path $file -Pattern '== result' -Encoding UTF8 | Select-Object -Last 1
    Write-Host ("저장: {0}   {1}" -f $file, $result.Line)
    Start-Process notepad $file
}
