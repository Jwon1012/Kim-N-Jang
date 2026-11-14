#include "모듈I.h"

// [내부 도우미 함수] 버퍼 오버플로우를 막으면서 안전하게 문자열을 이어 붙이는 함수
extern void append_str(char* buf, size_t max_len, const char* str) 
{
    if (buf == NULL || str == NULL) return;
    strncat_s(buf, max_len, str, _TRUNCATE);
}

// ★ AST 트리를 재귀적으로 순회하며 LaTeX 문자열 조립
void ast_to_latex(const ASTNode* node, char* out_buf, size_t max_len) 
{
    if (node == NULL || out_buf == NULL) return;

    // 1. 숫자(NODE_NUMBER) 또는 변수(NODE_VARIABLE)
    if (node->type == NODE_NUMBER || node->type == NODE_VARIABLE) 
    {
        if (strcmp(node->value, "inf") == 0 || strcmp(node->value, "infty") == 0) append_str(out_buf, max_len, "\\infty");
        else if (strcmp(node->value, "pi") == 0)  append_str(out_buf, max_len, "\\pi");
        else append_str(out_buf, max_len, node->value);
        return;
    }

    // 2. 이항 연산자 (+, -, *, /, ^) 및 단항 음수 (-x)
    if (node->type == NODE_BINARY_OP) 
    {
        if (strcmp(node->value, "/") == 0) 
        {
            append_str(out_buf, max_len, "\\frac{");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "}{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "^") == 0) 
        {
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "^{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "*") == 0) 
        {
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, " \\cdot ");
            ast_to_latex(node->right, out_buf, max_len);
        }
        else 
        {
            // 단항 마이너스(-x)인 경우 left가 NULL
            if (node->left == NULL && strcmp(node->value, "-") == 0) 
            {
                append_str(out_buf, max_len, "-");
                ast_to_latex(node->right, out_buf, max_len);
            }
            else 
            {
                ast_to_latex(node->left, out_buf, max_len);
                append_str(out_buf, max_len, " ");
                append_str(out_buf, max_len, node->value);
                append_str(out_buf, max_len, " ");
                ast_to_latex(node->right, out_buf, max_len); 
            }
        }
        return;
    }

    // 3. 단일 인자 수학 함수 (sin, cos, ln, exp 등)
    if (node->type == NODE_FUNCTION)
    {
        if (strcmp(node->value, "sqrt") == 0) {
            append_str(out_buf, max_len, "\\sqrt{");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "}");
            return;
        }
        if (strcmp(node->value, "abs") == 0) { //절댓값도 일종의 함수
            append_str(out_buf, max_len, "\\left|");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "\\right|");
            return;
        }
        if (strcmp(node->value, "prime") == 0) { //prime은 뉴턴식 미분, 
            append_str(out_buf, max_len, "\\left("); //f'(x)로 쓸 수도 있지만 여러번 미분하는 케이스도 고려
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "\\right)'");
            return;
        }
        // 기존 일반 삼각·로그 함수 처리...
        append_str(out_buf, max_len, "\\");
        append_str(out_buf, max_len, node->value);
        append_str(out_buf, max_len, "\\left(");
        ast_to_latex(node->left, out_buf, max_len);
        append_str(out_buf, max_len, "\\right)");
        return;
    }

    // 4. 다중 인자 미적분 연산자 (lim, sum, int)
    if (node->type == NODE_SPECIAL_OP) 
    {
        if (strcmp(node->value, "lim") == 0 && node->arg_count == 3) 
        {
            append_str(out_buf, max_len, "\\lim_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, " \\to ");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
        }
        else if (strcmp(node->value, "sum") == 0 && node->arg_count == 4) 
        {
            append_str(out_buf, max_len, "\\sum_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "=");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        else if (strcmp(node->value, "int") == 0 && node->arg_count == 4) 
        {
            append_str(out_buf, max_len, "\\int_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, " \\, d");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        else if (strcmp(node->value, "frac") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\frac{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}{");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "logbase") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\log_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}\\left(");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "\\right)");
        }
        else if (strcmp(node->value, "indef_int") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\int ");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, " \\, d");
            ast_to_latex(node->args[1], out_buf, max_len);
        }
        else if (strcmp(node->value, "diff") == 0 && node->arg_count == 3) 
        {
            // \frac{d^{차수}}{d변수^{차수}} 피미분식
            append_str(out_buf, max_len, "\\frac{d^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "}}{d");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "}} ");
            ast_to_latex(node->args[0], out_buf, max_len);
        }
        return;
    }
}

void print_help(void)
{
    printf("\n==================== [ CalcTech 수식 입력 가이드 ] ====================\n");
    printf(" 1. 기본 연산 및 상수\n");
    printf("    - 사칙연산 / 거듭제곱 / 팩토리얼 : +, -, *, /, ^, !   (예: 2x^2 + 5!)\n");
    printf("    - 수학 상수                      : pi, inf, infty\n\n");

    printf(" 2. 단일 인자 함수 (인자 1개)\n");
    printf("    - sqrt(x)  : 제곱근              | abs(x)   : 절댓값 (|x|)\n");
    printf("    - prime(f) : 프라임 미분 (f')    | ln(x)    : 자연로그\n");
    printf("    - log(x)   : 상용로그            | sin, cos, tan, exp 등\n\n");

    printf(" 3. 대형 수학 연산자 및 다중 인자 함수 (인자 순서 주의!)\n");
    printf("    - frac(분자, 분모)               : 분수\n");
    printf("    - logbase(밑, 진수)              : 밑이 있는 로그\n");
    printf("    - indef_int(피적분식, 변수)      : 부정적분\n");
    printf("    - diff(피미분식, 변수, 차수)     : 라이프니츠 미분\n");
    printf("    - lim(변수, 목표값, 식)          : 극한 (예: lim(x, inf, 1/x))\n");
    printf("    - sum(변수, 시작값, 끝값, 식)    : 시그마 합 (예: sum(k, 1, n, k^2))\n");
    printf("    - int(아래끝, 위끝, 식, 변수)    : 정적분 (예: int(0, pi, sin(x), x))\n");
    printf("=======================================================================\n");
}

    int main(void) 
    {
    char input[256];

    printf("CalcTech 3-Tier 수식 파서 & LaTeX 변환기 테스트\n");
    printf("(종료하려면 'exit' 입력)\n");

    while (1) 
    {
        printf("\n수식 입력 > ");

        if (fgets(input, sizeof(input), stdin) == NULL) break;

        input[strcspn(input, "\r\n")] = '\0';

        if (strcmp(input, "exit") == 0) break;
        if (strlen(input) == 0) continue;
        if (strcmp(input, "/?") == 0) print_help(); continue;


        // [1단계] Lexer: 문자열 -> 토큰 리스트
        TokenList tokens = tokenize(input);
        printf("\n[1단계: 토큰 분석 결과]\n");
        print_tokens(&tokens);


        // [2단계] Parser: 토큰 리스트 -> AST 트리
        Parser parser = { &tokens, 0 };
        ASTNode* root = parse_expression(&parser);
        printf("\n[2단계: AST 트리 구조]\n");
        print_ast_tree(root);


        // [3단계] LaTeX Generator: AST 트리 -> LaTeX 문자열
        char latex_result[2048] = "";
        ast_to_latex(root, latex_result, sizeof(latex_result));
        printf("\n[3단계: 최종 LaTeX 변환 결과]\n");
        printf("$$ %s $$\n", latex_result);


        // 동적 할당 메모리 정리
        free_ast(root);
        free_token_list(&tokens); // [추가] 토큰 리스트 메모리 누수 방지
        printf("--------------------------------------------------\n");
    }

    return 0;
}