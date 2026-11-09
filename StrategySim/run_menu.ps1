# 시뮬레이션 메뉴: 시나리오 한 판을 턴별 리플레이 파일로 저장해 메모장으로 열거나, 전체 기록표를 만든다.
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!(Test-Path .\sim.exe)) { cmd /c .\build.bat | Out-Null }
New-Item -ItemType Directory -Force replays | Out-Null

$scen = @(
    @('meta(user)', '부상자 사냥 3 + 대기자 사냥 + 돌진 (예상 메타)'),
    @('all-wounded', '부상자 사냥 5'),
    @('wound+hunt', '부상자 사냥 2 + 대기자 사냥 2 + 소유자 추격'),
    @('rushers',    '돌진 3 + 소유자 추격 2'),
    @('samples',    '교수님 예제 AI 5명'),
    @('mixed',      '부상자 사냥 + 예제 AI + 대기자 사냥 + 소유자 추격 + 돌진'),
    @('mirror',     '우리 복제본 2명 + 부상자 사냥 2 + 돌진'),
    @('counter',      '우리를 노리는 카운터 5명 (열쇠 근처 매복 + 출구 막기 + 난투 + 소유자 추격 + 돌진)'),
    @('counter-gate', '출구 막기 2 + 열쇠 매복 2 + 난투 (우리 길을 막는 메타)'),
    @('safe-mix',     '조심스러운 주자 2 + 부상자 사냥 + 돌진 + 예제 AI'),
    @('chaos',        '무작위 5명 (규칙 충돌 많이 생김)'),
    @('teams',      '팀원 전략 5개 (custom 폴더 team1~5.c)'),
    @('my-meta',    '내가 sim.c에 적어 둔 메타 (team1,team2 + 부상자 사냥 + 대기자 사냥 + 돌진)')
)

while ($true) {
    Write-Host ''
    Write-Host '===== Dungeon Escape 시뮬레이션 (우리 = P0, 교수님 맵 고정, 스폰 자리 고정, 플레이어 배정만 랜덤, H0 없음) ====='
    for ($i = 0; $i -lt $scen.Count; $i++) { Write-Host ("  {0}. 한 판 보기 - {1}" -f ($i + 1), $scen[$i][1]) }
    Write-Host '  a. 전체 기록표 만들기 (시나리오마다 300판, results.md에 추가)'
    Write-Host '  b. player0.c / custom 폴더 / sim.c 고친 뒤 다시 빌드'
    Write-Host '  c. 6팀 직접 지정해서 돌리기 (첫 번째 팀이 기준, 예: ours,safe,vulture,exitguard,brawler,team1)'
    Write-Host '  d. 코어 버그 탐침 (probe.exe: 규칙 경계 상황 + 무작위 판 + 맵/상태파일 변조)'
    Write-Host '  0. 종료'
    $c = Read-Host '번호 또는 글자'
    if ($c -eq '0') { break }
    if ($c -eq 'b') { cmd /c .\build.bat; continue }
    if ($c -eq 'c') {
        Write-Host '쓸 수 있는 이름: ours sample rusher wounded gatehunter hunter safe vulture exitguard brawler chaos team1~team5'
        $t = Read-Host '팀 6개를 쉼표로 (첫 번째가 승률 기준 팀)'
        $g = Read-Host '몇 판? (엔터=500)'
        if ($g -notmatch '^\d+$') { $g = 500 }
        cmd /c ".\sim.exe --games $g --seed 7 --teams $t"
        continue
    }
    if ($c -eq 'd') { if (!(Test-Path .\probe.exe)) { cmd /c .\build_probe.bat | Out-Null }; cmd /c ".\probe.exe --games 2000"; continue }
    if ($c -eq 'a') {
        $label = Read-Host '이번 기록 이름 (예: v2 경비 비켜주기)'
        cmd /c ".\sim.exe --games 300 --seed 7 --label ""$label - 교수님 맵"" --report results.md"
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
        & .\sim.exe --games 1 --seed $seed --scenario $name --watch $ms
        Write-Host ("seed {0} (이 번호로 같은 판을 다시 볼 수 있음)" -f $seed)
        Read-Host '엔터를 누르면 메뉴로 돌아갑니다' | Out-Null
        continue
    }
    $safe = $name -replace '[()+]', '_'
    $file = "replays\{0}_{1}_seed{2}.txt" -f $safe, (Get-Date -Format 'MMdd_HHmmss'), $seed
    # cmd 리다이렉트로 UTF-8 바이트를 그대로 저장 (같은 seed로 다시 돌리면 같은 판이 나온다)
    cmd /c ".\sim.exe --games 1 --seed $seed --scenario ""$name"" --show > ""$file"""
    $result = Select-String -Path $file -Pattern '== result' -Encoding UTF8 | Select-Object -Last 1
    Write-Host ("저장: {0}   {1}" -f $file, $result.Line)
    Start-Process notepad $file
}
