#include "모듈I.h"

// [내부 도우미 함수] 버퍼 오버플로우를 막으면서 안전하게 문자열을 이어 붙이는 함수
static void append_str(char* buf, size_t max_len, const char* str) {
    //(글자를 이어붙인 도화지의 시작주소, 도화지 전체 칸수, 새로 이어붙일 문자열)
    if (buf == NULL || str == NULL) return;
    strncat_s(buf, max_len, str, _TRUNCATE);
    //strncat = string + concatenate 말 그대로 문자열 이어붙이기
    //strncat_s(시작주소, 도화지 크기, 이어붙인 문자열, 소숫점 버리기)
}

// ★AST 트리를 재귀적으로 순회하며 LaTeX 문자열 조립
void ast_to_latex(const ASTNode* node, char* out_buf, size_t max_len) {
    if (node == NULL || out_buf == NULL) return;

    // 1. 숫자 또는 변수: 자기 자신의 값을 그대로 적음 ("2", "x")
    if (node->type == NODE_NUMBER || node->type == NODE_VARIABLE) {
        append_str(out_buf, max_len, node->value); 
        return;
    }

    // 2. 이항 연산자 (+, -, *, /, ^)
    if (node->type == NODE_BINARY_OP) {
        if (strcmp(node->value, "/") == 0) {
            // 분수: \frac{분자}{분모}
            append_str(out_buf, max_len, "\\frac{");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "}{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "^") == 0) {
            // 거듭제곱: 밑^{지수}
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "^{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "*") == 0) {
            // 곱셈: 왼쪽 \cdot 오른쪽
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, " \\cdot ");
            ast_to_latex(node->right, out_buf, max_len);
        }
        else {
            // 덧셈(+), 뺄셈(-): 왼쪽 + 연산자 + 오른쪽
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, " ");
            append_str(out_buf, max_len, node->value);
            append_str(out_buf, max_len, " ");
            ast_to_latex(out_buf, out_buf, max_len); // 주의! 아래 줄처럼 node->right로 작성!
        }
        return;
    }

    // 3. 단일 인자 수학 함수 (sin, cos, ln, exp 등): \함수명\left(인자\right)
    if (node->type == NODE_FUNCTION) 
    {
            // 예외: 제곱근(sqrt)은 괄호 대신 중괄호 \sqrt{인자} 사용
            if (strcmp(node->value, "sqrt") == 0) 
            {
                append_str(out_buf, max_len, "\\sqrt{");
                ast_to_latex(node->left, out_buf, max_len);
                append_str(out_buf, max_len, "}");
                return;
            }

            // 일반 수학 함수: \sin\left(인자\right)
            append_str(out_buf, max_len, "\\");
            append_str(out_buf, max_len, node->value);
            append_str(out_buf, max_len, "\\left(");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "\\right)");
            return;
    }

    // 4. 다중 인자 미적분 연산자 (lim, sum, int)
    if (node->type == NODE_SPECIAL_OP) {
        if (strcmp(node->value, "lim") == 0 && node->arg_count == 3) {
            // \lim_{args[0] \to args[1]} args[2]
            append_str(out_buf, max_len, "\\lim_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, " \\to ");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
        }
        else if (strcmp(node->value, "sum") == 0 && node->arg_count == 4) {
            // \sum_{args[0]=args[1]}^{args[2]} args[3]
            append_str(out_buf, max_len, "\\sum_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "=");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        else if (strcmp(node->value, "int") == 0 && node->arg_count == 4) {
            // \int_{args[0]}^{args[1]} args[2] \, d(args[3])
            append_str(out_buf, max_len, "\\int_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, " \\, d");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        return;
    }
}

int main(void) {
    char input[256];

    printf("   CalcTech 3-Tier 수식 파서 & LaTeX 변환기 테스트\n");
    printf("   (종료하려면 'exit' 입력)\n");

    while (1) {
        printf("\n수식 입력 > ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        // 줄바꿈(\n) 제거
        input[strcspn(input, "\n")] = '\0';

        // 종료 조건
        if (strcmp(input, "exit") == 0) break;
        if (strlen(input) == 0) continue;

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
        char latex_result[2048] = ""; // 반드시 빈 문자열("")로 초기화!
        ast_to_latex(root, latex_result, sizeof(latex_result));
        printf("\n[3단계: 최종 LaTeX 변환 결과]\n");
        printf("$$ %s $$\n", latex_result);

        // 동적 할당 메모리 정리
        free_ast(root);
        printf("--------------------------------------------------\n");
    }

    return 0;
}