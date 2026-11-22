#include "CalcTech공용두꺼비집.h"

// [모듈 III] C-Python 파이프라인 연결 준비

//1. 동적 축 스케일링 & 수칙 적응형 샘플링 커널
// 서브트리에 상수 'pi'가 포함되어 있는지 확인 (예: sin(2*pi*x) 판별용)
static bool ast_contains_pi(const ASTNode* node) 
{
    if (node == NULL) return false;
    // 변수 노드(NODE_VARIABLE)이면서 이름표가 "pi"인 경우
    if (node->type == NODE_VARIABLE && strcmp(node->value, "pi") == 0) return true;
    // 왼쪽 자식 또는 오른쪽 자식 서브트리 중 한 곳이라도 "pi"가 있으면 true 반환
    return ast_contains_pi(node->left) || ast_contains_pi(node->right);
}

// 서브트리에 변수 'x' 외에 비선형 연산(exp, ln, 2차 이상 거듭제곱 등)이 있는지 확인
static bool ast_is_nonlinear_arg(const ASTNode* node) {
    if (!node) return false;
    if (node->type == NODE_FUNCTION) return true; // sin(exp(x)), sin(ln(x)) 등
    if (node->type == NODE_BINARY_OP && strcmp(node->value, "^") == 0) 
    {
        // 지수가 0,1이 아닌 거듭제곱(x^2 등)이 들어있으면 비선형 인자로 간주
        double exp_val = atof(node->right->value);
        bool is_linear_exp = (fabs(exp_val - 1.0) <= 1e-9) || (fabs(exp_val - 0.0) <= 1e-9); //지수가 0이거나 1인 경우
        if (!is_linear_exp) {
            return true;
        }
    }
    return ast_is_nonlinear_arg(node->left) || ast_is_nonlinear_arg(node->right);
}

// AST 전체를 분석해 X축에 π 라디안 눈금을 쓸지(1) 일반 10진 눈금을 쓸지(0) 판별
static int analyze_ast_pi_mode(const ASTNode* node, bool inside_trig) {
    if (!node) return -1; // 아직 결정 안 됨

    // 1. 삼각함수(sin, cos, tan) 노드를 만난 경우
    if (node->type == NODE_FUNCTION &&
        (strcmp(node->value, "sin") == 0 ||
            strcmp(node->value, "cos") == 0 ||
            strcmp(node->value, "tan") == 0) ||
            strcmp(node->value, "sec") == 0 ||
            strcmp(node->value, "csc") == 0 ||
            strcmp(node->value, "cot") == 0)
    {

        // 2순위: 내부 인자가 비선형이면(sin(x^2), sin(exp(x))) 10진 눈금 확정 (어차피 수학적으로 얻어갈 부분이 전무)
        if (ast_is_nonlinear_arg(node->left)) return 0;

        // 3순위: 선형 인자에 'pi'가 이미 곱해져 있으면(tan(pi*x), sin(2*pi*x)) 주기가 유리수이므로 10진 눈금
        if (ast_contains_pi(node->left)) return 0;

        // 순수 삼각함수(sin(x), cos(2x))이므로 π 라디안 눈금 후보
        return 1;
    }

    // 2. 삼각함수 바깥에 변수 'x', 지수(exp), 로그(ln)가 섞여 있는 경우 (예: x - sin(x))
    if (!inside_trig) {
        if (node->type == NODE_VARIABLE && strcmp(node->value, "x") == 0) {
            return 0; // 1순위: 비주기 다항 항 혼합 -> 10진 눈금
        }
        if (node->type == NODE_FUNCTION) {
            // 삼각함수 외의 다른 초월함수(exp, ln, sqrt 등)가 바깥에 존재
            return 0;
        }
    }

    int left_res = analyze_ast_pi_mode(node->left, inside_trig);
    int right_res = analyze_ast_pi_mode(node->right, inside_trig);

    // 어느 한쪽이라도 10진 강제(0) 판정이 나면 전체 식은 비주기 함수로 10진 눈금(0) 우선
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

// qsort용 double 오름차순 비교 함수 (백분위 이상치 컷용)
static int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

// [보조 함수] 단일 함수 배열에서 점근선을 마스킹하고 유효 [y_low, y_high] 추출
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
    res.is_1to1_locked = (opt->scale_target == SCALE_LOCK_1TO1) ? 1 : 0;

    int ticks = (opt->target_ticks > 0) ? opt->target_ticks : 8;
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
        y_low = (f_low < g_low) ? f_low : g_low;
        y_high = (f_high > g_high) ? f_high : g_high;
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

//2. 함수의 심화 정보 (극점, 교점, 극한값"만" 있는 점, 변곡점) & 소수점 근사치 복원

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
static void format_symbolic_or_decimal(double val, char* out_buf, size_t buf_size) {
    const double EPS = 1e-11;

    if (isnan(val) || isinf(val)) {
        snprintf(out_buf, buf_size, "NaN");
        return;
    }
    if (fabs(val) < EPS) {
        snprintf(out_buf, buf_size, "0");
        return;
    }

    // 1. 정수 판별
    double r_int = round(val);
    if (fabs(val - r_int) < EPS) {
        snprintf(out_buf, buf_size, "%.0f", r_int);
        return;
    }

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
            return;
        }
    }

    // 3. π 유리수배 (p*π / q, 분모 상한 q <= 12)
    double pi_ratio = val / M_PI;
    for (int q = 1; q <= 12; q++) {
        double p_dbl = round(pi_ratio * q);
        if (fabs(p_dbl) > 0 && fabs(pi_ratio - p_dbl / q) < EPS) {
            int p = (int)p_dbl;
            int g = gcd_int(p, q);
            p /= g;
            int rq = q / g;
            if (rq == 1) {
                if (p == 1)       snprintf(out_buf, buf_size, "pi");
                else if (p == -1) snprintf(out_buf, buf_size, "-pi");
                else              snprintf(out_buf, buf_size, "%dpi", p);
            }
            else {
                if (p == 1)       snprintf(out_buf, buf_size, "pi/%d", rq);
                else if (p == -1) snprintf(out_buf, buf_size, "-pi/%d", rq);
                else              snprintf(out_buf, buf_size, "%dpi/%d", p, rq);
            }
            return;
        }
    }

    // 4. 단순 제곱근 ±sqrt(d) 판별 (근호 상한 d <= 100)
    double sq = val * val;
    double r_sq = round(sq);
    if (r_sq >= 2.0 && r_sq <= 100.0 && fabs(sq - r_sq) < EPS) {
        int d = (int)r_sq;
        int root_d = (int)round(sqrt((double)d));
        if (root_d * root_d != d) {
            snprintf(out_buf, buf_size, "%ssqrt(%d)", (val < 0 ? "-" : ""), d);
            return;
        }
    }

    // 5. 이차방정식 근의 공식 형태 (p ± sqrt(d)) / q (삼차함수 극점, etc.)
    for (int q = 1; q <= 20; q++) 
    {
        for (int d = 2; d <= 100; d++) 
        {
            int root_d = (int)round(sqrt((double)d));
            if (root_d * root_d == d) continue; // 완전제곱수 제외

            double sq_d = sqrt((double)d);
            // +sqrt(d) 경우
            double p_plus = round(val * q - sq_d);
            if (fabs(val - (p_plus + sq_d) / q) < EPS) 
            {
                int p = (int)p_plus;
                if (q == 1) snprintf(out_buf, buf_size, "%d + sqrt(%d)", p, d);
                else        snprintf(out_buf, buf_size, "(%d + sqrt(%d))/%d", p, d, q);
                return;
            }
            // -sqrt(d) 경우
            double p_minus = round(val * q + sq_d);
            if (fabs(val - (p_minus - sq_d) / q) < EPS) 
            {
                int p = (int)p_minus;
                if (q == 1) snprintf(out_buf, buf_size, "%d - sqrt(%d)", p, d);
                else        snprintf(out_buf, buf_size, "(%d - sqrt(%d))/%d", p, d, q);
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

// 심화 특징점(절편, 극점, 변곡점, 교점, 극한값만 있는 점) 정밀 추출기

// 대상 함수 mode: 0 = f(x), 1 = f'(x), 2 = f''(x), 3 = f(x) - g(x)
static double eval_target_func(const ASTNode* f, const ASTNode* g, double x, int mode) {
    const double h = 1e-6;
    if (mode == 0) return eval_ast(f, x);
    if (mode == 3) return eval_ast(f, x) - eval_ast(g, x);
    if (mode == 1) {
        // 중앙차분 1차 도함수
        return (eval_ast(f, x + h) - eval_ast(f, x - h)) / (2.0 * h);
    }
    if (mode == 2) {
        // 중앙차분 2차 도함수
        return (eval_ast(f, x + h) - 2.0 * eval_ast(f, x) + eval_ast(f, x - h)) / (h * h);
    }
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
    if (fa * fb > 0.0) return false; //이분법 구간

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
    for (int i = 0; i < *count; i++) {
        if (list[i].type == type && fabs(list[i].x - x) < 1e-7 && fabs(list[i].y - y) < 1e-7) {
            return;
        }
    }

    char x_str[32], y_str[32];
    format_symbolic_or_decimal(x, x_str, sizeof(x_str));
    format_symbolic_or_decimal(y, y_str, sizeof(y_str));

    list[*count].type = type;
    list[*count].x = x;
    list[*count].y = y;
    snprintf(list[*count].label, sizeof(list[*count].label), "(%s, %s)", x_str, y_str);
    (*count)++;
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
            push_keypoint(out_points, &count, max_points, KP_Y_INTERCEPT, 0.0, y0);
        }
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
        check_direct_candidate(f_root, g_root, -SIMPLE_IRRATIONALS[i], x_min, x_max, out_points, &count, max_points);
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
            double y_ext = eval_ast(f_root, root);
            double d2 = eval_target_func(f_root, NULL, root, 2);
            if (!isnan(y_ext) && !isinf(y_ext) && !isnan(d2) && fabs(d2) > 1e-7) {
                KeyPointType ext_type = (d2 < 0.0) ? KP_LOCAL_MAX : KP_LOCAL_MIN;
                push_keypoint(out_points, &count, max_points, ext_type, root, y_ext);
                // X축에 접하는 중근(극값이면서 동시에 y = 0) 보완 등록
                if (fabs(y_ext) < 1e-9) {
                    push_keypoint(out_points, &count, max_points, KP_X_INTERCEPT, root, 0.0);
                }
            }
        }
        // (c) 변곡점: f''(x) = 0 (전후 2계 도함수 부호 반전 확인)
        if (refine_root_hybrid(f_root, NULL, a, b, 2, &root)) {
            double y_inf = eval_ast(f_root, root);
            double d2_left = eval_target_func(f_root, NULL, root - 1e-4, 2);
            double d2_right = eval_target_func(f_root, NULL, root + 1e-4, 2);
            if (!isnan(y_inf) && !isinf(y_inf) && (d2_left * d2_right < 0.0)) {
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

// 3. 고해상도 동적 샘플링, 특이점 보간 & 고계 미분 정밀도 보완 (최종 통합부)

// [해결책 3] 고계 미분 및 테일러 급수 근사 시 부동소수점 오차 누적 억제
// 차수(order)에 맞춰 최적 스텝 크기 h_n = eps^(1/(n+2)) 동적 조정 및 확장 정밀도(long double) 연산
// 대상의 크기에 따라 돋보기의 배율을 자동으로 조정해주는 코드
double eval_nth_derivative_safe(const ASTNode* f, double x, int order) {
    if (!f || order < 0) return NAN;
    if (order == 0) return eval_ast(f, x);

    // 돋보기 배율 결정
    const long double LDBL_EPS = 1e-16L; 
    double h = (double)powl(LDBL_EPS, 1.0L / (long double)(order + 2));
    if (h < 1e-7) h = 1e-7;
    if (h > 1e-2) h = 1e-2;

    // 1계 ~ 4계 중심 차분 고밀도 스텐실, 함수를 따로 정의하는게 아닌 한번에 계산 (테일러 급수 PIP용 오차 최소화)
    if (order == 1) {
        long double fm2 = eval_ast(f, x - 2.0 * h);
        long double fm1 = eval_ast(f, x - h);
        long double fp1 = eval_ast(f, x + h);
        long double fp2 = eval_ast(f, x + 2.0 * h);
        return (double)((fm2 - 8.0L * fm1 + 8.0L * fp1 - fp2) / (12.0L * h));
    }
    if (order == 2) {
        long double fm2 = eval_ast(f, x - 2.0 * h);
        long double fm1 = eval_ast(f, x - h);
        long double f0 = eval_ast(f, x);
        long double fp1 = eval_ast(f, x + h);
        long double fp2 = eval_ast(f, x + 2.0 * h);
        return (double)((-fm2 + 16.0L * fm1 - 30.0L * f0 + 16.0L * fp1 - fp2) / (12.0L * h * h));
    }
    if (order == 3) {
        long double fm2 = eval_ast(f, x - 2.0 * h);
        long double fm1 = eval_ast(f, x - h);
        long double fp1 = eval_ast(f, x + h);
        long double fp2 = eval_ast(f, x + 2.0 * h);
        return (double)((-fm2 + 2.0L * fm1 - 2.0L * fp1 + fp2) / (2.0L * h * h * h));
    }
    if (order == 4) {
        long double fm2 = eval_ast(f, x - 2.0 * h);
        long double fm1 = eval_ast(f, x - h);
        long double f0 = eval_ast(f, x);
        long double fp1 = eval_ast(f, x + h);
        long double fp2 = eval_ast(f, x + 2.0 * h);
        return (double)((fm2 - 4.0L * fm1 + 6.0L * f0 - 4.0L * fp1 + fp2) / (h * h * h * h));
    }

    // 5계 이상 재귀 호출 시에도 차수별 최적 스텝 유지
    double h_high = 1e-2;
    return (eval_nth_derivative_safe(f, x + h_high, order - 1)
        - eval_nth_derivative_safe(f, x - h_high, order - 1)) / (2.0 * h_high);
}

// [해결책 2] 특이점 구간(ln(x) 정의역 경계, sin(x)/x 제거 가능 특이점) 정밀 평가 헬퍼
static double eval_with_singularity_guard(const ASTNode* f, double x, bool* out_is_hole) {
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
                    if (!isnan(probe_y) && !isinf(probe_y))
                    {
                        out_x[count] = probe_x; out_y[count] = probe_y; count++;
                    }
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