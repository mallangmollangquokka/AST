#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "json_c.c"

/* 노드의 _nodetype 문자열을 안전하게 얻기 */
const char* ntype(json_value node) {
    json_value nt = json_get(node, "_nodetype");
    if (json_get_type(nt) == JSON_STRING) return json_to_string(nt);
    return "";
}
/* 타입 문자열 추출 (ast.c 와 동일) */
void get_type_string(json_value start_node, char *buf)
{
    buf[0] = '\0';
    json_value cur = json_get(start_node, "type");
    while (1) {
        json_value nt = json_get(cur, "_nodetype");
        if (json_get_type(nt) != JSON_STRING) { strcat(buf, "?"); break; }
        char *t = json_to_string(nt);
        if (strcmp(t, "PtrDecl") == 0) {
            strcat(buf, "*");
            cur = json_get(cur, "type");
        } else if (strcmp(t, "TypeDecl") == 0) {
            json_value idtype = json_get(cur, "type");
            json_value names  = json_get(idtype, "names");
            char *typename = json_to_string(json_get(names, 0));
            char tmp[128];
            strcpy(tmp, typename);
            strcat(tmp, buf);
            strcpy(buf, tmp);
            break;
        } else { strcat(buf, "?"); break; }
    }
}

/* 들여쓰기 출력 */
void indent(char *out, int depth) {
    for (int i = 0; i < depth; i++) strcat(out, "    ");
}

/* 전방 선언 (서로 호출하므로) */
void gen(json_value node, char *out, int depth);
void gen_stmt(json_value node, char *out, int depth);

/* 문장 하나를 출력 (들여쓰기 + 세미콜론 책임) */
void gen_stmt(json_value node, char *out, int depth)
{
    const char *t = ntype(node);

    /* Compound: 블록 자체는 호출한 쪽에서 처리하므로 여기선 표현식 문장 위주 */
    if (strcmp(t, "Compound") == 0) {
        gen(node, out, depth);
        return;
    }
    /* 제어문(while/if)은 세미콜론 없이 gen 에 위임 */
    if (strcmp(t, "While") == 0 || strcmp(t, "If") == 0) {
        indent(out, depth);
        gen(node, out, depth);
        strcat(out, "\n");
        return;
    }
    /* Decl, Return 은 자체적으로 ; 붙임 */
    if (strcmp(t, "Decl") == 0 || strcmp(t, "Return") == 0) {
        indent(out, depth);
        gen(node, out, depth);
        strcat(out, "\n");
        return;
    }
    /* 그 외(Assignment, FuncCall 등 표현식 문장): ; 붙임 */
    indent(out, depth);
    gen(node, out, depth);
    strcat(out, ";\n");
}
/* 이항 연산자의 우선순위 반환 (클수록 먼저 묶임). BinaryOp가 아니면 999(괄호 불필요) */
int op_priority(json_value node)
{
    const char *nt = ntype(node);

    if (strcmp(nt, "Assignment") == 0) return 1;   /* 대입은 최하위급 */

    if (strcmp(nt, "BinaryOp") != 0) return 999;   /* 단순 값은 괄호 불필요 */

    char *op = json_to_string(json_get(node, "op"));

    if (!strcmp(op,"*")||!strcmp(op,"/")||!strcmp(op,"%")) return 12;
    if (!strcmp(op,"+")||!strcmp(op,"-"))                  return 11;
    if (!strcmp(op,"<<")||!strcmp(op,">>"))                return 10;
    if (!strcmp(op,"<")||!strcmp(op,"<=")||
        !strcmp(op,">")||!strcmp(op,">="))                 return 9;
    if (!strcmp(op,"==")||!strcmp(op,"!="))                return 8;
    if (!strcmp(op,"&"))                                   return 7;
    if (!strcmp(op,"^"))                                   return 6;
    if (!strcmp(op,"|"))                                   return 5;
    if (!strcmp(op,"&&"))                                  return 4;
    if (!strcmp(op,"||"))                                  return 3;
    return 2;
}

/* 노드 하나를 C 코드로 변환 (재귀) */
void gen(json_value node, char *out, int depth)
{
    const char *t = ntype(node);

    /* ---------- 표현식 ---------- */
    if (strcmp(t, "Constant") == 0) {
        json_value v = json_get(node, "value");
        if (json_get_type(v) == JSON_STRING) strcat(out, json_to_string(v));
        return;
    }
    if (strcmp(t, "ID") == 0) {
        json_value n = json_get(node, "name");
        if (json_get_type(n) == JSON_STRING) strcat(out, json_to_string(n));
        return;
    }
    if (strcmp(t, "BinaryOp") == 0) {
        int my_pri = op_priority(node);
        json_value left  = json_get(node, "left");
        json_value right = json_get(node, "right");

        /* 왼쪽 자식: 우선순위가 나보다 낮으면 괄호 */
        if (op_priority(left) < my_pri) {
            strcat(out, "("); gen(left, out, depth); strcat(out, ")");
        } else {
            gen(left, out, depth);
        }

        strcat(out, " ");
        strcat(out, json_to_string(json_get(node, "op")));
        strcat(out, " ");

        /* 오른쪽 자식: 우선순위가 나보다 낮거나 같으면 괄호
           (같은 우선순위도 괄호 — 왼쪽 결합성 때문에 a-(b-c) 같은 경우 보호) */
        if (op_priority(right) <= my_pri) {
            strcat(out, "("); gen(right, out, depth); strcat(out, ")");
        } else {
            gen(right, out, depth);
        }
        return;
    }
    if (strcmp(t, "Assignment") == 0) {
        gen(json_get(node, "lvalue"), out, depth);
        strcat(out, " ");
        strcat(out, json_to_string(json_get(node, "op")));
        strcat(out, " ");
        /* 오른쪽이 또 대입이면 괄호 불필요(우결합), 그 외는 그대로 */
        gen(json_get(node, "rvalue"), out, depth);
        return;
    }
    if (strcmp(t, "ArrayRef") == 0) {
        gen(json_get(node, "name"), out, depth);
        strcat(out, "[");
        gen(json_get(node, "subscript"), out, depth);
        strcat(out, "]");
        return;
    }
    if (strcmp(t, "FuncCall") == 0) {
        gen(json_get(node, "name"), out, depth);
        strcat(out, "(");
        json_value args = json_get(node, "args");
        if (json_get_type(args) == JSON_OBJECT) gen(args, out, depth);
        strcat(out, ")");
        return;
    }
    if (strcmp(t, "ExprList") == 0) {
        json_value exprs = json_get(node, "exprs");
        int len = json_len(exprs);
        for (int i = 0; i < len; i++) {
            gen(json_get(exprs, i), out, depth);
            if (i != len - 1) strcat(out, ", ");
        }
        return;
    }

    /* ---------- 문장 ---------- */
    if (strcmp(t, "Return") == 0) {
        strcat(out, "return");
        json_value e = json_get(node, "expr");
        if (json_get_type(e) == JSON_OBJECT) { strcat(out, " "); gen(e, out, depth); }
        strcat(out, ";");
        return;
    }
    if (strcmp(t, "Decl") == 0) {
        char type_str[128];
        get_type_string(node, type_str);
        strcat(out, type_str);
        strcat(out, " ");
        json_value nm = json_get(node, "name");
        if (json_get_type(nm) == JSON_STRING) strcat(out, json_to_string(nm));
        json_value init = json_get(node, "init");
        if (json_get_type(init) == JSON_OBJECT) { strcat(out, " = "); gen(init, out, depth); }
        strcat(out, ";");
        return;
    }
    if (strcmp(t, "Compound") == 0) {
        strcat(out, "{\n");
        json_value items = json_get(node, "block_items");
        if (json_get_type(items) == JSON_ARRAY) {
            int len = json_len(items);
            for (int i = 0; i < len; i++)
                gen_stmt(json_get(items, i), out, depth + 1);   /* 한 단계 더 들여씀 */
        }
        indent(out, depth);
        strcat(out, "}");
        return;
    }
    if (strcmp(t, "While") == 0) {
        strcat(out, "while (");
        gen(json_get(node, "cond"), out, depth);
        strcat(out, ") ");
        json_value stmt = json_get(node, "stmt");
        if (strcmp(ntype(stmt), "Compound") == 0) {
            gen(stmt, out, depth);                  /* 블록이면 같은 줄에 { */
        } else {
            strcat(out, "\n");                      /* 단일 문장이면 다음 줄 */
            gen_stmt(stmt, out, depth + 1);
        }
        return;
    }
    if (strcmp(t, "If") == 0) {
        strcat(out, "if (");
        gen(json_get(node, "cond"), out, depth);
        strcat(out, ") ");
        json_value iftrue = json_get(node, "iftrue");
        if (strcmp(ntype(iftrue), "Compound") == 0) {
            gen(iftrue, out, depth);
        } else {
            strcat(out, "\n");
            gen_stmt(iftrue, out, depth + 1);
        }
        json_value iffalse = json_get(node, "iffalse");
        if (json_get_type(iffalse) == JSON_OBJECT) {
            strcat(out, " else ");
            if (strcmp(ntype(iffalse), "Compound") == 0) gen(iffalse, out, depth);
            else { strcat(out, "\n"); gen_stmt(iffalse, out, depth + 1); }
        }
        return;
    }

    /* 미처리 */
    strcat(out, "<"); strcat(out, t); strcat(out, ">");
}

/* 함수 시그니처 만들기: "int sym_lookup(char* s)" (ast.c 로직 재사용) */
void gen_signature(json_value funcdef, char *out)
{
    json_value decl     = json_get(funcdef, "decl");
    char       *name    = json_to_string(json_get(decl, "name"));
    json_value funcdecl = json_get(decl, "type");

    char rettype[128];
    get_type_string(funcdecl, rettype);

    strcat(out, rettype);
    strcat(out, " ");
    strcat(out, name);
    strcat(out, "(");

    json_value args = json_get(funcdecl, "args");
    if (json_get_type(args) != JSON_OBJECT) {
        strcat(out, "void");
    } else {
        json_value params = json_get(args, "params");
        int plen = json_len(params);
        for (int j = 0; j < plen; j++) {
            json_value p = json_get(params, j);
            char ptype[128];
            get_type_string(p, ptype);
            strcat(out, ptype);
            strcat(out, " ");
            json_value pname = json_get(p, "name");
            if (json_get_type(pname) == JSON_STRING) strcat(out, json_to_string(pname));
            if (j != plen - 1) strcat(out, ", ");
        }
    }
    strcat(out, ")");
}

/* 함수 전체(시그니처 + body) 복원 */
void gen_function(json_value funcdef, char *out)
{
    gen_signature(funcdef, out);
    strcat(out, "\n{\n");

    json_value block = json_get(json_get(funcdef, "body"), "block_items");
    if (json_get_type(block) == JSON_ARRAY) {
        int blen = json_len(block);
        for (int j = 0; j < blen; j++)
            gen_stmt(json_get(block, j), out, 1);   /* depth 1 = 함수 안 */
    }
    strcat(out, "}\n\n");
}

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    json_value root = json_read("ast.json");
    json_value ext  = json_get(root, "ext");
    int ext_len = json_len(ext);

    char *code = (char *)malloc(2000000);   /* 36개니까 크게 */
    code[0] = '\0';

    /* 모든 FuncDef 복원 */
    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        if (strcmp(ntype(node), "FuncDef") != 0) continue;

        gen_function(node, code);
    }

    printf("%s", code);

    /* 파일로 저장 */
    FILE *fp = fopen("restored.c", "w");
    if (fp) {
        fputs(code, fp);
        fclose(fp);
        printf("\n>>> saved to restored.c\n");
    }

    free(code);
    return 0;
}