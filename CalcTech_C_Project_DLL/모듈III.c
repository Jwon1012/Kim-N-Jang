#include "CalcTech공용두꺼비집.h"

#pragma region 은닉 함수 선언부
static int compare_doubles(const void* a, const void* b);
static bool get_valid_y_bounds(double* y_arr, int count, double x_range, double* out_low, double* out_high);
static int gcd_int(int a, int b);
static double eval_target_func(const ASTNode* f, const ASTNode* g, double x, int order);
static bool refine_root_hybrid(const ASTNode* f, const ASTNode* g, double a, double b, int order, double* out_root);
static void push_keypoint(GraphKeyPoint* list, int* count, int max_count, KeyPointType type, double x, double y);
static void check_direct_candidate(
    const ASTNode* f_root, const ASTNode* g_root, double x_cand, double x_min, 
    double x_max, GraphKeyPoint* out_points, int* count, int max_points);
static bool is_valid_period(const ASTNode* root, double p);
static double reduce_to_minimal_period(const ASTNode* root, double p);
static void push_asymptote_record(
    GAsymp* list, int* count, int max_count,
    ASType type, double val, int direction,
    bool is_confirmed, double residual, const char* );
static bool check_vertical_one_sided_divergence(const ASTNode* root, double x0, int dir);
static double refine_vertical_asymptote_x(const ASTNode* root, double a, double b);
static bool evaluate_residual_convergence_dir(
    const ASTNode* f_root, const ASTNode* cand_g_root,
    int dir, double* out_final_residual, bool* out_is_converged);
static bool detect_self_horizontal_dir(const ASTNode* f_root, int dir, double* out_y, double* out_step_diff);
#pragma endregion

// [모듈 III] C-Python 파이프라인 연결 준비

#pragma region 동적 축 스케일링 & 수칙 적응형 샘플링 커널
// 서브트리에 상수 'π '가 포함되어 있는지 확인 (예: sin(2*π *x) 판별용)
bool ast_contains_pi(const ASTNode* node) 
{
    if (node == NULL) return false;
    // 숫자/변수 노드(NODE_NUMBER/NODE_VARIABLE)이면서 이름표가 "π "인 경우
    if ( (node->type == NODE_NUMBER ||node->type == NODE_VARIABLE) && strcmp(node->value, "pi") == 0) return true;
    // 왼쪽 자식 또는 오른쪽 자식 서브트리 중 한 곳이라도 "π "가 있으면 true 반환
    return ast_contains_pi(node->left) || ast_contains_pi(node->right);
}

// 트리 조각 안에 변수 'x'가 하나라도 있는지 검사 (x*x과 x^2을 정확히 인식할 수 있게 하는 도우미함수)
static bool ast_contains_x(const ASTNode* node) {
    if (node == NULL) return false;
    if (node->type == NODE_VARIABLE && strcmp(node->value, "x") == 0) return true;
    for (int i = 0; i < node->arg_count; i++) {
        if (ast_contains_x(node->args[i])) return true;
    }
    return ast_contains_x(node->left) || ast_contains_x(node->right);
}

// 서브트리에 변수 'x' 외에 비선형 연산(exp, ln, 2차 이상 거듭제곱 등)이 있는지 확인
bool ast_is_nonlinear_arg(const ASTNode* node) {
    if (!node) return false;
    if (node->type == NODE_FUNCTION) return true; // sin(exp(x)), sin(ln(x)) 등

    // 1. 곱셈(*): 양쪽에 모두 x가 있으면 비선형 (예: x*x)
    if (node->type == NODE_BINARY_OP && strcmp(node->value, "*") == 0) {
        if (ast_contains_x(node->left) && ast_contains_x(node->right)) return true;
    }

    // 2. 나눗셈(/): 분모에 x가 있으면 비선형 (예: 1/x)
    if (node->type == NODE_BINARY_OP && strcmp(node->value, "/") == 0) {
        if (ast_contains_x(node->right)) return true;
    }

    // 3. 거듭제곱(^)
    if (node->type == NODE_BINARY_OP && strcmp(node->value, "^") == 0) {
        if (node->right != NULL && node->right->type == NODE_NUMBER) {
            double exp_val = atof(node->right->value);
            bool is_linear_exp = (fabs(exp_val - 1.0) <= 1e-9) || (fabs(exp_val - 0.0) <= 1e-9);
            if (!is_linear_exp) return true;
        }
        else return true;
    }
    return ast_is_nonlinear_arg(node->left) || ast_is_nonlinear_arg(node->right);
}

// AST 전체를 분석해 X축에 π 라디안 눈금을 쓸지(1) 일반 10진 눈금을 쓸지(0) 판별
  int analyze_ast_pi_mode(const ASTNode* node, bool inside_trig) {
    if (!node) return -1; // 아직 결정 안 됨

    // 1. 삼각함수(sin, cos, tan) 노드를 만난 경우
    if (node->type == NODE_FUNCTION &&
           (strcmp(node->value, "sin") == 0 ||
            strcmp(node->value, "cos") == 0 ||
            strcmp(node->value, "tan") == 0 ||
            strcmp(node->value, "sec") == 0 ||
            strcmp(node->value, "csc") == 0 ||
            strcmp(node->value, "cot") == 0)) {
        // 내부 인자가 비선형이면(sin(x^2), sin(exp(x))) 10진 눈금 확정 (어차피 수학적으로 얻어갈 부분이 전무)
        if (ast_is_nonlinear_arg(node->left)) return 0;

        // 선형 인자에 'π '가 이미 곱해져 있으면(tan(π *x), sin(2*π *x)) 주기가 유리수이므로 10진 눈금
        if (ast_contains_pi(node->left)) return 0;

        // 순수 삼각함수(sin(x), cos(2x))이므로 π 라디안 눈금 후보
        return 1;
    }

    // 2. 삼각함수 바깥에 변수 'x', 지수(exp), 로그(ln)가 섞여 있는 경우 (예: x - sin(x))
    if (!inside_trig) {
        // 비주기 다항 항 혼합 -> 10진 눈금
        if (node->type == NODE_VARIABLE && strcmp(node->value, "x") == 0) return 0;

        // 삼각함수 외의 다른 초월함수(exp, ln, √ 등)가 바깥에 존재
        if (node->type == NODE_FUNCTION) return 0;
    }
    int left_res = analyze_ast_pi_mode(node->left, inside_trig);
    int right_res = analyze_ast_pi_mode(node->right, inside_trig);

    // 어느 한쪽이라도 10진 강제(0) 판정이 나면 전체 식은 비주기 함수로 10진 눈금(0) 우선
    // 3개 이상의 항일지언정 어차피 and조건으로 묶여있는 한 모든 항이 같은 숫자여야함.
    if (left_res == 0 || right_res == 0) return 0;
    if (left_res == 1 || right_res == 1) return 1;
    return -1;
}

int decide_x_pi_mode(const ASTNode* root, int x_unit_mode) {
    if (x_unit_mode == X_UNIT_DECIMAL) return 0;
    if (x_unit_mode == X_UNIT_PI)      return 1;

    int auto_res = analyze_ast_pi_mode(root, false);
    return (auto_res == 1) ? 1 : 0; // 기본값은 10진 눈금(0)
}

// Nice-Step 눈금 간격 계산기 (10진 및 π 라디안)
double calc_nice_step(double range, int target_ticks) {
    range = fabs(range);
    if (range < 1e-12 || target_ticks <= 0) return 1.0;

    double raw_step = range / (double)target_ticks;
    double exponent = floor(log10(raw_step));
    double mag = pow(10.0, exponent);
    double fraction = raw_step / mag;

    //사람의 눈에는 1,2,5,10의 곱꼴로 된 단위가 눈에 잘 보임
    double nice_fraction;
    if (fraction < 1.5)      nice_fraction = 1.0;
    else if (fraction < 3.5) nice_fraction = 2.0;
    else if (fraction < 7.5) nice_fraction = 5.0;
    else                     nice_fraction = 10.0;

    return nice_fraction * mag;
}
double calc_pi_nice_step(double range, int target_ticks) {
    double pi_units = calc_nice_step(range / M_PI, target_ticks);
    // 1.0π 대신 π/2 단위가 자연스럽게 나오도록 보정
    if (fabs(pi_units - 1.0) < 1e-9) {
        pi_units = 0.5;
    }
    return pi_units * M_PI;
}

// 백분위 이상치 컷 함수 for 극단값 제거
// 1,0,-1반환 for 오름차순 정렬
static int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

// 점근선 근처에서 y값이 무한대로 폭발해도 그래프 본체가 납작해지지 않게
// 상·하위 극단값(이상치)을 잘라내고 유효한 y축 시야 범위만 건져내는 함수
static bool get_valid_y_bounds(double* y_arr, int count, double x_range, double* out_low, double* out_high) {
    if (!y_arr || count <= 0) return false;

    double* valid_vals = (double*)malloc(sizeof(double) * count);
    if (!valid_vals) return false;

    int valid_cnt = 0;
    double dx = x_range / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double y = y_arr[i];

        // 1. NAN이나 INF 제외
        if (isnan(y) || isinf(y)) continue;

        // 2. 수직 점근선 감지 및 마스킹 (tan(πx), 1/x 등)
        // 이전 점과 부호가 반대이면서 기울기 변화가 비정상적으로 폭발하는 경우
        if (i > 0 && !isnan(y_arr[i - 1]) && !isinf(y_arr[i - 1])) {
            double prev_y = y_arr[i - 1];
            double dy = fabs(y - prev_y);
            // 부호가 바뀌었고, 화면 x폭 대비 y 변화량이 극단적으로 크면 점근선으로 간주
            if ((y * prev_y < 0.0) && (dy / dx > 1000.0) && (fabs(y) > 50.0 || fabs(prev_y) > 50.0)) {
                y_arr[i] = NAN; // 그래프가 수직으로 찢어지지 않게 데이터 자체를 끊어줌!
                continue;
            }
        }

        valid_vals[valid_cnt++] = y;
    }

    if (valid_cnt == 0) {
        free(valid_vals);
        return false;
    }

    // 3. 이상치(Tail) 절단: 상·하위 3% 극단 꼬리(점근선 부근 솟구침 등) 제거
    qsort(valid_vals, valid_cnt, sizeof(double), compare_doubles);

    int low_idx = (int)(valid_cnt * 0.03);
    int high_idx = (int)(valid_cnt * 0.97);
    if (high_idx >= valid_cnt) high_idx = valid_cnt - 1;
    if (low_idx > high_idx)    low_idx = 0;

    *out_low = valid_vals[low_idx];
    *out_high = valid_vals[high_idx];

    free(valid_vals);
    return true;
}

// 동적 축 스케일링 메인 통합 함수 (AScale 산출)
AScale compute_dynamic_axis_scale(
    const ASTNode* f_root, const ASTNode* g_root,
    double x_min, double x_max,
    double* y_f, double* y_g, int sample_count,
    const ScaleOption* opt
) {
    AScale res;
    res.x_min = x_min;
    res.x_max = x_max;
    res.is_1to1_locked = (opt->scale_target == SCALE_LOCK_1TO1) ? 1 : 0; //1 -> 보정불필요 (체급이 비슷한 함수) 0 -> 보정 필요

    int ticks = (opt->target_ticks > 0) ? opt->target_ticks : 8; //
    double x_range = fabs(x_max - x_min);

    // 1. X축 π 라디안 모드 판별 및 X축 Nice-Step 계산
    int pi_f = decide_x_pi_mode(f_root, opt->x_unit_mode);
    int pi_g = (g_root != NULL && opt->scale_target != SCALE_FOLLOW_F)
        ? decide_x_pi_mode(g_root, opt->x_unit_mode) : 0;

    if (opt->scale_target == SCALE_FOLLOW_G && g_root != NULL) {
        res.resolved_pi_x = decide_x_pi_mode(g_root, opt->x_unit_mode);
    }
    else {
        // 하나라도 일반 10진 눈금이 필요하면 10진 우선 (단, 강제 모드 제외)
        res.resolved_pi_x = (opt->x_unit_mode == X_UNIT_PI) ? 1 : (pi_f && (g_root == NULL || pi_g));
    }

    res.x_tick_step = res.resolved_pi_x
        ? calc_pi_nice_step(x_range, ticks)
        : calc_nice_step(x_range, ticks);

    // 2. scale_target 옵션에 따른 Y축 유효 치역(y_low, y_high) 수집
    double f_low = -5.0, f_high = 5.0;
    double g_low = -5.0, g_high = 5.0;
    bool has_f = get_valid_y_bounds(y_f, sample_count, x_range, &f_low, &f_high);
    bool has_g = (g_root != NULL) ? get_valid_y_bounds(y_g, sample_count, x_range, &g_low, &g_high) : false;

    double y_low = -5.0, y_high = 5.0;

    if (opt->scale_target == SCALE_FOLLOW_G && has_g) {
        y_low = g_low; y_high = g_high;
    }
    else if (opt->scale_target == SCALE_FOLLOW_F && has_f) {
        y_low = f_low; y_high = f_high;
    }
    else if (has_f && has_g) {
        // SCALE_AUTO_UNION (기본값): 두 함수 유효 치역의 합집합
        y_low = (f_low < g_low) ? f_low : g_low; //더 작은 쪽을 값으로
        y_high = (f_high > g_high) ? f_high : g_high; //더 큰쪽을 값으로
    }
    else if (has_f) {
        y_low = f_low; y_high = f_high;
    }
    else if (has_g) {
        y_low = g_low; y_high = g_high;
    }

    // 상수 함수(y = 3 등)라서 폭이 0인 경우 방어
    if (fabs(y_high - y_low) < 1e-9) {
        y_low -= 2.0;
        y_high += 2.0;
    }

    // 3. 원점(y = 0) 강제 포함 (Axis Anchoring)
    double y_span = y_high - y_low;
    if (opt->include_origin_y) {
        if (y_low > 0.0 && y_low < y_span * 0.35) {
            y_low = 0.0;
        }
        else if (y_high < 0.0 && fabs(y_high) < y_span * 0.35) {
            y_high = 0.0;
        }
    }

    // 4. 특징점 경계 여백(15% Padding) 추가 및 Y축 Nice-Step 산출
    y_span = y_high - y_low;
    double pad = y_span * 0.15;
    res.y_min = y_low - pad;
    res.y_max = y_high + pad;
    res.y_tick_step = calc_nice_step(res.y_max - res.y_min, ticks);

    // 5. 1:1 기하 비율 잠금 모드(SCALE_LOCK_1TO1) 후처리
    if (res.is_1to1_locked) {
        double y_center = (res.y_min + res.y_max) * 0.5;
        res.y_min = y_center - (x_range * 0.5);
        res.y_max = y_center + (x_range * 0.5);
        res.y_tick_step = res.x_tick_step;
    }

    return res;
}

#pragma endregion

#pragma region 함수의 심화 정보 (극점, 교점, 극한값"만" 있는 점, 변곡점) & 소수점 근사치 복원
// 두 수의 최대공약수를 출력하는 함수 (for 기약분수화)
static int gcd_int(int a, int b) {
    a = abs(a);
    b = abs(b);
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return (a == 0) ? 1 : a;
}

// 수치 근사값을 기호 문자열(유리수 q<=100, 이차무리수 d<=100, π, e, ln)로 복원, 불가 시 %.6f
// e.g. 방정식 x^2 = 97의 근: x = ±√97, 2^x = x+2의 근: x = 2, -1.69009 
  void format_symbolic_or_decimal(double val, char* out_buf, size_t buf_size) {
    const double EPS = 1e-7;

    if (isnan(val) || isinf(val)) {
        snprintf(out_buf, buf_size, "NaN");
        return;}
    if (fabs(val) < EPS) {
        snprintf(out_buf, buf_size, "0");
        return;}

    // 1. 정수 판별
    double r_int = round(val);
    if (fabs(val - r_int) < EPS) {
        snprintf(out_buf, buf_size, "%.0f", r_int);
        return;}

    // 2. 유리수 p/q 판별 (분모 상한 q <= 100, 소수 첫째 자리 k/10 포함)
    for (int q = 2; q <= 100; q++) {
        double p_dbl = round(val * q);
        if (fabs(val - p_dbl / q) < EPS) {
            int p = (int)p_dbl;
            int g = gcd_int(p, q);
            p /= g;
            int rq = q / g;
            if (rq == 1) snprintf(out_buf, buf_size, "%d", p);
            else snprintf(out_buf, buf_size, "%d/%d", p, rq);
            return;}
    }

    // 3. π 유리수배 (p*π / q, 분모 상한 q <= 12)
    double pi_ratio = val / M_PI; 
    for (int q = 1; q <= 12; q++) {
        double p_dbl = round(pi_ratio * q);
    // [파이 분수 복원 판정] 입력값에서 π 를 나눈 비율(π _ratio)이 0이 아닌 기약분수(p/q)와
    // 오차(EPS) 이내로 일치하면 진짜 (p/q)*π  꼴로 확정 (예: 1.047197... -> π /3)
        if (fabs(p_dbl) > 0 && fabs(pi_ratio - p_dbl / q) < EPS) { 
            int p = (int)p_dbl;
            int g = gcd_int(p, q);
            p /= g;
            int rq = q / g; 
            if (rq == 1) {
                if (p == 1)       snprintf(out_buf, buf_size, "π");
                else if (p == -1) snprintf(out_buf, buf_size, "-π");
                else              snprintf(out_buf, buf_size, "%dπ", p);
            }
            else {
                if (p == 1)       snprintf(out_buf, buf_size, "π/%d", rq);
                else if (p == -1) snprintf(out_buf, buf_size, "-π/%d", rq);
                else              snprintf(out_buf, buf_size, "%dπ/%d", p, rq);
            }
            return;
        }
    }

    // 4. 단순 제곱근 ±√(d) 판별 (근호 상한 d <= 100)
    double sq = val * val;
    double r_sq = round(sq);
    if (r_sq >= 2.0 && r_sq <= 100.0 && fabs(sq - r_sq) < EPS) {
        int d = (int)r_sq;
        int root_d = (int)round(sqrt((double)d));
        if (root_d * root_d != d) {
            snprintf(out_buf, buf_size, "%s√(%d)", (val < 0 ? "-" : ""), d);
            return;
        }
    }

    // 5. 이차방정식 근의 공식 형태 (p ± √(d)) / q (삼차함수 극점, etc.)
    const double QUAD_EPS = 1e-9; // 우연한 초월수 일치 방지를 위해 정밀도 강화
    for (int q = 1; q <= 12; q++)
    {
        for (int d = 2; d <= 50; d++)
        {
            // 제곱인수 포함 수 제외 (예: 4, 8, 9, 12 -> 2*√(2) 등은 완전제곱인수 제거)
            bool has_sq_factor = false; //무죄 추정
            for (int k = 2; k * k <= d; k++) {
                if (d % (k * k) == 0) { has_sq_factor = true; break; }
            }
            if (has_sq_factor) continue;

            double sq_d = sqrt((double)d);

            // +√(d) 경우
            double p_plus = round(val * q - sq_d);
            if (fabs(val - (p_plus + sq_d) / q) < QUAD_EPS)
            {
                int p = (int)p_plus;
                if (p == 0)      snprintf(out_buf, buf_size, "√(%d)/%d", d, q);
                else if (q == 1) snprintf(out_buf, buf_size, "%d + √(%d)", p, d);
                else             snprintf(out_buf, buf_size, "(%d + √(%d))/%d", p, d, q);
                return;
            }

            // -√(d) 경우 (부호 '-' 주의)
            double p_minus = round(val * q + sq_d);
            if (fabs(val - (p_minus - sq_d) / q) < QUAD_EPS)
            {
                int p = (int)p_minus;
                if (p == 0)      snprintf(out_buf, buf_size, "-√(%d)/%d", d, q);
                else if (q == 1) snprintf(out_buf, buf_size, "%d - √(%d)", p, d);
                else             snprintf(out_buf, buf_size, "(%d - √(%d))/%d", p, d, q);
                return;
            }
        }
    }

    // 6. 자연상수 e 선형결합 (a*e + b) 및 자연로그 ±ln(n)
    for (int a = -5; a <= 5; a++) 
    {
        if (a == 0) continue;
        double b = round(val - a * M_E);
        if (fabs(b) <= 10.0 && fabs(val - (a * M_E + b)) < EPS) 
        {
            if (fabs(b) < EPS) 
            {
                if (a == 1) snprintf(out_buf, buf_size, "e");
                else if (a == -1) snprintf(out_buf, buf_size, "-e");
                else snprintf(out_buf, buf_size, "%de", a);
            }
            else {
                if (a == 1) snprintf(out_buf, buf_size, "e %c %.0f", (b > 0 ? '+' : '-'), fabs(b));
                else if (a == -1) snprintf(out_buf, buf_size, "-e %c %.0f", (b > 0 ? '+' : '-'), fabs(b));
                else snprintf(out_buf, buf_size, "%de %c %.0f", a, (b > 0 ? '+' : '-'), fabs(b));
            }
            return;
        }
    }
    for (int n = 2; n <= 20; n++) {
        double ln_n = log((double)n);
        if (fabs(val - ln_n) < EPS) { snprintf(out_buf, buf_size, "ln(%d)", n);  return; }
        if (fabs(val + ln_n) < EPS) { snprintf(out_buf, buf_size, "-ln(%d)", n); return; }
    }

    // 7. 복원 불가 초월근 -> 소수점 근사치 확정
    snprintf(out_buf, buf_size, "%.6f", val);
}

// 심화 특징점(절편, 극점, 변곡점, 교점, 극한값만 있는 점, 주기) 정밀 추출기

// 대상 함수 mode: 0 = f(x), 1 = f'(x), 2 = f''(x), 3 = f(x) - g(x)
static double eval_target_func(const ASTNode* f, const ASTNode* g, double x, int mode) {
  if (mode == 0) return eval_ast(f, x);
  if (mode == 1) return num_diff_1st(f, x);
  if (mode == 2) return num_diff_2nd(f, x);
  if (mode == 3) return eval_ast(f, x) - eval_ast(g, x);
  return NAN;
}

// [a, b] 구간 내 이분법 (사잇값 정리 알고리즘 ver.) + 뉴턴-랩슨 하이브리드 정밀 근 탐색
// true -> 구간 내 근 존재, false -> 존재X

/* ※이분법만 쓰기엔 x축을 스치지만 하는 함수를 못잡아내고(사잇값 정리를 써도 두 함수값의 곱이 양수가 나옴)
    뉴런-랩슨은 융통성이 없어서 같이 사용 */
static bool refine_root_hybrid(const ASTNode* f, const ASTNode* g, double a, double b, int mode, double* out_root) {
    double fa = eval_target_func(f, g, a, mode);
    double fb = eval_target_func(f, g, b, mode);
    if (isnan(fa) || isinf(fa) || isnan(fb) || isinf(fb)) return false;

    if (fabs(fa) < 1e-13) { *out_root = a; return true; }
    if (fabs(fb) < 1e-13) { *out_root = b; return true; }
    if (fa * fb > 0.0) return false; //이분법 구간 (스치는 경우는 일단 패스)

    double low = a, high = b;
    double x = 0.5 * (low + high);
    const double dh = 1e-7;

    for (int iter = 0; iter < 60; iter++) {
        double fx = eval_target_func(f, g, x, mode);
        if (isnan(fx) || isinf(fx)) return false; //불연속 지뢰 탈출
        if (fabs(fx) < 1e-13 || fabs(high - low) < 1e-14) { //조기종료 코드
            *out_root = x;
            return true;
        }

        if (fa * fx < 0.0) { high = x; fb = fx; }
        else { low = x;  fa = fx; }

        // 뉴턴 스텝 시도 (구간 이탈 시 이분법 폴백)
        double dfx = (eval_target_func(f, g, x + dh, mode) - eval_target_func(f, g, x - dh, mode)) / (2.0 * dh);
        double next_x = (fabs(dfx) > 1e-14) ? (x - fx / dfx) : 0.5 * (low + high);

        if (next_x <= low || next_x >= high) {
            next_x = 0.5 * (low + high);
        }
        x = next_x;
    }
    *out_root = x;
    return true;
}

// 특징점 중복 등록 방지 및 구조체 조립 도우미
static void push_keypoint(
    GraphKeyPoint* list, int* count, int max_count,
    KeyPointType type, double x, double y
) {
    if (*count >= max_count || isnan(x) || isinf(x) || isnan(y) || isinf(y)) return;

    // 0 근접 부동소수점 잔차(-0.0000000000001 등) 정리
    if (fabs(x) < 1e-12) x = 0.0;
    if (fabs(y) < 1e-12) y = 0.0;

    // 동일한 위치·타입 중복 방지
    for (int i = 0; i < *count; i++) {if (list[i].type == type && fabs(list[i].x - x) < 1e-7 && fabs(list[i].y - y) < 1e-7) return;}

    char x_str[32], y_str[32];
    format_symbolic_or_decimal(x, x_str, sizeof(x_str));
    format_symbolic_or_decimal(y, y_str, sizeof(y_str));

    list[*count].type = type;
    list[*count].x = x;
    list[*count].y = y;
    snprintf(list[*count].label, sizeof(list[*count].label), "(%s, %s)", x_str, y_str);
    (*count)++;
}

//사용자가 제공한 범위 내에 함수의 특징점이 있는지 확인하는 함수
static void check_direct_candidate(
    const ASTNode* f_root, const ASTNode* g_root,
    double x_cand, double x_min, double x_max,
    GraphKeyPoint* out_points, int* count, int max_points
) {
    if (x_cand < x_min || x_cand > x_max) return;

    // X절편 검증: f(x) == 0
    if (fabs(eval_target_func(f_root, NULL, x_cand, 0)) < 1e-11) {
        push_keypoint(out_points, count, max_points, KP_X_INTERCEPT, x_cand, 0.0);
    }

    // 극점 검증: f'(x) == 0 (2계 도함수로 극대/극소 판별)
    if (fabs(eval_target_func(f_root, NULL, x_cand, 1)) < 1e-11) {
        double y_ext = eval_ast(f_root, x_cand);
        double d2 = eval_target_func(f_root, NULL, x_cand, 2);
        if (!isnan(y_ext) && !isinf(y_ext) && !isnan(d2) && fabs(d2) > 1e-7) {
            KeyPointType ext_type = (d2 < 0.0) ? KP_LOCAL_MAX : KP_LOCAL_MIN;
            push_keypoint(out_points, count, max_points, ext_type, x_cand, y_ext);
        }
    }

    // 교점 검증: f(x) - g(x) == 0
    if (g_root && fabs(eval_target_func(f_root, g_root, x_cand, 3)) < 1e-11) {
        push_keypoint(out_points, count, max_points, KP_INTERSECTION, x_cand, eval_ast(f_root, x_cand));}
}

// 주기 존재여부 확인 함수
static bool is_valid_period(const ASTNode* f, double p) {
    if (!f || isnan(p) || isinf(p) || p <= 1e-6) return false;

    const double EPS = 1e-6;
    const double PHI = (sqrt(5) - 1.0) / 2.0; // 격자 겹침(앨리어싱) 방지용 황금비 스텝 (보다 정확히는 황금비의 역수)
    //위의 PHI없으면 sin(x)의 주기가 π로 오판 -> 어떤 유리수 격자와 엇박자이고 유리수로 근사시키기 가장 어려운 무리수다운 무리수 추가
    int valid_checks = 0;
    double first_val = NAN;
    bool is_constant = true;

    // 비균일 샘플 25개 지점에서 f(x) == f(x + p) 검증
    for (int i = 0; i < 25; i++) {
        double x = -3.7 + i * PHI * (1.0 + 0.15 * p); //시작점 + 간격 * 보폭배율
        double y1 = eval_ast(f, x);
        double y2 = eval_ast(f, x + p);

        // 점근선(tan x 등)이나 정의역 이탈 구간은 건너뜀
        if (isnan(y1) || isinf(y1) || isnan(y2) || isinf(y2)) continue;
        if (fabs(y1) > 1e6 || fabs(y2) > 1e6) continue;

        if (isnan(first_val)) {
            first_val = y1;
        }
        else if (fabs(y1 - first_val) > EPS) {
            is_constant = false; // 함숫값이 변하므로 상수함수가 아님
        }

        // 주기성 위배 시 즉시 탈락
        if (fabs(y1 - y2) > EPS) return false;
        valid_checks++;
    }

    // 유효 비교가 충분하고, 상수함수가 아닐 때만 진짜 주기 인정
    return (valid_checks >= 12 && !is_constant);
}

// [내부 도우미] 찾은 주기 P를 정수 k(12 -> 2)로 나누어 최소 주기(기본 주기)로 축소
// 계속 1씩 줄이다가 주기가 아니라고 뜨면 그 직전의 값을 주기로 간주
static double reduce_to_minimal_period(const ASTNode* f, double p) {
    for (int k = 12; k >= 2; k--) {
        double sub_p = p / (double)k;
        if (is_valid_period(f, sub_p)) {
            p = sub_p;
        }
    }
    return p;
}

// 메인 주기 추출 함수: 주기함수면 최소 양의 주기 p 반환 & out_label에 기호 문자열 저장, 비주기면 NAN 반환
double detect_function_period(const ASTNode* f_root, double search_max, char* out_label, size_t label_size) {
    if (out_label && label_size > 0) snprintf(out_label, label_size, "None");
    if (!f_root) return NAN;
    if (search_max <= 0.0) search_max = 20.0;

    double found_p = NAN;

    // 1단계: 교과서 핵심 주기 후보(π 유리수배 및 유리수) 우선 검증
    const double CANDIDATES[] = {
        M_PI / 6.0, M_PI / 4.0, M_PI / 3.0, M_PI / 2.0,
        (2.0 * M_PI) / 3.0, M_PI, 1.5 * M_PI, 2.0 * M_PI,
        3.0 * M_PI, 4.0 * M_PI, 6.0 * M_PI,
        0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0
    };
    int cand_cnt = (int)(sizeof(CANDIDATES) / sizeof(CANDIDATES[0]));

    for (int i = 0; i < cand_cnt; i++) {
        if (is_valid_period(f_root, CANDIDATES[i])) {
            found_p = reduce_to_minimal_period(f_root, CANDIDATES[i]);
            break;
        }
    }

// 1단계에서 못 찾았을 때만 2단계 저인망 탐색 진입
    if (isnan(found_p)) {
        // (특수각·π  배수)에서 이미 주기를 찾았으면 2단계로 내려갈 필요 없이 바로 퇴근
        double x0 = 0.37;
        double y0 = eval_ast(f_root, x0);
        // 2단계: sin(1.7x)처럼 파이와 무관한 생소한 계수를 가진 주기함수 추적용 저인망 탐색
        // 원점(0) 대칭 함정을 피하려고 비대칭 기준점(0.37)을 먼저 찔러보고, 막히면 1.13으로 우회
        if (isnan(y0) || isinf(y0) || fabs(y0) > 1e5) {
            x0 = 1.13;
            y0 = eval_ast(f_root, x0);
        }
        if (isnan(y0) || isinf(y0)) return NAN;

        const int STEPS = 1500;
        double dp = search_max / (double)STEPS;

        for (int i = 1; i < STEPS; i++) {
            double p1 = i * dp;
            double p2 = p1 + dp;
            double d1 = eval_ast(f_root, x0 + p1) - y0;
            double d2 = eval_ast(f_root, x0 + p2) - y0;

            if (isnan(d1) || isnan(d2) || fabs(d1) > 1e5 || fabs(d2) > 1e5) continue;
            if (d1 * d2 > 0.0) continue;

            double lo = p1, hi = p2;
            for (int iter = 0; iter < 40; iter++) {
                double mid = 0.5 * (lo + hi);
                double dm = eval_ast(f_root, x0 + mid) - y0;

                if (fabs(dm) < 1e-12) { lo = mid; hi = mid; break; }
                if (d1 * dm <= 0.0) hi = mid;
                else { lo = mid; d1 = dm; }
            }

            double cand_p = 0.5 * (lo + hi);
            if (is_valid_period(f_root, cand_p)) {
                found_p = reduce_to_minimal_period(f_root, cand_p);
                break; // return 대신 break로 빠져나와 3단계로 이동
            }
        }
    }
    // 3단계: 발견된 최소 주기를 기호 문자열(예: "2π ", "π /2", "2")로 변환
    if (!isnan(found_p) && out_label && label_size > 0) {
        format_symbolic_or_decimal(found_p, out_label, label_size);
    }
    return found_p;
}

// 메인 특징점 추출 진입 함수
int extract_graph_keypoints(
    const ASTNode* f_root, const ASTNode* g_root,
    double x_min, double x_max,
    GraphKeyPoint* out_points, int max_points
) {
    if (!f_root || !out_points || max_points <= 0 || x_max <= x_min) return 0; //!f_root는 f_root == NULL과 같은 뜻

    int count = 0;
    const int SCAN_STEPS = 2000;
    double dx = (x_max - x_min) / (double)SCAN_STEPS;

    // 1. Y절편 (x = 0 이 현재 뷰포트 안에 있을 때)
    if (x_min <= 0.0 && x_max >= 0.0) {
        double y0 = eval_ast(f_root, 0.0);
        if (!isnan(y0) && !isinf(y0)) {
            push_keypoint(out_points, &count, max_points, KP_Y_INTERCEPT, 0.0, y0);}
    }

    // 2A. 소수 첫째 자리(0.1 단위) 유리수 근 우선 검증 (초월방정식 2^x = x + 2 등의 유리수 해 보호)
    int k_start = (int)ceil(x_min * 10.0); 
    int k_end = (int)floor(x_max * 10.0); //e.g. 현재 화면이 [-0.5,0.5]면 k가 -50, -49, ..., 49, 50씩 대입
    for (int k = k_start; k <= k_end; k++) {
        double x_cand = k / 10.0;
        if (fabs(eval_target_func(f_root, NULL, x_cand, 0)) < 1e-11) {
            push_keypoint(out_points, &count, max_points, KP_X_INTERCEPT, x_cand, 0.0);
        }
        if (g_root && fabs(eval_target_func(f_root, g_root, x_cand, 3)) < 1e-11) {
            push_keypoint(out_points, &count, max_points, KP_INTERSECTION, x_cand, eval_ast(f_root, x_cand));
        }
    }

    // 2B. 단순 무리수(문자 1종류 + 숫자 최대 2개 기준 핵심 상수) 직접 대입 검증
    const double SIMPLE_IRRATIONALS[] = {
        M_PI / 6.0, M_PI / 4.0, M_PI / 3.0, M_PI / 2.0, M_PI, 2.0 * M_PI,
        M_E, M_E - 1.0, M_E + 1.0, 1.0 / M_E,
        sqrt(2.0), sqrt(3.0), sqrt(5.0), sqrt(2.0) / 2.0, sqrt(3.0) / 2.0
    };
    const int NUM_SIMPLE = (int)(sizeof(SIMPLE_IRRATIONALS) / sizeof(SIMPLE_IRRATIONALS[0]));
    for (int i = 0; i < NUM_SIMPLE; i++) {
        check_direct_candidate(f_root, g_root, SIMPLE_IRRATIONALS[i], x_min, x_max, out_points, &count, max_points);
        check_direct_candidate(f_root, g_root, -SIMPLE_IRRATIONALS[i], x_min, x_max, out_points, &count, max_points);
    }

    // 3. 구간 훑기(Bracketing) -> X절편, 극점, 변곡점, 교점, 극한값만 있는 점(Hole) 탐색
    for (int i = 0; i < SCAN_STEPS; i++) {
        double a = x_min + i * dx;
        double b = a + dx;
        double root = 0.0;

        // (a) X절편: f(x) = 0
        if (refine_root_hybrid(f_root, NULL, a, b, 0, &root)) {
            double y_chk = eval_ast(f_root, root);
            if (!isnan(y_chk) && fabs(y_chk) < 1e-6) { // 점근선 부호 반전 오탐 차단
                push_keypoint(out_points, &count, max_points, KP_X_INTERCEPT, root, 0.0);
            }
        }

        // (b) 극점 (극대/극소): f'(x) = 0 및 2계 도함수 부호 판별
        if (refine_root_hybrid(f_root, NULL, a, b, 1, &root)) {
            // 0.1 단위 주요 격자점(0, 1 등)에 극도로 근접한 부동소수점 오차 보정
            double snap = round(root * 10.0) / 10.0;
            if (fabs(root - snap) < 1e-6) root = snap;

            double d2 = eval_target_func(f_root, NULL, root, 2);
            bool is_hole = false;
            double y_ext = eval_with_singularity_guard(f_root, root, &is_hole);

            if (!is_hole && !isnan(y_ext) && !isinf(y_ext) && !isnan(d2) && fabs(d2) > 1e-7) {
                KeyPointType ext_type = (d2 < 0.0) ? KP_LOCAL_MAX : KP_LOCAL_MIN;
                push_keypoint(out_points, &count, max_points, ext_type, root, y_ext);
                if (fabs(y_ext) < 1e-9) {
                    push_keypoint(out_points, &count, max_points, KP_X_INTERCEPT, root, 0.0);
                }
            }
        }

        // (c) 변곡점: f''(x) = 0 (전후 2계 도함수 부호 반전 + 노이즈 임계값 확인)
        if (refine_root_hybrid(f_root, NULL, a, b, 2, &root)) {
            double snap = round(root * 10.0) / 10.0;
            if (fabs(root - snap) < 1e-6) root = snap;

            bool is_hole = false;
            double y_inf = eval_with_singularity_guard(f_root, root, &is_hole);
            double d2_left = eval_target_func(f_root, NULL, root - 1e-3, 2);
            double d2_right = eval_target_func(f_root, NULL, root + 1e-3, 2);

            if (!is_hole && !isnan(y_inf) && !isinf(y_inf) &&
                fabs(d2_left) > 1e-6 && fabs(d2_right) > 1e-6 &&
                (d2_left * d2_right < 0.0)) {
                push_keypoint(out_points, &count, max_points, KP_INFLECTION, root, y_inf);
            }
        }
        // (d) 두 함수의 교점: f(x) - g(x) = 0
        if (g_root && refine_root_hybrid(f_root, g_root, a, b, 3, &root)) {
            double y_int = eval_ast(f_root, root);
            double g_int = eval_ast(g_root, root);
            if (!isnan(y_int) && !isnan(g_int) && fabs(y_int - g_int) < 1e-6) {
                push_keypoint(out_points, &count, max_points, KP_INTERSECTION, root, y_int);
            }
        }

        // (e) 극한값"만" 있는 점 (Removable Discontinuity / Hole) 탐색
        // 0.1 간격 격자점 또는 불연속 의심 지점에서 f(c)는 NaN이지만 좌·우극한이 일치하는 경우
        double mid = 0.5 * (a + b);
        double cand_hole = round(mid * 10.0) / 10.0; // 주요 유리수 격자(정수, 0.1 단위) 우선 검사
        if (cand_hole >= a && cand_hole <= b)                      
        {
            double f_at_c = eval_ast(f_root, cand_hole);
            if (isnan(f_at_c) || isinf(f_at_c)) 
            {
                double lim_l = eval_ast(f_root, cand_hole - 1e-7);
                double lim_r = eval_ast(f_root, cand_hole + 1e-7);
                if (!isnan(lim_l) && !isinf(lim_l) && !isnan(lim_r) && !isinf(lim_r) &&
                    fabs(lim_l - lim_r) < 1e-5) {
                    push_keypoint(out_points, &count, max_points, KP_HOLE, cand_hole, 0.5 * (lim_l + lim_r));
                }
            }
        }
    }

    return count;
}

#pragma endregion

#pragma region 기존 모듈 개발시 나온 문제점 보완

// [해결책 2] 특이점 구간(ln(x) 정의역 경계, sin(x)/x 제거 가능 특이점) 정밀 평가 헬퍼
double eval_with_singularity_guard(const ASTNode* f, double x, bool* out_is_hole) {
    if (out_is_hole) *out_is_hole = false;
    double y = eval_ast(f, x);

    // 0/0 꼴 제거 가능 특이점(예: sin(x)/x 의 x=0) -> 좌·우 극한이 일치하면 극한값으로 보간
    if (isnan(y)) {
        double lim_l = eval_ast(f, x - 1e-7);
        double lim_r = eval_ast(f, x + 1e-7);
        if (!isnan(lim_l) && !isinf(lim_l) && !isnan(lim_r) && !isinf(lim_r) &&
            fabs(lim_l - lim_r) < 1e-5) {
            if (out_is_hole) *out_is_hole = true;
            return 0.5 * (lim_l + lim_r);
        }
    }
    return y;
}

// [해결책 1 + 2] 고해상도(canvas_width_px) 연동 동적 샘플링 & 정의역 경계 로그 세분화
// 반환값: 실제 out_x, out_y 버퍼에 채워진 유효 샘플 점 개수
int sample_curve_adaptive(
    const ASTNode* f_root,
    double x_min, double x_max,
    int canvas_width_px,
    double* out_x, double* out_y, int max_samples
) {
    if (!f_root || !out_x || !out_y || max_samples <= 0 || x_max <= x_min) return 0;

    // 1. 해상도 연동 샘플 개수 결정 (픽셀당 최소 2샘플 보장, 최소 하한 2000개)
    int base_samples = (canvas_width_px * 2 > 2000) ? (canvas_width_px * 2) : 2000;
    if (base_samples > max_samples - 64) {
        base_samples = max_samples - 64; // 특이점 로그 세분화 여유 슬롯 64칸 확보
    }
    if (base_samples < 2) return 0;

    int count = 0;
    double dx = (x_max - x_min) / (double)(base_samples - 1);
    double prev_x = x_min;
    double prev_y = eval_with_singularity_guard(f_root, prev_x, NULL);

    for (int i = 0; i < base_samples && count < max_samples; i++) {
        double curr_x = x_min + i * dx;
        double curr_y = eval_with_singularity_guard(f_root, curr_x, NULL);

        // 2. 정의역 경계(예: ln(x)의 x -> 0+) 감지 시 로그 스케일(10^-8 ~ 10^-1) 미세 점 추가
        if (i > 0 && isnan(prev_y) != isnan(curr_y)) {
            double valid_x = isnan(prev_y) ? curr_x : prev_x;
            double invalid_x = isnan(prev_y) ? prev_x : curr_x;
            double dir = (valid_x > invalid_x) ? 1.0 : -1.0;

            // 경계선 안쪽으로 10^-8 간격까지 8단계 로그 보강 삽입
            const double LOG_OFFSETS[] = { 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 5e-2 };
            for (int k = 0; k < 8 && count < max_samples; k++)
            {
                double probe_x = invalid_x + dir * LOG_OFFSETS[k];
                if ((dir > 0 && probe_x < valid_x) || (dir < 0 && probe_x > valid_x))
                {
                    double probe_y = eval_ast(f_root, probe_x);
                    if (!isnan(probe_y) && !isinf(probe_y)) { out_x[count] = probe_x; out_y[count] = probe_y; count++; }
                }
            }
        }

        out_x[count] = curr_x;
        out_y[count] = curr_y;
        count++;

        prev_x = curr_x;
        prev_y = curr_y;
    }

    return count;
}

#pragma endregion

 #pragma region 선발된 점근선의 방정식 최종 검증 및 전달

// ※점근선의 수학적 정의: 곡선 위의 한 점이 정해진 직선에 한없이 가까워질때 두 대상 사이의 거리가 0에 수렴하는 기준 직선

/* [점근선 추출 및 검증 파이프라인]
  0. 기존에는 모듈3에서만 구현하려 했으나 y = g(x)꼴의 점근선 추출 로직을 구현할때
  예외처리를 아주 까다롭게 해야해서 
  다른 모듈에서 갖고온 점근선 후보를 테스트 해보는 식으로 변경

  1. [모듈 5: Python 심볼릭 1차 후보 선출]
     - 분수함수의 분모가 0이 되는 지점(수직 후보 x = a) 및
       다항식 나눗셈·극한 연산을 통한 비수직 점근선 후보식(y = g(x)) 1차 추출
 
  2. [모듈 4: C 커널 후보식 정밀 검증]
     - 비수직 후보(y = g(x)): |x| -> ∞ 구간에서 잔차 r(x) = |f(x) - g(x)| -> 0 수렴 검증
     - 수직 후보(x = a): 단측(좌·우) 로그 스케일 발산 검증으로 제거 가능 특이점(Hole) 필터링
 
  3. [모듈 4: C 커널 자체 추가 탐색 (초월함수 보완)]
     - 파이썬이 놓친 초월함수(ln, tan, arctan 등)를 위해 C에서 직접
       화면 내 수직 경계 스캔 및 원거리 코시 수렴 판정으로 수평/수직 후보 추가 발굴
 
  4. [모듈 2·3: 경계 정밀화 및 통합 패키징]
     - 이분법(이진 탐색)으로 불연속 경계 좌표 정밀화 및 기호 문자열("x = π /2" 등) 복원
     - 확정된 점근선([빵틀 25] GraphAsymptote)을 모듈 2(특이적분 구간 분할) 및
       모듈 3(파이썬 UI 렌더링 패킷)으로 최종 전달
 */

// 내부 도우미: 중복 점근선 병합 및 방정식 문자열 조립
static void push_asymptote_record(
    GAsymp* list, int* count, int max_count,
    ASType type, double val, int direction,
    bool is_confirmed, double residual, const char* custom_eq
) {
    if (*count >= max_count) return;
    if ((type == ASYMP_VERTICAL || type == ASYMP_HORIZONTAL) && (isnan(val) || isinf(val))) return;

    if (!isnan(val) && fabs(val) < 1e-9) val = 0.0;

    char eq_buf[96];
    if (custom_eq != NULL && custom_eq[0] != '\0') {
        snprintf(eq_buf, sizeof(eq_buf), "%s", custom_eq);
    }
    else {
        char val_str[64];
        format_symbolic_or_decimal(val, val_str, sizeof(val_str));
        if (type == ASYMP_VERTICAL) {
            snprintf(eq_buf, sizeof(eq_buf), "x = %s", val_str);
        }
        else {
            snprintf(eq_buf, sizeof(eq_buf), "y = %s", val_str);
        }
    }

    // 동일 방정식 중복 등록 방지 (방향성 및 확정 상태 병합)
    for (int i = 0; i < *count; i++) {
        bool same_numeric = (!isnan(val) && !isnan(list[i].value) &&
            list[i].type == type && fabs(list[i].value - val) < 1e-5);
        bool same_string = (list[i].type == type && strcmp(list[i].equation, eq_buf) == 0);

        if (same_numeric || same_string) {
            if (list[i].direction != direction) {
                list[i].direction = 0; // 좌·우 또는 ±∞ 양방향 통합
            }
            if (is_confirmed) {
                list[i].is_confirmed = true;
            }
            if (!isnan(residual) && (isnan(list[i].final_residual) || residual < list[i].final_residual)) {
                list[i].final_residual = residual;
            }
            return;
        }
    }

    list[*count].type = type;
    list[*count].value = val;
    list[*count].direction = direction;
    list[*count].is_confirmed = is_confirmed;
    list[*count].final_residual = residual;
    snprintf(list[*count].equation, sizeof(list[*count].equation), "%s", eq_buf);
    (*count)++;
}

// [파트 A] 수직 점근선(x = a) 독립 추출기
// 단측(dir: -1 좌극한, +1 우극한) 로그 스케일 발산 검증
// y = √(x)의 정의역 끝점 및 y = sin(x)/x의 제거 가능 특이점(Hole) 필터링
static bool check_vertical_one_sided_divergence(const ASTNode* f, double c, int dir) {
    const double DELTAS[] = { 1e-2, 1e-4, 1e-6, 1e-8 };
    double mags[4];

    for (int i = 0; i < 4; i++) {
        double probe_x = c + (double)dir * DELTAS[i];
        double y = eval_ast(f, probe_x);
        if (isnan(y)) return false;
        if (isinf(y)) return true;
        mags[i] = fabs(y);
    }

    bool strictly_growing = (mags[0] < mags[1]) && (mags[1] < mags[2]) && (mags[2] < mags[3]);
    bool reaches_infinity = (mags[3] > 15.0) && (mags[3] > mags[0] * 2.5);
    return strictly_growing && reaches_infinity;
}

// 유효 영역(valid_x)과 NaN/발산 영역(invalid_x) 사이 경계 정밀 이분 탐색
static double refine_vertical_asymptote_x(const ASTNode* f, double valid_x, double invalid_x) {
    double lo = valid_x;
    double hi = invalid_x;

    for (int iter = 0; iter < 50; iter++) {
        double mid = 0.5 * (lo + hi);
        double y_mid = eval_ast(f, mid);

        if (isnan(y_mid) || isinf(y_mid) || fabs(y_mid) > 1e8) {
            hi = mid;
        }
        else {
            lo = mid;
        }
    }

    double candidate = 0.5 * (lo + hi);
    double snap_01 = round(candidate * 10.0) / 10.0;
    if (fabs(candidate - snap_01) < 1e-6) {
        return snap_01;
    }
    return candidate;
}

// 독립 API 1: [x_min, x_max] 내 수직 점근선 추출
int extract_vertical_asymptotes(
    const ASTNode* f_root,
    double x_min, double x_max,
    int canvas_width_px,
    GAsymp* out_list, int max_count
) {
    if (!f_root || !out_list || max_count <= 0 || x_max <= x_min) return 0;

    int count = 0;
    int steps = (canvas_width_px * 2 > 2000) ? (canvas_width_px * 2) : 2000;
    double dx = (x_max - x_min) / (double)steps;

    double prev_x = x_min;
    double prev_y = eval_ast(f_root, prev_x);

    for (int i = 1; i <= steps && count < max_count; i++) {
        double curr_x = x_min + i * dx;
        double curr_y = eval_ast(f_root, curr_x);

        bool prev_valid = (!isnan(prev_y) && !isinf(prev_y));
        bool curr_valid = (!isnan(curr_y) && !isinf(curr_y));

        double cand_x = NAN;
        bool has_candidate = false;

        // 1) 유효 <-> NaN/inf 상태 전환 경계
        if (prev_valid != curr_valid) {
            cand_x = prev_valid
                ? refine_vertical_asymptote_x(f_root, prev_x, curr_x)
                : refine_vertical_asymptote_x(f_root, curr_x, prev_x);
            has_candidate = true;
        }
        // 2) 부호 반전 + 급격한 기울기 폭발 (1/x 형태)
        else if (prev_valid && curr_valid) {
            double dy = fabs(curr_y - prev_y);
            if ((curr_y * prev_y < 0.0) && (dy / dx > 1000.0) &&
                (fabs(curr_y) > 50.0 || fabs(prev_y) > 50.0)) {
                cand_x = 0.5 * (prev_x + curr_x);
                has_candidate = true;
            }
        }

        if (has_candidate && !isnan(cand_x)) {
            bool div_l = check_vertical_one_sided_divergence(f_root, cand_x, -1);
            bool div_r = check_vertical_one_sided_divergence(f_root, cand_x, +1);

            if (div_l || div_r) {
                int dir = (div_l && div_r) ? 0 : (div_l ? -1 : +1);
                push_asymptote_record(
                    out_list, &count, max_count,
                    ASYMP_VERTICAL, cand_x, dir, true, NAN, NULL
                );
            }
        }

        prev_x = curr_x;
        prev_y = curr_y;
    }

    return count;
}

// [파트 B] 비수직 점근선(수평·사선·곡선) 잔차 r(x) = |f(x) - g(x)| 검증기

// 한쪽 무한대 방향(dir: +1 또는 -1)에서 후보 식 cand_g_root에 대한 잔차 r(x) 수렴 검사
static bool evaluate_residual_convergence_dir(
    const ASTNode* f_root,
    const ASTNode* cand_g_root,
    int dir,
    double* out_final_residual,
    bool* out_is_converged
) {
    // 지수함수 오버플로우 방지를 위한 중간 스케일 + 원거리 스케일 탐사점
    const double PROBE_X[] = { 1e2, 1e3, 1e5, 1e7, 1e9 };
    const int NUM_PROBES = 5;
    double res[5];
    int valid_probes = 0;

    for (int i = 0; i < NUM_PROBES; i++) {
        double x = (double)dir * PROBE_X[i];
        double fx = eval_ast(f_root, x);
        double gx = eval_ast(cand_g_root, x);

        if (isnan(fx) || isinf(fx) || isnan(gx) || isinf(gx)) {
            break; // 정의역 이탈 또는 부동소수점 오버플로우 시 이전 유효 단계까지만 평가
        }
        res[valid_probes++] = fabs(fx - gx);
    }

    if (valid_probes < 3) {
        if (out_final_residual) *out_final_residual = NAN;
        if (out_is_converged) *out_is_converged = false;
        return false;
    }

    double last_r = res[valid_probes - 1];
    double prev_r = res[valid_probes - 2];
    double early_r = res[0];

    if (out_final_residual) *out_final_residual = last_r;

    // 잔차가 초반 대비 지속적으로 줄어들며 0으로 수렴하는지 판정
    bool shrinking = (last_r <= prev_r + 1e-12) && (last_r < early_r * 0.1 + 1e-9);
    bool within_tol = (last_r < 1e-5);

    if (out_is_converged) {
        *out_is_converged = (shrinking && within_tol);
    }
    return true;
}

// 독립 API 2: 파이썬/모듈4가 넘긴 후보 식(cand_g_root)을 잔차 r(x)로 검증
bool verify_candidate_asymptote_expr(
    const ASTNode* f_root,
    const ASTNode* cand_g_root,
    ASType cand_type,
    const char* cand_eq_str,
    GAsymp* out_record
) {
    if (!f_root || !cand_g_root || !out_record) return false;

    double res_pos = NAN, res_neg = NAN;
    bool conv_pos = false, conv_neg = false;

    bool has_pos = evaluate_residual_convergence_dir(f_root, cand_g_root, +1, &res_pos, &conv_pos);
    bool has_neg = evaluate_residual_convergence_dir(f_root, cand_g_root, -1, &res_neg, &conv_neg);

    if (!has_pos && !has_neg) return false;

    int dir = 0;
    double best_res = NAN;
    bool confirmed = (conv_pos || conv_neg);

    if (conv_pos && conv_neg) {
        dir = 0;
        best_res = (res_pos < res_neg) ? res_pos : res_neg;
    }
    else if (conv_pos) {
        dir = +1;
        best_res = res_pos;
    }
    else if (conv_neg) {
        dir = -1;
        best_res = res_neg;
    }
    else {
        // 수렴 확정은 안 되었더라도 계산상 측정된 최소 잔차 리포트 반환
        dir = (has_pos && (!has_neg || res_pos <= res_neg)) ? +1 : -1;
        best_res = (dir == +1) ? res_pos : res_neg;
    }

    // 상수 함수(수평 점근선)일 경우 값 추출
    double const_val = (cand_type == ASYMP_HORIZONTAL) ? eval_ast(cand_g_root, 0.0) : NAN;

    out_record->type = cand_type;
    out_record->value = const_val;
    out_record->direction = dir;
    out_record->is_confirmed = confirmed;
    out_record->final_residual = best_res;

    if (cand_eq_str && cand_eq_str[0] != '\0') {
        snprintf(out_record->equation, sizeof(out_record->equation), "%s", cand_eq_str);
    }
    else if (!isnan(const_val)) {
        char val_str[64];
        format_symbolic_or_decimal(const_val, val_str, sizeof(val_str));
        snprintf(out_record->equation, sizeof(out_record->equation), "y = %s", val_str);
    }
    else {
        snprintf(out_record->equation, sizeof(out_record->equation), "y = g(x)");
    }

    return true;
}

// C 자체 보조 탐색: 원거리 코시 수렴 판정으로 자체 수평 점근선 후보 포착
static bool detect_self_horizontal_dir(const ASTNode* f_root, int dir, double* out_y, double* out_step_diff) {
    const double PROBE_X[] = { 1e2, 1e3, 1e5, 1e7, 1e9 };
    double y[5];
    int n = 0;

    for (int i = 0; i < 5; i++) {
        double v = eval_ast(f_root, (double)dir * PROBE_X[i]);
        if (isnan(v) || isinf(v)) break;
        y[n++] = v;
    }
    if (n < 4) return false;

    double d1 = fabs(y[n - 3] - y[n - 4]);
    double d2 = fabs(y[n - 2] - y[n - 3]);
    double d3 = fabs(y[n - 1] - y[n - 2]);

    if (d3 < d2 && d2 <= d1 + 1e-12 && d3 < 1e-6) {
        *out_y = y[n - 1];
        *out_step_diff = d3;
        return true;
    }
    return false;
}

//[파트 C] 파이썬 전달용 통합 진입 함수 (독립 실행 메인 API)
int extract_asymptote_equations(
    const ASTNode* f_root,
    double x_min, double x_max,
    int canvas_width_px,
    const ASTNode* const* cand_g_roots, // 파이썬/모듈4 제안 후보 AST 배열 (없으면 NULL)
    const ASType* cand_types,    // 각 후보의 점근선 타입 배열 (없으면 NULL)
    const char* const* cand_eq_strs,    // 각 후보의 문자열 배열 (없으면 NULL)
    int num_candidates,                 // 후보 개수 (0 가능)
    GAsymp* out_asymptotes,
    int max_asymptotes
) {
    if (!f_root || !out_asymptotes || max_asymptotes <= 0) return 0;

    // 1. 현재 화면 구간의 수직 점근선 추출
    int total = extract_vertical_asymptotes(
        f_root, x_min, x_max, canvas_width_px, out_asymptotes, max_asymptotes
    );

    // 2. 파이썬/모듈4가 넘긴 후보 기준식들(수평·사선·곡선) 잔차 검증
    if (cand_g_roots != NULL && num_candidates > 0) {
        for (int i = 0; i < num_candidates && total < max_asymptotes; i++) {
            if (!cand_g_roots[i]) continue;
            ASType c_type = (cand_types != NULL) ? cand_types[i] : ASYMP_CURVILINEAR;
            const char* c_str = (cand_eq_strs != NULL) ? cand_eq_strs[i] : NULL;

            GAsymp verified;
            if (verify_candidate_asymptote_expr(f_root, cand_g_roots[i], c_type, c_str, &verified)) {
                push_asymptote_record(
                    out_asymptotes, &total, max_asymptotes,
                    verified.type, verified.value, verified.direction,
                    verified.is_confirmed, verified.final_residual, verified.equation
                );
            }
        }
    }

    // 3. C 커널 자체 원거리 수렴 탐색으로 수평 점근선 보완
    double y_pos = NAN, diff_pos = NAN;
    if (detect_self_horizontal_dir(f_root, +1, &y_pos, &diff_pos)) {
        push_asymptote_record(
            out_asymptotes, &total, max_asymptotes,
            ASYMP_HORIZONTAL, y_pos, +1, true, diff_pos, NULL
        );
    }

    double y_neg = NAN, diff_neg = NAN;
    if (detect_self_horizontal_dir(f_root, -1, &y_neg, &diff_neg)) {
        push_asymptote_record(
            out_asymptotes, &total, max_asymptotes,
            ASYMP_HORIZONTAL, y_neg, -1, true, diff_neg, NULL
        );
    }

    return total;
}

#pragma endregion
