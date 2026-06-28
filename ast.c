/*
 * ast.c
 * AST(ast.json) 정적분석 - 기본 과제 1~5
 *
 *   1. 함수 개수 추출
 *   2. 함수들의 리턴타입 추출
 *   3. 함수들의 이름 추출
 *   4. 함수들의 파라미터 타입/변수명 추출
 *   5. 함수들의 if 조건 개수 추출
 *
 * 분석 대상: 최상위 FileAST 의 "ext" 배열에서 _nodetype 이 "FuncDef" 인 노드들.
 * (헤더에서 딸려온 함수 "선언"(Decl)은 제외, 실제 함수 "정의"(FuncDef)만 카운트)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>   /* 콘솔 한글 출력용 (윈도우) */
#endif
#include "json_c.c"

/* ------------------------------------------------------------------
 * 타입 문자열 추출
 *   "type" 키를 가진 노드(FuncDecl 또는 Decl)를 받아서,
 *   TypeDecl 이 나올 때까지 내려가며 실제 타입 이름을 buf 에 채운다.
 *   중간에 PtrDecl(포인터)을 만나면 '*' 를 붙인다.
 *   리턴타입과 파라미터 타입 모두 이 함수로 처리(구조가 동일).
 * ------------------------------------------------------------------ */
void get_type_string(json_value start_node, char *buf)
{
    buf[0] = '\0';
    json_value cur = json_get(start_node, "type");

    while (1) {
        json_value nt = json_get(cur, "_nodetype");
        if (json_get_type(nt) != JSON_STRING) {   /* 타입 정보가 없으면 종료 */
            strcat(buf, "?");
            break;
        }
        char *t = json_to_string(nt);

        if (strcmp(t, "PtrDecl") == 0) {
            strcat(buf, "*");                 /* 포인터: '*' 모아두고 한 단계 더 */
            cur = json_get(cur, "type");
        }
        else if (strcmp(t, "TypeDecl") == 0) {
            /* TypeDecl -> type -> names[0] 이 실제 타입 이름 */
            json_value idtype = json_get(cur, "type");
            json_value names  = json_get(idtype, "names");
            char *typename = json_to_string(json_get(names, 0));

            char tmp[128];
            strcpy(tmp, typename);
            strcat(tmp, buf);                 /* "int" + 그동안 모은 '*' */
            strcpy(buf, tmp);
            break;
        }
        else {
            /* ArrayDecl 등 예상 못한 노드 */
            strcat(buf, "?");
            break;
        }
    }
}

/* ------------------------------------------------------------------
 * if 개수 세기 (재귀)
 *   어떤 노드든 받아서, 그 하위 트리에 등장하는 모든 "If" 노드 수를 센다.
 *   중첩 if, while/for 안의 if, else 안의 if 까지 전부 포함.
 * ------------------------------------------------------------------ */
int count_ifs(json_value node)
{
    json_type t = json_get_type(node);

    if (t == JSON_OBJECT) {
        int total = 0;

        json_value nt = json_get(node, "_nodetype");
        if (json_get_type(nt) == JSON_STRING &&
            strcmp(json_to_string(nt), "If") == 0) {
            total += 1;                       /* 이 노드 자체가 If */
        }

        int len = json_len(node);             /* 객체의 모든 값을 재귀 검사 */
        for (int i = 0; i < len; i++)
            total += count_ifs(json_get(node, i));
        return total;
    }

    if (t == JSON_ARRAY) {
        int total = 0;
        int len = json_len(node);
        for (int i = 0; i < len; i++)
            total += count_ifs(json_get(node, i));
        return total;
    }

    return 0;                                 /* 문자열/숫자/null 은 셀 것 없음 */
}

/* ------------------------------------------------------------------
 * 한 FuncDef 노드의 정보를 출력
 * ------------------------------------------------------------------ */
void print_function_info(int idx, json_value funcdef)
{
    json_value decl     = json_get(funcdef, "decl");
    char       *name    = json_to_string(json_get(decl, "name"));
    json_value funcdecl = json_get(decl, "type");   /* FuncDecl */

    /* (2) 리턴타입 */
    char rettype[128];
    get_type_string(funcdecl, rettype);

    /* (3) 이름 + (2) 리턴타입 + (4) 파라미터 를 한 줄 시그니처로 */
    printf("[%2d] %s %s(", idx, rettype, name);

    /* (4) 파라미터 */
    json_value args = json_get(funcdecl, "args");
    if (json_get_type(args) != JSON_OBJECT) {
        printf("void");                        /* args 없음 = 파라미터 없는 함수 */
    } else {
        json_value params = json_get(args, "params");
        int plen = json_len(params);
        for (int j = 0; j < plen; j++) {
            json_value p = json_get(params, j);

            char ptype[128];
            get_type_string(p, ptype);         /* 파라미터 타입 */

            json_value pname = json_get(p, "name");   /* 파라미터 변수명 */
            char *pname_str = (json_get_type(pname) == JSON_STRING)
                              ? json_to_string(pname) : "";

            printf("%s %s", ptype, pname_str);
            if (j != plen - 1) printf(", ");
        }
    }
    printf(")");

    /* (5) if 개수 */
    json_value body = json_get(funcdef, "body");
    int ifs = count_ifs(body);
    printf("   | if x %d\n", ifs);
}

/* ------------------------------------------------------------------ */
int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);   /* 윈도우 콘솔 UTF-8 (한글 깨짐 방지) */
#endif

    json_value root = json_read("ast.json");
    json_value ext  = json_get(root, "ext");
    int ext_len = json_len(ext);

    /* (1) 함수 개수: ext 안에서 _nodetype == "FuncDef" 인 것만 카운트 */
    int func_count = 0;
    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        json_value nt   = json_get(node, "_nodetype");
        if (json_get_type(nt) == JSON_STRING &&
            strcmp(json_to_string(nt), "FuncDef") == 0) {
            func_count++;
        }
    }

    printf("======================================================\n");
    printf(" AST static analysis (ast.json)\n");
    printf("======================================================\n");
    printf("(1) function count : %d\n", func_count);
    printf("------------------------------------------------------\n");
    printf("(2)~(5) per-function info\n");
    printf("   format: [no] <rettype> <name>(<params>)  | if count\n");
    printf("------------------------------------------------------\n");

    /* (2)~(5): FuncDef 마다 리턴타입/이름/파라미터/if개수 */
    int idx = 0;
    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        json_value nt   = json_get(node, "_nodetype");
        if (json_get_type(nt) != JSON_STRING ||
            strcmp(json_to_string(nt), "FuncDef") != 0)
            continue;

        idx++;
        print_function_info(idx, node);
    }

    printf("======================================================\n");
    return 0;
}