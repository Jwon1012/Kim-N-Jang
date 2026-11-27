#include "CalcTech공용두꺼비집.h"

static void print_help(void)
{
    printf("\n==================== [ CalcTech 수식 입력 가이드 ] ====================\n");
    printf(" 1. 기본 연산 및 상수\n");
    printf("    - 사칙연산 / 거듭제곱 / 팩토리얼 : +, -, *, /, ^, !   (예: 2x^2 + 5!)\n");
    printf("    - 수학 상수                      : pi, inf, infty\n\n");

    printf(" 2. 단일 인자 함수 (인자 1개)\n");
    printf("    - sqrt(x)  : 제곱근              | abs(x)   : 절댓값 (|x|)\n");
    printf("    - prime(f) : 프라임 미분 (f')    | ln(x)    : 자연로그\n");
    printf("    - log(x)   : 상용로그            | sin, cos, tan, exp 등\n");
    printf("    - gamma(x): 감마함수 ※  0이상의 모든 실수에 대응하는 팩토리얼\n\n");

    printf(" 3. 대형 수학 연산자 및 다중 인자 함수 (인자 순서 주의!)\n");
    printf("    - frac(분자, 분모)               : 분수\n");
    printf("    - logbase(밑, 진수)              : 밑이 있는 로그\n");
    printf("    - indef_int(피적분식, 변수)      : 부정적분\n");
    printf("    - diff(피미분식, 변수, 차수)     : 라이프니츠 미분\n");
    printf("    - lim(변수, 목표값, 식)          : 극한 (예: lim(x, inf, 1/x))\n");
    printf("    - sum(변수, 시작값, 끝값, 식)    : 시그마 합 (예: sum(k, 1, n, k^2))\n");
    printf("    - int(아래끝, 위끝, 식, 변수)    : 정적분 (예: int(0, pi, sin(x), x))\n");
    printf("    - abs(식)                      : 절댓값 함수 (예: abs(-4) => 4, abs(x) => |x|\n");
    printf("=======================================================================\n");
}

// lim(x, target, expr) 형태의 AST 노드를 수치적으로 평가해 극한값 반환
static double eval_limit_node(const ASTNode* lim_node) {
    if (!lim_node || lim_node->type != NODE_SPECIAL_OP || strcmp(lim_node->value, "lim") != 0) {
        return NAN;
    }
    if (lim_node->arg_count < 3 || !lim_node->args[1] || !lim_node->args[2]) {
        return NAN;
    }

    double target = eval_ast(lim_node->args[1], 0.0);
    const ASTNode* expr = lim_node->args[2];

    // 1. x -> +∞ 또는 x -> -∞ 극한
    if (isinf(target)) {
        double dir = (target > 0) ? 1.0 : -1.0;
        double y1 = eval_ast(expr, dir * 1e5);
        double y2 = eval_ast(expr, dir * 1e6);
        if (isnan(y1) || isnan(y2)) return NAN;
        if (isinf(y2)) return y2;
        if (fabs(y2 - y1) < 1e-4) return y2;
        return NAN; // 진동 또는 발산
    }

    // 2. x -> a 유한 극한 (좌극한과 우극한 비교)
    double lim_left = eval_ast(expr, target - 1e-7);
    double lim_right = eval_ast(expr, target + 1e-7);

    if (isnan(lim_left) || isinf(lim_left) || isnan(lim_right) || isinf(lim_right)) {
        return NAN;
    }
    if (fabs(lim_left - lim_right) < 1e-4) {
        return 0.5 * (lim_left + lim_right);
    }
    return NAN; // 좌·우극한 불일치 (극한 존재 안 함)
}

// 모듈 3 심화 특징점 & 주기 & 특수식 결과 출력기 (존재하는 정보만 출력)
static void print_advanced_analysis(const ASTNode* root, double x_min, double x_max) {
    if (!root) return;

    char sym_buf[64];
    int printed_any = 0;

    printf("\n[4단계: 모듈 III 심화 수학 정보 & 기호/무리수 복원]\n");

    // (A) 정적분으로 정의된 수식인 경우: int(a, b, f(x), x)
    if (root->type == NODE_SPECIAL_OP && strcmp(root->value, "int") == 0 && root->arg_count == 4) {
        double val = eval_ast(root, 0.0);
        format_symbolic_or_decimal(val, sym_buf, sizeof(sym_buf));
        printf("  ▶ 정적분 계산 결과 : %s  (근사치: %.6f)\n", sym_buf, val);
        return; // 단일 상수값 정적분식은 그래프 특징점 생략
    }

    // (B) 극한으로 정의된 수식인 경우: lim(x, target, f(x))
    if (root->type == NODE_SPECIAL_OP && strcmp(root->value, "lim") == 0 && root->arg_count == 3) {
        double lim_val = eval_limit_node(root);
        if (isnan(lim_val)) {
            printf("  ▶ 극한값 판정      : 극한이 존재하지 않거나 발산함\n");
        }
        else if (isinf(lim_val)) {
            printf("  ▶ 극한값 판정      : %s\n", (lim_val > 0 ? "+∞ (양의 무한대 발산)" : "-∞ (음의 무한대 발산)"));
        }
        else {
            format_symbolic_or_decimal(lim_val, sym_buf, sizeof(sym_buf));
            printf("  ▶ 극한값 계산 결과 : %s  (근사치: %.6f)\n", sym_buf, lim_val);
        }
        return;
    }

    // (C) 일반 함수 f(x)의 주기 판별
    char period_label[32];
    double period = detect_function_period(root, 20.0, period_label, sizeof(period_label));
    int is_periodic = (!isnan(period) && strcmp(period_label, "None") != 0);

    double target_min = x_min;
    double target_max = x_max;

    if (is_periodic) {
        printf("  ▶ 최소 주기 (p)    : %s  (근사치: %.6f)\n", period_label, period);
        printf("  ▶ 분석 기준 구간   : 첫 번째 주기 [0, %s)\n", period_label);
        target_min = 0.0;
        target_max = period - 1e-7; // x = 0 과 x = p 끝점 중복 등록 방지
        printed_any = 1;
    }

    // (D) 구간 [x_min, x_max] 내 심화 특징점 추출
    GraphKeyPoint kps[MAX_KEY_POINTS];
    int kp_cnt = extract_graph_keypoints(root, NULL, target_min, target_max, kps, MAX_KEY_POINTS);

    // 1) X축과의 교점 (X절편)
    int has_x_int = 0;
    for (int i = 0; i < kp_cnt; i++) {
        if (kps[i].type == KP_X_INTERCEPT) {
            if (!has_x_int) { printf("  ▶ X축과의 교점     : "); has_x_int = 1; printed_any = 1; }
            else printf(", ");
            printf("%s", kps[i].label);
        }
    }
    if (has_x_int) printf("\n");

    // 2) Y축과의 교점 (Y절편)
    for (int i = 0; i < kp_cnt; i++) {
        if (kps[i].type == KP_Y_INTERCEPT) {
            printf("  ▶ Y축과의 교점     : %s\n", kps[i].label);
            printed_any = 1;
        }
    }

    // 3) 극점 (극댓값 / 극솟값)
    for (int i = 0; i < kp_cnt; i++) {
        if (kps[i].type == KP_LOCAL_MAX) {
            printf("  ▶ 극대점           : %s  (근사 x=%.6f, y=%.6f)\n", kps[i].label, kps[i].x, kps[i].y);
            printed_any = 1;
        }
        else if (kps[i].type == KP_LOCAL_MIN) {
            printf("  ▶ 극소점           : %s  (근사 x=%.6f, y=%.6f)\n", kps[i].label, kps[i].x, kps[i].y);
            printed_any = 1;
        }
    }

    // 4) 변곡점 및 해당 지점의 변곡접선 기울기 f'(x_inf)
    for (int i = 0; i < kp_cnt; i++) {
        if (kps[i].type == KP_INFLECTION) {
            double inf_slope = eval_nth_derivative_safe(root, kps[i].x, 1);
            format_symbolic_or_decimal(inf_slope, sym_buf, sizeof(sym_buf));
            printf("  ▶ 변곡점           : %s  | 변곡접선 기울기 = %s (근사: %.6f)\n",
                kps[i].label, sym_buf, inf_slope);
            printed_any = 1;
        }
    }

    // 5) 극한값만 존재하는 점 (제거 가능 불연속점 / Hole)
    for (int i = 0; i < kp_cnt; i++) {
        if (kps[i].type == KP_HOLE) {
            printf("  ▶ 구멍 불연속점    : %s (극한값만 존재)\n", kps[i].label);
            printed_any = 1;
        }
    }

    if (!printed_any) {
        printf("  ▶ 탐색 구간 [%.2f, %.2f] 내에 표시할 특이점/주기가 없습니다.\n", x_min, x_max);
    }
}
// main.c -> print_advanced_analysis() 바로 위에 추가
static int is_exact_decimal_label(const char* str) {
    // 분수('/')나 기호 상수(pi, e, sqrt, ln)가 없으면 이미 딱 떨어지는 숫자
    return (strpbrk(str, "/pisqreln") == NULL);
}
int main(void) {
    char input[256];
    double x_min = -10.0, x_max = 10.0;

    printf("=================================================================\n");
    printf("  CalcTech 통합 파이프라인 테스트 (모듈 I 파서 + 모듈 III 분석기)\n");
    printf("  기본 탐색 구간: [%.1f, %.1f]  (구간 변경: range a b / 종료: exit / 도움말: /?)\n", x_min, x_max);
    printf("=================================================================\n");

    while (1) {
        printf("\n수식 입력 > ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\r\n")] = '\0';

        if (strcmp(input, "exit") == 0) break;
        if (strcmp(input, "/?") == 0) print_help();
        if (strlen(input) == 0) continue;

        // 탐색 구간 변경 명령어 (예: range -5 5)
        if (strncmp(input, "range ", 6) == 0) {
            double a, b;
            if (sscanf_s(input + 6, "%lf %lf", &a, &b) == 2 && a < b) {
                x_min = a; x_max = b;
                printf("  [설정 완료] 탐색 구간이 [%.2f, %.2f]로 변경되었습니다.\n", x_min, x_max);
            }
            else {
                printf("  [입력 오류] 예: range -10 10\n");
            }
            continue;
        }

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

        // [4단계] 모듈 III 심화 정보 추출 및 무리수/기호 복원 검증
        print_advanced_analysis(root, x_min, x_max);

        free_ast(root);
        free_token_list(&tokens);
        printf("-----------------------------------------------------------------\n");
    }

    return 0;
}
