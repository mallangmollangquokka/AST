# AST 정적 분석 & 원본 코드 복원

C 컴파일러가 만들어내는 **AST(추상 구문 트리)** 를 직접 다뤄보는 과제 프로젝트입니다.
AST를 JSON으로 직렬화한 `ast.json`을 C로 파싱해, 코드 텍스트가 아닌 **트리 구조만으로** 함수 정보를 추출하고 원본 소스를 복원합니다.

## AST란?

컴파일러는 소스 코드를 글자 나열이 아니라 구조로 이해합니다. `if (x > 0)` 는 "조건 분기 → 비교 연산 → 변수와 상수" 같은 트리로 분해되는데, 이 트리가 AST입니다.
이 트리는 파서(`pycparser`) 메모리 안에만 존재하므로, JSON 텍스트로 저장(직렬화)한 것이 `ast.json`입니다. JSON은 언어 독립적이라 C로 읽어 분석할 수 있습니다.

분석 대상 `ast.json`은 작은 셀프 호스팅 C 컴파일러의 AST입니다. 최상위 `ext` 배열에서:

- `_nodetype == "Decl"` → 헤더 함수 선언 (분석 제외)
- `_nodetype == "FuncDef"` → 실제 함수 정의 (분석 대상, 총 36개)

## 파일 구성

| 파일 | 설명 |
|---|---|
| `json_c.c` | JSON 파서 (파일 읽기 함수 `json_read` 직접 구현) |
| `ast.json` | 분석 대상 AST (멘토 제공) |
| `ast.c` | 함수 정보 추출 |
| `all_recovery.c` | AST → 원본 C 코드 복원 |
| `restored.c` | 복원 결과 (36개 함수) — `all_recovery` 실행 시 **생성되는 출력** |

## 빌드 및 실행

> `ast.c` / `all_recovery.c`는 각각 `json_c.c`를 `#include` 하므로 별도 링크 없이 그대로 컴파일됩니다.

```bash
# 1) 함수 정보 추출
gcc ast.c -o ast && ./ast

# 2) 원본 코드 복원 → restored.c 생성
gcc all_recovery.c -o recovery && ./recovery
```

`make` 를 쓰면 한 번에 빌드할 수 있습니다.

```bash
make        # ast, recovery 둘 다 빌드
make run    # 빌드 후 둘 다 실행 (restored.c 생성)
make clean
```

## `ast.c` — 함수 정보 추출

`FuncDef` 노드를 순회하며 추출합니다.

- **함수 개수** — `FuncDef` 카운트 (36개)
- **리턴 타입** — `TypeDecl` 까지 타고 내려가며 추출, `PtrDecl` 이면 `*` 추가
- **함수 이름** — `decl → name`
- **파라미터** — `args → params` 순회 (타입 + 변수명)
- **if 개수** — `body` 재귀 순회로 `If` 노드 카운트 (중첩 포함)

## `all_recovery.c` — 원본 코드 복원

AST를 거꾸로 읽어 원본 C 코드를 복원하는 작은 디컴파일러입니다. `gen()` 재귀 함수가 노드 종류별로 C 코드를 만들고 자식 노드에 대해 자신을 호출합니다. 36개 함수를 미처리 항목 없이 전부 복원해 `restored.c` 로 저장합니다.

## 알게 된 점 · 한계

- AST는 문법 구조만 담고 괄호·주석·공백은 남지 않는다 → 복원 시 우선순위로 괄호를 재생성해야 함
- 복원본은 함수 몸통만 복원해 전역 변수·함수 원형이 없어 단독 컴파일은 불가
- 깊은 `else if` 들여쓰기가 일부 어긋나지만 동작에는 영향 없음

## 사용 도구

`C` · `json_c.c`(JSON 파서) · Python `pycparser`(AST 생성)
