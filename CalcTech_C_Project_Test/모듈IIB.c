#include "CalcTech공용두꺼비집.h"
//모듈 IIB: 파생함수 10가지 기능 구현


//파생함수 I: 심프슨 the GOAT

//파생함수IA: 곡선위에 그은 접선이 만드는 도형의 넓이
// 접선 정보 구조체 생성 및 접선의 방정식 계산: g_t(x) = f'(t)(x - t) + f(t)
TangentInfo calc_tangent_info(const ASTNode* f, double t) 
{
    TangentInfo info;
    info.t = t;
    info.ft = eval_ast(f, t); //f(t)
    info.slope = num_diff_1st(f, t); //f'(t)

    // y절편 g_t(0) = f(t) - t * f'(t)
    info.y_intercept = info.ft - t * info.slope;

    // x절편 g_t(x) = 0 => x = t - f(t) / f'(t)
    if (fabs(info.slope) < ZERO_EPSILON) {
        info.x_intercept = NAN; // 수평 접선이면 x절편 없음
    }
    else {
        info.x_intercept = t - (info.ft / info.slope);
    }

    return info;
}

double eval_tangent(TangentInfo tan_info, double x) {
    return tan_info.slope * (x - tan_info.t) + tan_info.ft;
}

double eval_boundary(int bound_type, const ASTNode* f, TangentInfo tan_info, double x) 
{ //곡선과 접선이 이루는 영역의 4가지 유형의 위아래를 막아주는 재료
    switch (bound_type) {
    case BOUND_FUNC:   return eval_ast(f, x); //곡선자체의 높이
    case BOUND_TAN:    return eval_tangent(tan_info, x); //접선 자체의 높이
    case BOUND_X_AXIS: return 0.0; // y=0의 높이값
    default:           return NAN;
    }
}
//FUNC + TAN => 곡선 & 접선, TAN + X_AXIS => 접선 & 두 축
//FUNC + X_AXIS => 곡선 & 축, 세가지 전부 => 곡선 & 접선 & 축

// 범용 심프슨 1/3 적분기: 어떤 경계 조합이든 |top(x) - bot(x)| 적분
double integrate_between_bounds(
    const ASTNode* f,
    TangentInfo tan_info,
    double a,
    double b,
    int top_type,
    int bot_type,
    int steps
) {
    if (f == NULL || isnan(a) || isnan(b)) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (b - a) / (double)steps;
    //높이에 심프슨 법칙의 가중치를 곱한값을 누적하는 바구니
    double sum = fabs(eval_boundary(top_type, f, tan_info, a) - eval_boundary(bot_type, f, tan_info, a))
        + fabs(eval_boundary(top_type, f, tan_info, b) - eval_boundary(bot_type, f, tan_info, b));

    for (int i = 1; i < steps; i++) { //
        double xi = a + i * dx;
        double y_top = eval_boundary(top_type, f, tan_info, xi);
        double y_bot = eval_boundary(bot_type, f, tan_info, xi);
        double diff = fabs(y_top - y_bot);
        if (isnan(diff)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * diff;
    }

    return sum * (dx / 3.0);
}

// [내부 도우미] 중복 없이 핵심 x좌표 추가 & 이분법 근 탐색
static void add_unique_point(double* arr, int* count, double val, double x_min, double x_max) {
    if (isnan(val) || val < x_min - 1e-7 || val > x_max + 1e-7) return;
    if (val < x_min) val = x_min;
    if (val > x_max) val = x_max;

    for (int i = 0; i < *count; i++) {
        if (fabs(arr[i] - val) < 1e-4) return; // 중복 좌표 무시
    }
    if (*count < MAX_CRITICAL_POINTS) {
        arr[(*count)++] = val;
    }
}

// mode == 0: 곡선과 접선의 교점 탐색, mode == 1: 곡선과 x축의 교점 탐색 탐색
static void scan_roots(const ASTNode* f, TangentInfo tan_info, int mode,
    double x_min, double x_max, double* pts, int* count) {
    int scan_steps = 1000;
    double dx = (x_max - x_min) / (double)scan_steps;

    for (int i = 0; i < scan_steps; i++) {
        double a = x_min + i * dx;
        double b = a + dx;
        double ha = (mode == 0) ? (eval_ast(f, a) - eval_tangent(tan_info, a)) : eval_ast(f, a);
        double hb = (mode == 0) ? (eval_ast(f, b) - eval_tangent(tan_info, b)) : eval_ast(f, b);

        if (isnan(ha) || isnan(hb)) continue;

        if (ha * hb <= 0.0) {
            double left = a, right = b, h_left = ha;
            for (int iter = 0; iter < 40; iter++) {
                double mid = 0.5 * (left + right);
                double h_mid = (mode == 0) ? (eval_ast(f, mid) - eval_tangent(tan_info, mid)) : eval_ast(f, mid);
                if (fabs(h_mid) < ZERO_EPSILON) { left = right = mid; break; }
                if (h_left * h_mid <= 0.0) right = mid;
                else { left = mid; h_left = h_mid; }
            }
            add_unique_point(pts, count, 0.5 * (left + right), x_min, x_max);
        }
    }
}

// 평면 자동 분할기: 4가지 유형의 모든 영역을 자동 추출
int partition_tangent_regions(
    const ASTNode* f,
    TangentInfo tan_info,
    double x_min,
    double x_max,
    TangentRegion* out_regions,
    int max_regions
) {
    if (f == NULL || out_regions == NULL || max_regions <= 0) return 0;

    double pts[MAX_CRITICAL_POINTS];
    int pt_cnt = 0;

    // (1) 모든 경계 후보 x좌표 수집
    add_unique_point(pts, &pt_cnt, x_min, x_min, x_max);                // 화면 왼쪽 끝
    add_unique_point(pts, &pt_cnt, x_max, x_min, x_max);                // 화면 오른쪽 끝
    add_unique_point(pts, &pt_cnt, 0.0, x_min, x_max);                  // y축 (x = 0)
    add_unique_point(pts, &pt_cnt, tan_info.t, x_min, x_max);           // 접점 t
    add_unique_point(pts, &pt_cnt, tan_info.x_intercept, x_min, x_max); // 접선의 x절편
    scan_roots(f, tan_info, 0, x_min, x_max, pts, &pt_cnt);              // 곡선-접선 교점
    scan_roots(f, tan_info, 1, x_min, x_max, pts, &pt_cnt);              // 곡선의 x절편 (f(x)=0)

    // (2) x좌표 오름차순 정렬
    for (int i = 1; i < pt_cnt; i++) {
        double key = pts[i];
        int j = i - 1;
        while (j >= 0 && pts[j] > key) {
            pts[j + 1] = pts[j];
            j--;
        }
        pts[j + 1] = key;
    }

    // (3) 각 세로 구간 [pts[k], pts[k+1]] 마다 상/중/하 세 경계를 정렬해 2개 층의 영역 생성
    int reg_cnt = 0;
    for (int k = 0; k < pt_cnt - 1 && reg_cnt < max_regions; k++) {
        double a = pts[k];
        double b = pts[k + 1];
        if (b - a < 1e-5) continue;

        double mid = 0.5 * (a + b);
        double vals[3] = { eval_ast(f, mid), eval_tangent(tan_info, mid), 0.0 };
        int types[3] = { BOUND_FUNC,       BOUND_TAN,                   BOUND_X_AXIS };

        if (isnan(vals[0]) || isnan(vals[1])) continue;

        // 3개의 y값을 오름차순 정렬 (vals[0] <= vals[1] <= vals[2])
        for (int i = 0; i < 2; i++) {
            for (int j = i + 1; j < 3; j++) {
                if (vals[i] > vals[j]) {
                    double tv = vals[i]; vals[i] = vals[j]; vals[j] = tv;
                    int tt = types[i];   types[i] = types[j]; types[j] = tt;
                }
            }
        }

        // 화면 좌우 끝(x_min, x_max)에 닿아있는지 여부 판정
        int closed_flag = (fabs(a - x_min) > 1e-4 && fabs(b - x_max) > 1e-4) ? 1 : 0;

        // 아래층 영역 [vals[0] ~ vals[1]] 및 위층 영역 [vals[1] ~ vals[2]] 등록
        for (int layer = 0; layer < 2 && reg_cnt < max_regions; layer++) {
            double area = integrate_between_bounds(f, tan_info, a, b, types[layer + 1], types[layer], 200);
            if (!isnan(area) && area > 1e-6) {
                out_regions[reg_cnt].id = reg_cnt;
                out_regions[reg_cnt].a = a;
                out_regions[reg_cnt].b = b;
                out_regions[reg_cnt].bot_type = types[layer];
                out_regions[reg_cnt].top_type = types[layer + 1];
                out_regions[reg_cnt].area = area;
                out_regions[reg_cnt].is_closed = closed_flag;
                reg_cnt++;
            }
        }
    }

    return reg_cnt;
}

//[★ PIP 파생함수] t 이동에 따른 선택 영역 넓이 변화 함수 S(t) 생성
void generate_tangent_area_S(
    const ASTNode* f,
    double t_min,
    double t_max,
    double x_min,
    double x_max,
    int target_region_id,
    int count,
    double* out_t,
    double* out_S
) {
    if (f == NULL || count < 2) return;

    double dt = (t_max - t_min) / (double)(count - 1);
    TangentRegion temp_regions[MAX_REGIONS];

    for (int i = 0; i < count; i++) {
        double t = t_min + i * dt;
        if (out_t) out_t[i] = t;

        if (out_S) {
            TangentInfo tan_info = calc_tangent_info(f, t);
            int reg_cnt = partition_tangent_regions(f, tan_info, x_min, x_max, temp_regions, MAX_REGIONS);

            if (target_region_id >= 0 && target_region_id < reg_cnt) {
                out_S[i] = temp_regions[target_region_id].area;
            }
            else {
                out_S[i] = NAN;
            }
        }
    }
}

//파생함수 IB: 정적분으로 정의된 함수의 변화량 추적

// 내부 도우미: 단일 구간 [x1, x2]에 대한 심프슨 1/3 정적분 (is_abs == 1이면 |f(t)| 적분)
// 기존에 있는 함수에는 절댓값 옵션이 존재하지 않았음
static double simpson_piece(const ASTNode* f, double x1, double x2, int steps, int is_abs) 
{
    if (fabs(x2 - x1) < 1e-15) return 0.0; //위아래끝이 같은 경우
    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (x2 - x1) / (double)steps;
    double y0 = eval_ast(f, x1);
    double yn = eval_ast(f, x2);
    if (isnan(y0) || isnan(yn)) return NAN;

    double sum = (is_abs ? fabs(y0) : y0) + (is_abs ? fabs(yn) : yn);

    for (int i = 1; i < steps; i++) {
        double xi = x1 + i * dx;
        double yi = eval_ast(f, xi);
        if (isnan(yi)) return NAN;
        double val = is_abs ? fabs(yi) : yi;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * val;
    }
    return sum * (dx / 3.0);
}

//g(x)의 그래프를 PIP에 그리기 위한 밑작업 함수
void generate_def_integral_array(
        const ASTNode * f,
        double x_min,
        double x_max,
        int mode, //패턴 1(시작점 고정 a~x), 패턴 2(구간 이동 x~x+h), 패턴 3(대칭 구간 a-x~a+x), 
        //위/아래끝이 또다른 함수의 경우는 IC에서 다룰 예정
        double param, //패턴 1, 3에서는 기준점 'a', 패턴 2에서는 구간 폭 'h'
        int is_abs, //0이면 순수 정적분(부호 유지), 1이면 기하학적 넓이(| f(t) | )
        int count,
        double* out_x,
        double* out_g
    ) 
{
        if (f == NULL || count < 2 || out_x == NULL || out_g == NULL) return;

        double dx = (x_max - x_min) / (double)(count - 1);

        // 1. 가로축 x 좌표 채우기
        for (int i = 0; i < count; i++) {
            out_x[i] = x_min + i * dx;
        }

        // 2. 패턴별 고속 계산
        if (mode == DEF_INT_FIXED_START) 
        {
            // [패턴 1] g(x) = ∫_a^x f(t) dt => a부터 
            // 기준점 param(a)에서 가장 가까운 격자 인덱스 찾기
            int base_idx = (int)round((param - x_min) / dx);
            if (base_idx < 0) base_idx = 0;
            if (base_idx >= count) base_idx = count - 1;

            // 기준점에서의 정확한 오프셋: ∫_a^(out_x[base_idx]) f(t) dt
            out_g[base_idx] = simpson_piece(f, param, out_x[base_idx], 20, is_abs);

            // 오른쪽으로 누적 적분 (O(N))
            for (int i = base_idx + 1; i < count; i++) {
                double piece = simpson_piece(f, out_x[i - 1], out_x[i], 4, is_abs);
                out_g[i] = out_g[i - 1] + piece;
            }
            // 왼쪽으로 누적 적분 (방향이 반대이므로 뺄셈, O(N))
            for (int i = base_idx - 1; i >= 0; i--) {
                double piece = simpson_piece(f, out_x[i], out_x[i + 1], 4, is_abs);
                out_g[i] = out_g[i + 1] - piece;
            }
        }
        else if (mode == DEF_INT_SLIDING_WIN) {
            // [패턴 2] g(x) = ∫_x^(x+h) f(t) dt  (param == h)
            for (int i = 0; i < count; i++) {
                double a = out_x[i];
                double b = a + param;
                out_g[i] = simpson_piece(f, a, b, 60, is_abs);
            }
        }
        else if (mode == DEF_INT_SYMMETRIC) {
            // [패턴 3] g(x) = ∫_(a-x)^(a+x) f(t) dt (param == a)
            for (int i = 0; i < count; i++) {
                double x = out_x[i];
                double a = param - x;
                double b = param + x;
                out_g[i] = simpson_piece(f, a, b, 60, is_abs);
            }
        }

        //cf)
        /*패턴 I: 양방향 누적, 영점을 기준으로 양방향 모두 계산하되, 오른쪽으로 가면 넓이에 +, 왼쪽으로 가면 -
        패턴 II: 폭이 h인 구간을 창문으로 가정하고, 그 창문을 움직이면 아래의 넓이가 어떻게 되는지 관찰
        패턴 III: a를 중심으로두고 양방향으로 같이 멀어져가면서 관찰*/
 }

//파생함수IC: 특이함수 정적분(위·아래끝 함수, 특이점 보정) 및 이상적분 (정적분의 극한)


// ※ 이상적분: a~x까지의 정적분을 하고 a값을 극한으로 보내는 적분 (제1종)
// 제2종: 점근선 같은 구간을 피한 이상적분
static double improper_integral_II(const ASTNode* f, double a, double b, int steps) 
{
    if (f == NULL || isnan(a) || isnan(b)) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    double sign = 1.0;
    if (a > b) { double tmp = a; a = b; b = tmp; sign = -1.0; } //Call by Value로 변수 스위치

    // 끝점 특이점 회피용 미세 오프셋 (구간 길이가 아주 작으면 그에 비례해 축소)
    double eps = 1e-7;
    if (b - a < 1e-5) eps = (b - a) * 1e-3;
    a += eps;
    b -= eps;

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (b - a) / (double)steps;
    double y0 = eval_ast(f, a);
    double yn = eval_ast(f, b);
    if (isnan(y0) || isinf(y0) || isnan(yn) || isinf(yn)) return NAN;

    double sum = y0 + yn;

    for (int i = 1; i < steps; i++) {
        double xi = a + i * dx;
        double yi = eval_ast(f, xi);

        // 구간 내부에 특이점(예: 1/ln(x)의 x=1)이 걸리면 좌우 극한 평균(코시 주값)으로 보정
        if (isnan(yi) || isinf(yi)) 
        {
            double y_left = eval_ast(f, xi - 1e-8);
            double y_right = eval_ast(f, xi + 1e-8);
            if (isnan(y_left) || isnan(y_right)) return NAN;
            yi = 0.5 * (y_left + y_right);
        }
        sum += (i % 2 == 0 ? 2.0 : 4.0) * yi;
    }

    return sign * sum * (dx / 3.0);
}

//위끝·아래끝이 또 다른 함수인 정적분 배열 생성: g(x) = ∫_{alpha(x)}^{beta(x)} f(t) dt
//또 심프슨 법칙이야. 우린 숭배해야만해 
void generate_func_bounds_integral_array(
    const ASTNode* f,
    const ASTNode* alpha, // 아래끝 함수 α(x)
    const ASTNode* beta,  // 위끝 함수 β(x)
    double x_min,
    double x_max,
    int count,
    double* out_x,
    double* out_g
) {
    if (f == NULL || alpha == NULL || beta == NULL || count < 2) return; //예외처리

    double dx = (x_max - x_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        if (out_x) out_x[i] = x;

        if (out_g) {
            double a = eval_ast(alpha, x);
            double b = eval_ast(beta, x);
            out_g[i] = improper_integral_II(f, a, b, 100);
        }
    }
}

// 무한 구간 이상적분 단일 값 계산: ∫_a^∞ f(x) dx
// 제1종: a~x를 정적분한 후 a를 극한으로 보내는 적분
// - 변수 치환 x = a + u / (1 - u) 를 통해 [a, ∞) 구간을 유한 구간 [0, 1)로 압축하여 적분
double improper_integral_I(const ASTNode* f, double a, int steps) {
    if (f == NULL || isnan(a)) return NAN;
    if (steps < 100) steps = 100;
    if (steps % 2 != 0) steps++;

    // u = 1.0 에서 분모 (1-u)^2 이 0이 되므로 1 - 1e-5 까지만 적분
    double u_min = 0.0;
    double u_max = 1.0 - 1e-5;
    double du = (u_max - u_min) / (double)steps;

    double sum = 0.0;
    for (int i = 0; i <= steps; i++) { 
        double u = u_min + i * du;
        double denom = 1.0 - u;
        double x = a + (u / denom);
        double jacobian = 1.0 / (denom * denom); // dx/du = 1 / (1-u)^2
        //길이가 무한대인 고무줄을 0.xx길이에 억지로 쑤셔넣었으니 1에 가까워질수록 간격을 훨씬 촘촘해짐
        double fx = eval_ast(f, x);
        if (isnan(fx) || isinf(fx)) return NAN;

        double val = fx * jacobian;
        double weight = (i == 0 || i == steps) ? 1.0 : ((i % 2 == 0) ? 2.0 : 4.0);
        sum += weight * val;
    }

    double result = sum * (du / 3.0);

    // 발산 판정: 끝자락 기여도가 너무 크면 발산(NAN) 처리
    if (fabs(result) > 1e8) return NAN;
    return result;
}

//파생함수 II: 접점 t 이동에 따른 접선의 절편 & 축 삼각형 넓이 변화
void generate_tangent_intercepts_array(
    const ASTNode* f,
    double t_min,
    double t_max,
    int count,
    double* out_t,
    double* out_x_int, //접선의 x절편 X(t) = t - f(t) / f'(t) 배열 (수평 접선 구간은 NAN)
    double* out_y_int, //접선의 y절편 Y(t) = f(t) - t*f'(t) 배열
    double* out_tri_area //접선과 x축, y축이 이루는 직각삼각형 넓이 0.5 * |X(t) * Y(t)| 배열
) {
    if (f == NULL || count < 2) return;

    double dt = (t_max - t_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double t = t_min + i * dt;
        if (out_t) out_t[i] = t;

        // IA에서 만든 접선 계산기 그대로 재활용
        TangentInfo info = calc_tangent_info(f, t);

        if (out_x_int) out_x_int[i] = info.x_intercept;
        if (out_y_int) out_y_int[i] = info.y_intercept;

        if (out_tri_area) {
            if (isnan(info.x_intercept) || isnan(info.y_intercept)) {
                out_tri_area[i] = NAN;
            }
            else {
                out_tri_area[i] = 0.5 * fabs(info.x_intercept * info.y_intercept);
            }
        }
    }
}

// 파생함수 III: 합성함수

// IIIA: N축 기반 합성함수 개형 추론 & 수선 궤적 추적

// IIIB: 합성 방부등식 실근, 해집합 스캐닝