#include "CalcTech공용두꺼비집.h"

// 1. AST 수식 평가기: 트리를 후위 순회하며 x 대입 결과(double) 반환
double eval_ast(const ASTNode* node, double x) 
{
    if (node == NULL) return NAN;
    //※ NAN(Not a Number): 계산 불가능한 값
    //cf. NULL: 포인터가 가리키는 주소가 X, 0: 숫자가 정확하게 0

    // (1) 숫자 및 상수 (pi, inf)
    if (node->type == NODE_NUMBER) 
    {
        if (strcmp(node->value, "pi") == 0) return M_PI;
        if (strcmp(node->value, "inf") == 0 || strcmp(node->value, "infty") == 0) return INFINITY;
        return atof(node->value);
    }

    // (2) 변수 (x 또는 자연상수 e)
    if (node->type == NODE_VARIABLE) {
        if (strcmp(node->value, "e") == 0) return M_E;
        return x; // 독립변수 x 대입
    }

    // (3) 이항 연산자 (+, -, *, /, ^) 및 단항 음수 (-x)
    if (node->type == NODE_BINARY_OP) 
    {
        // 단항 마이너스 처리 (모듈 I에서 left == NULL 로 저장됨)
        if (node->left == NULL && strcmp(node->value, "-") == 0) {
            return -eval_ast(node->right, x);
        }

        double L = eval_ast(node->left, x);
        double R = eval_ast(node->right, x);

        if (strcmp(node->value, "+") == 0) return L + R;
        if (strcmp(node->value, "-") == 0) return L - R;
        if (strcmp(node->value, "*") == 0) return L * R;
        if (strcmp(node->value, "/") == 0) return (fabs(R) < 1e-15) ? NAN : (L / R);
        if (strcmp(node->value, "^") == 0) return pow(L, R);
        return NAN;
    }

    // (4) 단일 인자 수학 함수 (KEYWORD_TABLE 전체 대응)
    if (node->type == NODE_FUNCTION) 
    {
        const char* fn = node->value;
        if (strcmp(fn, "prime") == 0) {
            return num_diff_1st(node->left, x);
        }

        double v = eval_ast(node->left, x);
        if (isnan(v)) return NAN;

        if (strcmp(fn, "exp") == 0)  return exp(v);
        if (strcmp(fn, "ln") == 0)   return (v <= 0.0) ? NAN : log(v);
        if (strcmp(fn, "log") == 0)  return (v <= 0.0) ? NAN : log10(v); //밑이 0인 로그는 존재할 수 없음
        if (strcmp(fn, "abs") == 0)  return fabs(v);
        if (strcmp(fn, "sqrt") == 0) return (v < 0.0) ? NAN : sqrt(v); //알맹이가 음수인 루트는 실수에선 존재할 수 없음

        // 삼각함수 및 역수
        if (strcmp(fn, "sin") == 0)  return sin(v);
        if (strcmp(fn, "cos") == 0)  return cos(v);
        if (strcmp(fn, "tan") == 0)  return tan(v);
        if (strcmp(fn, "sec") == 0)  return 1.0 / cos(v);
        if (strcmp(fn, "csc") == 0)  return 1.0 / sin(v);
        if (strcmp(fn, "cot") == 0)  return 1.0 / tan(v);

        // 역삼각함수
        if (strcmp(fn, "asin") == 0) return asin(v);
        if (strcmp(fn, "acos") == 0) return acos(v);
        if (strcmp(fn, "atan") == 0) return atan(v);
        if (strcmp(fn, "asec") == 0) return acos(1.0 / v);
        if (strcmp(fn, "acsc") == 0) return asin(1.0 / v);
        if (strcmp(fn, "acot") == 0) return atan(1.0 / v);

        // 쌍곡선함수
        if (strcmp(fn, "sinh") == 0) return sinh(v);
        if (strcmp(fn, "cosh") == 0) return cosh(v);
        if (strcmp(fn, "tanh") == 0) return tanh(v);
        return NAN;
    }

    // (5) 다중 인자 특수 연산자 (frac, logbase, diff, int)
    if (node->type == NODE_SPECIAL_OP) 
    {
        const char* op = node->value;
        if (strcmp(op, "frac") == 0 && node->arg_count == 2) 
        {
            double num = eval_ast(node->args[0], x); //분자
            double den = eval_ast(node->args[1], x); //분모
            return (fabs(den) < 1e-15) ? NAN : (num / den); //분모가 1e-15보다 작으면 계산 불가
        }
        if (strcmp(op, "logbase") == 0 && node->arg_count == 2) 
        {
            double base = eval_ast(node->args[0], x); //밑
            double val = eval_ast(node->args[1], x); //진수
            if (base <= 0.0 || base == 1.0 || val <= 0.0) return NAN; //로그의 정의조건
            return log(val) / log(base);
        }
        if (strcmp(op, "diff") == 0 && node->arg_count == 3) 
        {
            int order = (int)eval_ast(node->args[2], x);
            if (order == 1) return num_diff_1st(node->args[0], x);
            if (order == 2) return num_diff_2nd(node->args[0], x);
        }
        if (strcmp(op, "int") == 0 && node->arg_count == 4) 
        {
            double a = eval_ast(node->args[0], x); //아래끝
            double b = eval_ast(node->args[1], x); //위끝
            return num_integrate(node->args[2], a, b, 1000);
        }
    }

    return NAN;
}

// 2. 수치 미분 (중앙 차분법: 오차 O(h^2))
double num_diff_1st(const ASTNode* root, double x) 
{
    double h = DIFF_STEP_H1;
    double f_plus = eval_ast(root, x + h);
    double f_minus = eval_ast(root, x - h);
    if (isnan(f_plus) || isnan(f_minus)) return NAN;
    return (f_plus - f_minus) / (2.0 * h);
}

double num_diff_2nd(const ASTNode* root, double x) 
{
    double h = 1e-4; // 2계 미분은 h^2으로 나누므로 부동소수점 손실 방지를 위해 1e-4 사용
    double f_plus = eval_ast(root, x + h);
    double f_curr = eval_ast(root, x);
    double f_minus = eval_ast(root, x - h);
    if (isnan(f_plus) || isnan(f_curr) || isnan(f_minus)) return NAN;
    return (f_plus - 2.0 * f_curr + f_minus) / (h * h);
}

// 3. 단일 구간 수치 적분 (합성 심프슨 1/3 법칙: 오차 O(dx^4))
// 구간을 n개로 쪼갠 다음 구간의 점에 인덱스 번호에 따라 [1,4,2,4,2,4,..,1]이라는 가중치를 곱해서 넓이를 계산
double num_integrate(const ASTNode* root, double a, double b, int steps) 
{
    if (isnan(a) || isnan(b) || root == NULL) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    // 심프슨 법칙은 구간 수(steps)가 반드시 짝수여야 함
    if (steps < 2) steps = 2; //2보다 작으면 2로 잡고
    if (steps % 2 != 0) steps++; //홀수면 1을 더해서 짝수화

    double dx = (b - a) / (double)steps; //쉽게말해 겁나게 작은 수
    double sum = eval_ast(root, a) + eval_ast(root, b); //전체 구간의 첫,끝점 (1이라는 가중치가 곱해짐)
    if (isnan(sum)) return NAN;

    for (int i = 1; i < steps; i++) 
    {
        double xi = a + i * dx;
        double yi = eval_ast(root, xi);
        if (isnan(yi)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * yi; 
        //인덱스 번호가 홀수 -> x2
        //인덱스 번호가 짝수 -> x4
    }

    return sum * (dx / 3.0);
}

// 4. [메인 커널] 2000개 격자 일괄 계산 및 배열 패킹
// Python이라는 주문처에서 갖고온 박스에 계산된 결과값(double 데이터)을 파파박 꽃아 넣어주는 공장
void sample_calculus_arrays(
    const ASTNode* root, //모듈I이 만든 수식 트리
    double x_min, //x축 시작점
    double x_max, //x축 끝점
    int count, //샘플링 개수
    double* out_x, //출력 버퍼 I: x좌표 arr
    double* out_y, // 버퍼 II: x에 대한 함숫값 배열
    double* out_dy, // 버퍼 III: 1계도함수 배열
    double* out_d2y, //버퍼 IV: 2계도함수 배열
    double* out_int_y) //버퍼 V: 누정 정적분 배열
{
    if (root == NULL || count < 2);

    double dx = (x_max - x_min) / (double)(count - 1);
    double running_area = 0.0;

    for (int i = 0; i < count; i++) 
    {
        double x = x_min + i * dx;
        double y = eval_ast(root, x);

        if (out_x)   out_x[i] = x;
        if (out_y)   out_y[i] = y;
        if (out_dy)  out_dy[i] = num_diff_1st(root, x);
        if (out_d2y) out_d2y[i] = num_diff_2nd(root, x);

        // 누적 정적분 F(x_i) = int_{x_min}^{x_i} f(t) dt (구간 내 심프슨 1스텝 누적)
        if (out_int_y) 
        {
            if (i == 0) {out_int_y[0] = 0.0;}
            else 
            {
                double x_prev = out_x ? out_x[i - 1] : (x - dx);
                double y_prev = out_y ? out_y[i - 1] : eval_ast(root, x_prev);
                double y_mid = eval_ast(root, 0.5 * (x_prev + x));

                if (!isnan(y_prev) && !isnan(y_mid) && !isnan(y)) {
                    running_area += (dx / 6.0) * (y_prev + 4.0 * y_mid + y);
                }
                out_int_y[i] = running_area;
            }
        }
    }
}

int main(void)
{
    char input[256];

    printf("=================================================================\n");
    printf(" CalcTech 모듈 I + 모듈 II 통합 테스트 (수식 파싱 & 수치 미적분)\n");
    printf(" (종료: 'exit' / 도움말: '/?')\n");
    printf("=================================================================\n");

    while (1)
    {
        printf("\n수식 입력 f(x) > ");

        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\r\n")] = '\0';

        if (strcmp(input, "exit") == 0) break;
        if (strlen(input) == 0) continue;
        if (strcmp(input, "/?") == 0) { print_help(); continue; }

        // [모듈 I - 1단계] Lexer: 문자열 -> 토큰 리스트
        TokenList tokens = tokenize(input);

        // [모듈 I - 2단계] Parser: 토큰 리스트 -> AST 트리
        Parser parser = { &tokens, 0 };
        ASTNode* root = parse_expression(&parser);

        if (root == NULL) {
            printf("[Error] 수식을 파싱할 수 없습니다.\n");
            free_token_list(&tokens);
            continue;
        }

        // [모듈 I - 3단계] LaTeX 변환 확인
        char latex_result[2048] = "";
        ast_to_latex(root, latex_result, sizeof(latex_result));
        printf("\n[파싱된 수식 (LaTeX)] : $$ %s $$\n", latex_result);

        // =====================================================================
        // [모듈 II] 2000개 격자 샘플링 커널 실행 (구간: [0.0, 2.0])
        // =====================================================================
        int N = DEFAULT_SAMPLE_COUNT; // 2000
        double x_min = 0.0;
        double x_max = 2.0;

        double* out_x = (double*)malloc(sizeof(double) * N);
        double* out_y = (double*)malloc(sizeof(double) * N);
        double* out_dy = (double*)malloc(sizeof(double) * N);
        double* out_d2y = (double*)malloc(sizeof(double) * N);
        double* out_int_y = (double*)malloc(sizeof(double) * N);

        // C 커널 공장에서 2000개 포인트 일괄 계산 후 배열에 적재
        sample_calculus_arrays(root, x_min, x_max, N, out_x, out_y, out_dy, out_d2y, out_int_y);

        // 주요 지점 샘플 출력 (2000개 중 10개 구간만 요약 출력)
        printf("\n[모듈 II 수치 미적분 검증 결과 (구간: %.1f ~ %.1f, N=%d)]\n", x_min, x_max, N);
        printf("-------------------------------------------------------------------------\n");
        printf("   Index  |     x     |   f(x)    |   f'(x)   |   f''(x)  | F(x)=∫f(t)dt\n");
        printf("-------------------------------------------------------------------------\n");

        int step = N / 10; // 200칸 간격으로 출력
        for (int i = 0; i < N; i += step) {
            printf("  [%4d]  | %9.4f | %9.4f | %9.4f | %9.4f | %11.4f\n",
                i, out_x[i], out_y[i], out_dy[i], out_d2y[i], out_int_y[i]);
        }
        // 마지막 끝점 (i = 1999, x = 2.0) 출력
        printf("  [%4d]  | %9.4f | %9.4f | %9.4f | %9.4f | %11.4f\n",
            N - 1, out_x[N - 1], out_y[N - 1], out_dy[N - 1], out_d2y[N - 1], out_int_y[N - 1]);
        printf("-------------------------------------------------------------------------\n");

        // 동적 할당 메모리 정리
        free(out_x);
        free(out_y);
        free(out_dy);
        free(out_d2y);
        free(out_int_y);

        free_ast(root);
        free_token_list(&tokens);
    }

    return 0;
}