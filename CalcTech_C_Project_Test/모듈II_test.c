#include "CalcTech공용두꺼비집.h"

//모듈II: 모듈I에서 출력한 LaTeX 수식이 그리는 그래프를 수학적으로 접근할 수 있는 함수 만들기

//IIA: C언어로 미분,적분 구현하기
// AST 수식 평가기: 트리를 후위 순회하며 x 대입 결과(double) 반환
// 첫번째 인자는 자판기의 내용물, 두번째 인자는 자판기에 넣을 동전
// 최종적으로는 계산된 결과라는 음료수를 밷어내는 함수
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
        if (node->left == NULL && strcmp(node->value, "-") == 0) return -eval_ast(node->right, x);

        double L = eval_ast(node->left, x);
        double R = eval_ast(node->right, x);

        if (strcmp(node->value, "+") == 0) return L + R;
        if (strcmp(node->value, "-") == 0) return L - R;
        if (strcmp(node->value, "*") == 0) return L * R;
        if (strcmp(node->value, "/") == 0) return (fabs(R) < 1e-15) ? NAN : (L / R); //0으로 나누는 경우 제외
        if (strcmp(node->value, "^") == 0) return pow(L, R);
        return NAN;
    }

    // (4) 단일 인자 수학 함수 (KEYWORD_TABLE 전체 대응)
    if (node->type == NODE_FUNCTION)
    {
        const char* fn = node->value;
        if (strcmp(fn, "prime") == 0) return num_diff_1st(node->left, x);
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
            if (base <= 0.0 || base == 1.0 || val <= 0.0) return NAN; //로그의 정의조건 예외처리
            return log(val) / log(base);
        }
        if (strcmp(op, "diff") == 0 && node->arg_count == 3)
        {
            int order = (int)eval_ast(node->args[2], x);
            if (order == 1) return num_diff_1st(node->args[0], x); //1계미분
            if (order == 2) return num_diff_2nd(node->args[0], x); //2계미분
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
        //홀수 인덱스 -> 배열의 짝수번째 ->x4 
        //짝수 인덱스 -> 배열의 홀수번째 ->x2
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
    if (root == NULL || count < 2) return;

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
            if (i == 0) { out_int_y[0] = 0.0; }
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
    case BOUND_FUNC:    return eval_ast(f, x); //곡선자체의 높이
    case BOUND_TAN:      return eval_tangent(tan_info, x); //접선 자체의 높이
    case BOUND_X_AXIS: return 0.0; // y=0의 높이값
    default:                      return NAN;
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
    //심프슨 법칙을 쓸떈 짝수번째 인덱스만 가능
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
void add_unique_point(double* arr, int* count, double val, double x_min, double x_max) {
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
void scan_roots(const ASTNode* f, TangentInfo tan_info, int mode,
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
double simpson_piece(const ASTNode* f, double x1, double x2, int steps, int is_abs) 
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
    }  //홀수 인덱스 -> 배열의 짝수번째 ->x4 
        //짝수 인덱스 -> 배열의 홀수번째 ->x2
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
double improper_integral_II(const ASTNode* f, double a, double b, int steps) 
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

// N축 기반 합성함수 개형 추론 & 수선 궤적 추적
void generate_composite_n_axis_arrays(
    const ASTNode* f, // 겉함수 f(t)
    const ASTNode* g, // 속함수 g(x)
    double x_min,
    double x_max,
    int count,
    double* out_x, //out_x   : 입력 정의역 x 배열
    double* out_t, //out_t   : 속함수 치역 t = g(x) 배열 (N축 공유 축 좌표)
    double* out_fog //out_fog : 최종 합성함수 y = f(g(x)) 배열 (PIP 캔버스용)
) {
    if (f == NULL || g == NULL || count < 2) return;

    double dx = (x_max - x_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        double t = eval_ast(g, x); //속함수 g(x) = t
        double y = isnan(t) ? NAN : eval_ast(f, t); //겉함수 y = f(t)
        //y = f(t), t = g(x)
        if (out_x)   out_x[i] = x;
        if (out_t)   out_t[i] = t;
        if (out_fog) out_fog[i] = y;
    }
}

// 합성방정식 실근 역추적 & 합성부등식 해집합 스캐닝

// [내부 도우미] 일반 함수방정식 h(u) = target 의 실근을 함수의 정의역 에서 모두 탐색
  int scan_equation_roots(
    const ASTNode* func,
    double target, //f(g(x)) = k에서 g(x)를 t로 치환하여 g(x) = t라는 방정식에서는 t가 target, f(t) = k에선 k가 target
    double u_min,
    double u_max,
    double* out_roots, //실근을 담는 바구니
    int max_roots
) {
    int count = 0;
    int scan_steps = 1000;
    double du = (u_max - u_min) / (double)scan_steps; //

    for (int i = 0; i < scan_steps && count < max_roots; i++) {
        double a = u_min + i * du;
        double b = a + du;
        double ha = eval_ast(func, a) - target;
        double hb = eval_ast(func, b) - target;

        if (isnan(ha) || isnan(hb)) continue;

        if (ha * hb <= 0.0) {
            double left = a, right = b, h_left = ha;
            for (int iter = 0; iter < 40; iter++) {
                double mid = 0.5 * (left + right);
                double h_mid = eval_ast(func, mid) - target;
                if (fabs(h_mid) < ZERO_EPSILON) { left = right = mid; break; }
                if (h_left * h_mid <= 0.0) right = mid;
                else { left = mid; h_left = h_mid; }
            }
            double root = 0.5 * (left + right);

            // 중복 근 방지
            int dup = 0;
            for (int k = 0; k < count; k++) {
                if (fabs(out_roots[k] - root) < 1e-4) { dup = 1; break; }
            }
            if (!dup) out_roots[count++] = root;
        }
    }
    return count;
}

//  합성방정식 f(g(x)) = k 의 모든 실근과 N축 수선 꺾임 좌표 추출
//  반환값: 찾아낸 전체 실근 x의 총개수
int solve_composite_equation_n_axis(
    const ASTNode* f, // 겉함수 f(t)
    const ASTNode* g, // 속함수 g(x)
    double k,         // 우변 상수 k (f(g(x)) = k)
    double t_min,
    double t_max,
    double x_min,
    double x_max,
    CompositeRootPath* out_paths,
    int max_paths
) {
    if (f == NULL || g == NULL || out_paths == NULL || max_paths <= 0) return 0;

    // 1단계: 겉함수 f(t) = k 를 만족하는 중간 해 t_j 탐색
    double t_roots[64];
    int t_cnt = scan_equation_roots(f, k, t_min, t_max, t_roots, 64);

    // 2단계: 각 t_j 에서 속함수로 수선의 발을 내려 g(x) = t_j 의 실근 x 탐색
    int total_paths = 0;
    double x_roots[64];

    for (int i = 0; i < t_cnt && total_paths < max_paths; i++) {
        double tj = t_roots[i];
        int x_cnt = scan_equation_roots(g, tj, x_min, x_max, x_roots, 64);

        for (int j = 0; j < x_cnt && total_paths < max_paths; j++) {
            out_paths[total_paths].t_val = tj;
            out_paths[total_paths].x_val = x_roots[j];
            total_paths++;
        }
    }

    return total_paths;
}

// 합성부등식 f(g(x)) [op] k 해집합 마스크 생성
void scan_composite_inequality(
    const ASTNode* f,
    const ASTNode* g,
    double k, 
    int op_type, //0 (> ), 1 (>= ), 2 (< ), 3 (<= )
    double x_min,
    double x_max,
    int count,
    double* out_x,
    int* out_mask // out_mask[i]: 부등식을 만족하면 1, 아니면 0 (파이썬에서 x축 형광 하이라이트용)
) {
    if (f == NULL || g == NULL || count < 2 || out_mask == NULL) return;

    double dx = (x_max - x_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        if (out_x) out_x[i] = x;

        double t = eval_ast(g, x);
        double y = isnan(t) ? NAN : eval_ast(f, t);

        if (isnan(y)) {
            out_mask[i] = 0;
            continue;
        }

        switch (op_type) {
        case 0: out_mask[i] = (y > k); break;
        case 1: out_mask[i] = (y >= k); break;
        case 2: out_mask[i] = (y < k); break;
        case 3: out_mask[i] = (y <= k); break;
        default: out_mask[i] = 0;       break;
        }
    }
}

//파생함수 IV: 도함수와 이계도함수의 서브캔버스 연동 (실험VI 확장판)

//원함수 -> 도함수, 도함수 -> 이계도함수
// 슬라이더 이동 시 현재 위치 t에서의 [원함수 접선] & [도함수 접선] 정보 일괄 추출
CDP calc_cascade_derivative_at(const ASTNode* f, double t) {
    CDP pt;
    pt.t = t;
    pt.ft = eval_ast(f, t);
    pt.dft = num_diff_1st(f, t);
    pt.d2ft = num_diff_2nd(f, t);

    // 1단계: f(x) 위 점 (t, f(t))에서의 접선 방정식 절편
    pt.tan1_y_int = pt.ft - t * pt.dft;

    // 2단계: f'(x) 위 점 (t, f'(t))에서의 접선 방정식 절편 (동일 절차 반복)
    pt.tan2_y_int = pt.dft - t * pt.d2ft;

    return pt;
}

// PIP 1(f') 및 PIP 2(f'') 배경 곡선 배열 생성
void generate_cascade_derivative_arrays(
    const ASTNode* f,
    double x_min,
    double x_max,
    int count,
    double* out_x,
    double* out_df,
    double* out_d2f
) {
    if (f == NULL || count < 2) return;

    double dx = (x_max - x_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        if (out_x)   out_x[i] = x;
        if (out_df)  out_df[i] = num_diff_1st(f, x);
        if (out_d2f) out_d2f[i] = num_diff_2nd(f, x);
    }
}

//파생함수 V: 평균값 정리와 경사하강법
 
//파생함수 VA: 라그랑주 평균값 정리(MVT) 평행 접선 c 탐색 및 오차 상계(M) 분석
/* ※라그랑주 MVT : 우리가 고등학교때 배운 MVT를 미분 가능한 초월함수까지 확장한 정리이고,
 함수가 절대로 넘을 수 없는 최대 속도 (립시츠 조건)를 찾는데 사용되는 정리중 하나.
 이 최대속도 (립시츠 상수 M)를 발견하면 컴퓨터에서 계산오차가 발생했을때의 타격을 예측 가능 */

MVTResult calc_lagrange_mvt(const ASTNode* f, double a, double b) 
{
    MVTResult res;
    memset(&res, 0, sizeof(MVTResult)); //memory + reset

    // 예외 처리: 함수 트리가 없거나 구간 길이가 0이면 빈 구조체 반환
    if (f == NULL || fabs(b - a) < ZERO_EPSILON) return res;

    // 사용자가 b < a 로 거꾸로 입력해도 자동으로 a < b 순서로 정렬
    if (a > b) {
        double tmp = a;
        a = b;
        b = tmp;
    }

    // 1단계: 양 끝점을 잇는 할선(평균변화율 직선) 정보 세팅
    res.a = a;
    res.b = b;
    res.fa = eval_ast(f, a);
    res.fb = eval_ast(f, b);
    res.secant_slope = (res.fb - res.fa) / (b - a);
    res.secant_y_int = res.fa - a * res.secant_slope;

    // 2단계: 구간 [a, b] 를 1000등분하여 스캔하면서
    // (1) 최대 기울기 M (오차 상계) 갱신
    // (2) f'(x) == secant_slope 가 되는 모든 만족점 c 포착
    int steps = 1000;
    double dx = (b - a) / (double)steps;

    for (int i = 0; i < steps; i++) {
        double x1 = a + i * dx;
        double x2 = x1 + dx;

        double d1 = num_diff_1st(f, x1); // 왼쪽 지점 순간변화율 f'(x1)
        double d2 = num_diff_1st(f, x2); // 오른쪽 지점 순간변화율 f'(x2)

        if (isnan(d1) || isnan(d2)) continue;

        // [공학적 확장] 구간 내 가장 가파른 기울기 |f'(x)|를 계속 기록해 둠
        // -> 나중에 |f(b) - f(a)| <= M * |b - a| (립시츠 오차 한계) 계산에 직결됨
        // 립시츠 오차 한계: 함수의 최대 속도 제한, M: 구간 내 접선의 기울기중 가장 가파른 기울기
        if (fabs(d1) > res.max_abs_deriv) {
            res.max_abs_deriv = fabs(d1);
        }

        // 보조함수 g(x) = f'(x) - secant_slope 정의
        // g(x) == 0 이 되는 지점이 곧 평균값 정리의 주인공 'c'
        double g1 = d1 - res.secant_slope;
        double g2 = d2 - res.secant_slope;

        // 인접한 두 점 사이에서 g(x)의 부호가 바뀌었다면 그 사이에 c가 존재함!
        if (g1 * g2 <= 0.0 && res.c_count < 32) {
            double left = x1, right = x2, gl = g1;

            // 이분법(Bisection) 35회 반복으로 c 좌표를 극한까지 정밀 조준
            for (int iter = 0; iter < 35; iter++) {
                double mid = 0.5 * (left + right);
                double gm = num_diff_1st(f, mid) - res.secant_slope; //보조함수 g(m)

                if (fabs(gm) < ZERO_EPSILON) {
                    left = right = mid;
                    break;
                }
                if (gl * gm <= 0.0) {
                    right = mid;
                }
                else {
                    left = mid;
                    gl = gm;
                }
            }

            double c = 0.5 * (left + right);

            // 평균값 정리는 열린구간 (a, b) 내부의 점을 찾으므로 양 끝점은 제외
            if (c > a + 1e-5 && c < b - 1e-5) {
                // 격자 경계에 딱 걸려 같은 근이 두 번 들어가는 중복 방지
                int is_dup = 0;
                for (int k = 0; k < res.c_count; k++) {
                    if (fabs(res.c_vals[k] - c) < 1e-4) {
                        is_dup = 1;
                        break;
                    }
                }

                // 최종 합격한 만족점 c의 좌표와 평행 접선 y절편 패키징
                if (!is_dup) {
                    int idx = res.c_count++;
                    res.c_vals[idx] = c;
                    res.fc_vals[idx] = eval_ast(f, c);
                    // 기울기가 secant_slope 이고 (c, f(c))를 지나는 평행 접선의 y절편
                    res.tan_y_ints[idx] = res.fc_vals[idx] - c * res.secant_slope;
                }
            }
        }
    }

    return res;
}

//파생함수 VB: 코시 평균값 정리 — 매개변수 곡선 (g(t), f(t)) 평행 속도 벡터 탐색


/*코시 MVT: 
아무리 어떤 물체가 wlfkf을 해도 현재 위치와 시작점을 이은 방향과 같아지는 순간은 반드시 존재한다는 정리.
이 식에서 어느 하나의 변수를 다른 변수로 극한을 보내면 로피탈정리 뚝딱*/
Cauchy calc_cauchy_mvt(const ASTNode* f, const ASTNode* g, double a, double b) {
    Cauchy res;
    memset(&res, 0, sizeof(Cauchy));

    if (f == NULL || g == NULL || fabs(b - a) < ZERO_EPSILON) return res;
    if (a > b) { double tmp = a; a = b; b = tmp; }

    // 1단계: 평면 곡선의 출발점(a)과 도착점(b), 그리고 둘을 잇는 변위 벡터(chord) 계산 (가로 변화량, 세로 변화량)
    // e.g. 출발점A(1,2) 도착점B(5,8) 변위벡터 (5-1, 8-2)
    res.a = a;
    res.b = b;
    res.ga = eval_ast(g, a);
    res.fa = eval_ast(f, a);
    res.gb = eval_ast(g, b);
    res.fb = eval_ast(f, b);
    res.chord_dx = res.gb - res.ga;
    res.chord_dy = res.fb - res.fa;

    // 2단계: 2D 외적(Cross Product)을 이용한 평행 조건 스캐닝
    // 두 벡터 (g'(t), f'(t)) 와 (chord_dx, chord_dy) 가 평행하려면 (평행 = 기울기 일치 = 변화량 일치 => 비례식)
    // 외적값: f'(t) * chord_dx - g'(t) * chord_dy == 0 이어야 함 (0으로 나누기 방지!)
    int steps = 1000;
    double dt = (b - a) / (double)steps;

    for (int i = 0; i < steps; i++) {
        double t1 = a + i * dt;
        double t2 = t1 + dt;

        double cross1 = num_diff_1st(f, t1) * res.chord_dx - num_diff_1st(g, t1) * res.chord_dy;
        double cross2 = num_diff_1st(f, t2) * res.chord_dx - num_diff_1st(g, t2) * res.chord_dy;

        if (isnan(cross1) || isnan(cross2)) continue;

        // 외적의 부호가 반전되었다면(시계방향 회전 -> 반시계방향 회전), 그 사이에 평행 순간 c 존재
        // 연속인 이상 어딘가에선 반드시 회전이 멈출거
        if (cross1 * cross2 <= 0.0 && res.c_count < 32) {
            double left = t1, right = t2, cl = cross1;

            for (int iter = 0; iter < 35; iter++) {
                double mid = 0.5 * (left + right);
                double cm = num_diff_1st(f, mid) * res.chord_dx - num_diff_1st(g, mid) * res.chord_dy;

                if (fabs(cm) < ZERO_EPSILON) {
                    left = right = mid;
                    break;
                }
                if (cl * cm <= 0.0) {
                    right = mid;
                }
                else {
                    left = mid;
                    cl = cm;
                }
            }

            double c = 0.5 * (left + right);

            if (c > a + 1e-5 && c < b - 1e-5) {
                int is_dup = 0;
                for (int k = 0; k < res.c_count; k++) {
                    if (fabs(res.c_vals[k] - c) < 1e-4) {
                        is_dup = 1;
                        break;
                    }
                }

                // 평행 접점 좌표와 그 순간의 속도 벡터 성분 (g'(c), f'(c)) 저장
                if (!is_dup) {
                    int idx = res.c_count++;
                    res.c_vals[idx] = c;
                    res.gc_vals[idx] = eval_ast(g, c);
                    res.fc_vals[idx] = eval_ast(f, c);
                    res.dg_vals[idx] = num_diff_1st(g, c);
                    res.df_vals[idx] = num_diff_1st(f, c);
                }
            }
        }
    }

    return res;
}

//파생함수 VC: 경사하강법
/*※G.D. 안개낀 야산에서 발바닥 경사만 느끼며 하산하는 알고리즘
학습량 (보폭)이 너무 크면 분명히 넘어질 것이고 최악의 경우엔 굴러 떨어질 수도 있음
그렇다고 이게 너무 작으면 느린걸 떠나서, 온 신경을 다리에 집중해야 하니까 에너지는 더욱 빨리 닳음*/

GDTraceResult trace_gradient_descent(
    const ASTNode* f,
    double x0,          // 공을 떨어뜨릴 초기 위치
    double lr,          // 학습률 eta (보폭 크기)
    int max_steps       // 최대 점프 횟수 (최대 127)
) {
    GDTraceResult res;
    memset(&res, 0, sizeof(GDTraceResult));

    if (f == NULL || max_steps <= 0) return res;
    if (max_steps > 127) max_steps = 127;

    double curr_x = x0;
    res.status = 1; // 기본 상태: 최대 스텝까지 진행 중

    for (int i = 0; i < max_steps; i++) {
        double curr_y = eval_ast(f, curr_x);
        double grad = num_diff_1st(f, curr_x); // 현재 발밑의 접선 기울기 f'(x_n)

        // [안전장치] 값이 NaN이거나 계곡 밖으로 너무 멀리 튕겨 나가면 '발산(2)' 판정 후 즉시 중단
        if (isnan(curr_y) || isnan(grad) || fabs(curr_x) > 1e6 || fabs(curr_y) > 1e8) {
            res.status = 2;
            break;
        }

        // 현재 스텝의 위치와 접선 기울기 기록
        res.x_hist[i] = curr_x;
        res.y_hist[i] = curr_y;
        res.grad_hist[i] = grad;
        res.step_count = i + 1;

        // 접선 기울기가 사실상 0이면 골짜기 바닥(극소점)에 안착 완료 -> '수렴(0)' 판정
        if (fabs(grad) < 1e-5) {
            res.secant_hist[i] = 0.0;
            res.status = 0;
            break;
        }

        // [핵심 업데이트 공식] 기울기(grad)의 반대(-) 방향으로 학습률(lr)만큼 이동!
        double next_x = curr_x - lr * grad;
        double next_y = eval_ast(f, next_x);

        // 방금 점프한 구간 [curr_x, next_x]의 실제 평균변화율(할선 기울기) 계산
        // -> 진동/발산 시 출발점 접선(grad)과 실제 평균변화율(secant)의 부호·크기가 어긋나는 걸 포착
        if (fabs(next_x - curr_x) > ZERO_EPSILON && !isnan(next_y)) {
            res.secant_hist[i] = (next_y - curr_y) / (next_x - curr_x);
        }
        else {
            res.secant_hist[i] = grad;
        }

        curr_x = next_x;
    }

    return res;
}

//파생함수 VI: 곡선의 길이

//파생함수 VIA: 호의 길이 추적 (결국 곡선의 길이공식을 대입한 함수)

/*※실제 수능에서는 27,8번 계산문제로 가끔 나올정도로 마이너한 주제지만
 실제 경로와 이동 경로를 비교하거나 이거를 함수로 나타내면 항상 1보다 큰 순증가함수여서
 여러모로 유능하게 사용됨*/

// [내부 도우미] 미소 길이 요소 ds/dx = sqrt(1 + (f'(x))^2) 계산
double eval_arc_speed(const ASTNode* f, double x) {
    double df = num_diff_1st(f, x);
    if (isnan(df)) return NAN;
    return sqrt(1.0 + df * df);
}

// 단일 구간 [a, b]의 정확한 곡선 전체 길이 L 계산 (심프슨 1/3 법칙)
double calc_arc_length(const ASTNode* f, double a, double b, int steps) {
    if (f == NULL || isnan(a) || isnan(b)) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    // a > b 인 경우에도 방향 부호를 살려서 계산 (a < b 이면 양수 길이)
    double sign = 1.0;
    if (a > b) {
        double tmp = a;
        a = b;
        b = tmp;
        sign = -1.0;
    }

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (b - a) / (double)steps;
    double y0 = eval_arc_speed(f, a);
    double yn = eval_arc_speed(f, b);
    if (isnan(y0) || isnan(yn)) return NAN;

    double sum = y0 + yn;

    for (int i = 1; i < steps; i++) {
        double xi = a + i * dx;
        double yi = eval_arc_speed(f, xi);
        if (isnan(yi)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * yi;
    }

    return sign * sum * (dx / 3.0);
}

// [PIP 파생함수 VIA] 시작점 a 기준 누적 곡선 길이 s(x) 및 직선 거리 d(x) 배열 생성
void generate_arc_length_arrays(
    const ASTNode* f,
    double a,           // 길이 측정 시작 기준점 x = a
    double x_min,
    double x_max,
    int count,
    double* out_x,      // 가로축 x좌표 배열
    double* out_s,      // 누적 곡선 길이 s(x) 배열
    double* out_chord   // 시작점 (a, f(a))와 현재 점 (x, f(x)) 사이의 직선 거리 배열
) {
    if (f == NULL || count < 2 || out_x == NULL || out_s == NULL) return;

    double dx = (x_max - x_min) / (double)(count - 1);
    double fa = eval_ast(f, a); // 직선 거리 계산용 기준점 높이 f(a)

    // 1. 가로축 x좌표 및 직선 현의 길이(chord) 채우기
    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        out_x[i] = x;

        if (out_chord != NULL) {
            double fx = eval_ast(f, x);
            if (isnan(fa) || isnan(fx)) {
                out_chord[i] = NAN;
            }
            else {
                double diff_x = x - a;
                double diff_y = fx - fa;
                out_chord[i] = sqrt(diff_x * diff_x + diff_y * diff_y);
            }
        }
    }

    // 2. 기준점 a에 가장 가까운 격자 인덱스 탐색 (파생함수 IB 패턴 1과 동일한 O(N) 양방향 누적)
    int base_idx = (int)round((a - x_min) / dx);
    if (base_idx < 0) base_idx = 0;
    if (base_idx >= count) base_idx = count - 1;

    // 기준점 a와 가장 가까운 격자점 사이의 초기 오프셋 보정
    out_s[base_idx] = calc_arc_length(f, a, out_x[base_idx], 20);

    // 오른쪽(x > a)으로 누적 곡선 길이 적분 (O(N))
    for (int i = base_idx + 1; i < count; i++) {
        double piece = calc_arc_length(f, out_x[i - 1], out_x[i], 4);
        if (isnan(out_s[i - 1]) || isnan(piece)) out_s[i] = NAN;
        else out_s[i] = out_s[i - 1] + piece;
    }

    // 왼쪽(x < a)으로 누적 곡선 길이 적분 (방향이 반대이므로 뺄셈, O(N))
    for (int i = base_idx - 1; i >= 0; i--) {
        double piece = calc_arc_length(f, out_x[i], out_x[i + 1], 4);
        if (isnan(out_s[i + 1]) || isnan(piece)) out_s[i] = NAN;
        else out_s[i] = out_s[i + 1] - piece;
    }
}

// 파생함수 VIB: 곡선의 길이를 N등분하는 좌표 추출 및 굴곡도(Tortuosity) 분석
// 굴곡도(타우): 두 점 사이를 곡선으로 지나가면, 직선으로 돌아가는거에 비해 몇배나 더 걸리는지 비교하는 상수
// 굴곡도 = 곡선의 길이 / 두점 사이의 직선 길이
ALR calc_arc_length_partition(const ASTNode* f, double a, double b, int num_divisions) {
    ALR res;
    memset(&res, 0, sizeof(ALR));

    if (f == NULL || isnan(a) || isnan(b) || fabs(b - a) < ZERO_EPSILON) return res;
    if (a > b) { double tmp = a; a = b; b = tmp; }

    // 분할 개수는 최소 2등분 ~ 최대 64등분으로 제한
    if (num_divisions < 2) num_divisions = 2;
    if (num_divisions > 64) num_divisions = 64;

    res.a = a;
    res.b = b;

    // 1단계: 고해상도(1000구간) 누적 호의 길이 테이블 구축 (VIA 엔진 재활용)
    const int GRID = 1001;
    double x_grid[1001];
    double s_grid[1001];
    generate_arc_length_arrays(f, a, a, b, GRID, x_grid, s_grid, NULL);

    res.total_length = s_grid[GRID - 1];
    if (isnan(res.total_length) || res.total_length <= ZERO_EPSILON) return res;

    // 2단계: 직선 현의 길이 및 굴곡도(우회율) 계산
    double fa = eval_ast(f, a);
    double fb = eval_ast(f, b);
    res.chord_length = sqrt((b - a) * (b - a) + (fb - fa) * (fb - fa));
    res.tortuosity = (res.chord_length > ZERO_EPSILON) ? (res.total_length / res.chord_length) : 1.0;

    // 3단계: 목표 길이 L_k = (k / num_divisions) * L 마다 이진 탐색 + 선형 보간으로 등분할 점 추출
    // ※선형보간: 두 점 사이를 일직선으로 연결해서 중간에 있는 값을 비례식으로 추정하는 수치해석 기법
    // 실제 자를 대고 곡선을 똑같은 길이로 자르기 (똑같은 간격으로 다르면 길이가 들쑥날쑥해짐)
    
    // (양 끝점 a, b를 제외한 내부 분할점 num_divisions - 1개 저장)
    int inner_pts = num_divisions - 1;
    if (inner_pts > 64) inner_pts = 64;
    res.div_count = inner_pts;

    for (int k = 1; k <= inner_pts; k++) {
        double target_s = res.total_length * ((double)k / (double)num_divisions);

        // s_grid는 단조증가 배열이므로 이진 탐색(Binary Search)으로 O(log N) 탐색
        int low = 0, high = GRID - 1;
        while (low + 1 < high) {
            int mid = (low + high) / 2;
            if (s_grid[mid] <= target_s) low = mid;
            else high = mid;
        }

        // [low, high] 구간 내에서 비율(ratio) 기반 선형 보간으로 정밀 x좌표 복원
        double ds = s_grid[high] - s_grid[low];
        double ratio = (ds > 1e-15) ? ((target_s - s_grid[low]) / ds) : 0.0;
        double eq_x = x_grid[low] + ratio * (x_grid[high] - x_grid[low]);

        res.eq_x[k - 1] = eq_x;
        res.eq_y[k - 1] = eval_ast(f, eq_x);
    }

    return res;
}

// 파생함수 VIC: 매개변수 곡선 (g(t), f(t)) 의 순간 속력 v(t) 및 누적 이동 거리 s(t) 추적

// [내부 도우미] 매개변수 곡선 순간 속력 v(t) = sqrt((g'(t))^2 + (f'(t))^2)
double eval_param_speed(const ASTNode* f, const ASTNode* g, double t) {
    double dg = num_diff_1st(g, t);
    double df = num_diff_1st(f, t);
    if (isnan(dg) || isnan(df)) return NAN;
    return sqrt(dg * dg + df * df);
}

// 단일 매개변수 구간 [t1, t2]의 곡선 궤적 길이 계산 (심프슨 1/3 법칙)
double calc_param_arc_length(const ASTNode* f, const ASTNode* g, double t1, double t2, int steps) {
    if (f == NULL || g == NULL || isnan(t1) || isnan(t2)) return NAN;
    if (fabs(t2 - t1) < 1e-15) return 0.0;

    double sign = 1.0;
    if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; sign = -1.0; }

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dt = (t2 - t1) / (double)steps;
    double v0 = eval_param_speed(f, g, t1);
    double vn = eval_param_speed(f, g, t2);
    if (isnan(v0) || isnan(vn)) return NAN;

    double sum = v0 + vn;
    for (int i = 1; i < steps; i++) {
        double ti = t1 + i * dt;
        double vi = eval_param_speed(f, g, ti);
        if (isnan(vi)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * vi;
    }

    return sign * sum * (dt / 3.0);
}

// [PIP 파생함수 VIC] 매개변수 t에 따른 궤적 좌표 (x(t), y(t)), 순간 속력 v(t), 누적 이동 거리 s(t) 생성
void generate_param_arc_arrays(
    const ASTNode* f,   // y(t) 수식 트리
    const ASTNode* g,   // x(t) 수식 트리
    double t_min,
    double t_max,
    int count,
    double* out_t,      // 매개변수 t 배열
    double* out_x,      // 궤적 x(t) = g(t) 배열
    double* out_y,      // 궤적 y(t) = f(t) 배열
    double* out_speed,  // 순간 속력 v(t) = sqrt(g'(t)^2 + f'(t)^2) 배열
    double* out_s       // 누적 이동 궤적 길이 s(t) = ∫_{t_min}^t v(u) du 배열
) {
    if (f == NULL || g == NULL || count < 2 || out_t == NULL) return;

    double dt = (t_max - t_min) / (double)(count - 1);
    double running_s = 0.0;

    for (int i = 0; i < count; i++) {
        double t = t_min + i * dt;
        out_t[i] = t;

        if (out_x)     out_x[i] = eval_ast(g, t);
        if (out_y)     out_y[i] = eval_ast(f, t);
        if (out_speed) out_speed[i] = eval_param_speed(f, g, t);

        if (out_s) {
            if (i == 0) {
                out_s[0] = 0.0;
            }
            else {
                double piece = calc_param_arc_length(f, g, out_t[i - 1], t, 4);
                if (isnan(running_s) || isnan(piece)) running_s = NAN;
                else running_s += piece;
                out_s[i] = running_s;
            }
        }
    }
}


// 파생함수 VII: 뉴턴-랩슨법(Newton-Raphson) 교점 탐색 및 접선 궤적 추적
// [내부 도우미] 차이 함수 h(x) = f(x) - g(x) 계산 (g가 NULL이면 단일 함수 f(x) = 0 모드)
double eval_diff_func(const ASTNode* f, const ASTNode* g, double x) {
    double fx = eval_ast(f, x);
    if (isnan(fx)) return NAN;
    if (g == NULL) return fx;
    double gx = eval_ast(g, x);
    if (isnan(gx)) return NAN;
    return fx - gx;
}

// [내부 도우미] 차이 함수의 도함수 h'(x) = f'(x) - g'(x) 수치 계산
double eval_diff_deriv(const ASTNode* f, const ASTNode* g, double x) {
    double df = num_diff_1st(f, x);
    if (isnan(df)) return NAN;
    if (g == NULL) return df; 
    double dg = num_diff_1st(g, x);
    if (isnan(dg)) return NAN;
    return df - dg;
}
// g == NULL 이면 방정식 f(x) = 0 의 근 탐색
// g != NULL 이면 두 곡선 f(x) = g(x) 의 교점 탐색 (예: 2^x = x + 2)
NewtonTrace trace_newton_raphson(
    const ASTNode* f,
    const ASTNode* g,   // NULL 허용 (단일 방정식 f(x)=0 모드)
    double x0,          // 사용자가 슬라이더로 지정하는 초기 추정값
    int max_iter        // 최대 반복 횟수 (최대 63)
) {
    NewtonTrace res;
    memset(&res, 0, sizeof(NewtonTrace)); //memory reset: NewtonTrace라는 빵틀을 빡빡 설거지해서 비우는 함수
    res.root = NAN; 

    if (f == NULL || isnan(x0) || max_iter <= 0) {
        res.status = 2;
        return res;
    }
    if (max_iter > 63) max_iter = 63;

    double curr_x = x0;
    res.status = 1; // 기본값: 최대 반복 횟수 도달

    for (int i = 0; i < max_iter; i++) { //f,g,h,h' 세팅
        double fx = eval_ast(f, curr_x);
        double gx = (g != NULL) ? eval_ast(g, curr_x) : 0.0;
        double hx = eval_diff_func(f, g, curr_x);
        double dhx = eval_diff_deriv(f, g, curr_x);

        // 정의역 이탈(NaN) 또는 좌표 폭주 시 발산 판정
        if (isnan(hx) || isnan(dhx) || fabs(curr_x) > 1e7 || fabs(hx) > 1e9) {
            res.status = 2;
            res.root = curr_x;
            break;
        }

        // 현재 스텝 상태 기록
        res.x_hist[i] = curr_x;
        res.fx_hist[i] = fx;
        res.gx_hist[i] = gx;
        res.hx_hist[i] = hx;
        res.slope_hist[i] = dhx;
        res.iter_count = i + 1;
        res.root = curr_x;

        // [수렴 판정] |h(x_n)| < 1e-9 이면 교점(근) 도달 완료!
        if (fabs(hx) < ZERO_EPSILON) {
            res.status = 0;
            break;
        }

        // [특이점 발산 판정] 접선의 기울기 h'(x_n)이 0에 너무 가까우면 x절편이 무한대로 발산!
        if (fabs(dhx) < ZERO_EPSILON) {
            res.status = 2;
            break;
        }

        // [핵심 뉴턴-랩슨 업데이트] 접선의 x절편: x_{n+1} = x_n - h(x_n) / h'(x_n)
        double next_x = curr_x - (hx / dhx);

        // 보폭이 극도로 작아져 정밀도 한계에 도달한 경우도 수렴으로 인정
        if (fabs(next_x - curr_x) < 1e-11 && fabs(hx) < 1e-6) {
            res.status = 0;
            res.root = next_x;
            break;
        }

        curr_x = next_x;
    }

    return res;
}


// 파생함수 VIII:테일러급수로 초월함수 근사 (최대 14차) & 잔여 오차 추적
TaylorResult generate_taylor_approx_arrays(
    const ASTNode* f,
    double a,           // 전개 기준점 x = a
    int degree,         // 다항식 차수 n (0 ~ MAX_TAYLOR_DEGREE)
    double x_min,
    double x_max,
    int count,
    double* out_x,      // 가로축 x좌표 배열
    double* out_poly,   // n차 테일러 다항식 P_n(x) 배열
    double* out_error   // 잔여 오차 |R_n(x)| = |f(x) - P_n(x)| 배열
) 
{
    TaylorResult res;
    memset(&res, 0, sizeof(TaylorResult));

    if (f == NULL || isnan(a) || count < 2) return res;
    if (degree < 0) degree = 0;
    if (degree > MAX_TAYLOR_DEGREE) degree = MAX_TAYLOR_DEGREE;

    res.center_a = a;
    res.degree = degree;

    // 1단계: 0차부터 degree차까지 각 차수에 맞는 차분 계수 빵틀(Stencil)을 조립해 도함수 산출
    double fact = 1.0;
    for (int k = 0; k <= degree; k++) {
        FDS st = build_central_diff_stencil(k);
        res.derivs[k] = apply_diff_stencil(f, a, &st);

        if (k > 0) fact *= (double)k;
        res.coeffs[k] = res.derivs[k] / fact;
    }

    // 2단계: 격자 전체에 대해 근사 곡선 P_n(x) 및 잔여 오차 |R_n(x)| 계산
    double dx = (x_max - x_min) / (double)(count - 1);
    res.max_error = 0.0;

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        if (out_x) out_x[i] = x;

        double fx = eval_ast(f, x);
        double px = isnan(res.coeffs[0]) ? NAN : eval_taylor_poly(&res, x);
        double err = (isnan(fx) || isnan(px)) ? NAN : fabs(fx - px);

        if (out_poly)  out_poly[i] = px;
        if (out_error) out_error[i] = err;

        if (!isnan(err) && err > res.max_error) {
            res.max_error = err;
        }
    }

    return res;
}
// [빵틀 조립기] 테일러 급수의 표현한도를 정하는 함수, 이를테면 1로 설정하면 0차 + 1차, 10으로 설정하면 상수부터 10차까지 깡그리 더해서 표현
/*1. 다항식 (x-1)^k을 전개했을때 나오는 계수들을 모아서 가중치로 재활용
    2. 그 가중치들을 기준점 주변의 함숫값에 각각 더해서 1/(h^k)곱 
    3. k번 미분한 식 출력*/
FDS build_central_diff_stencil(int order) {
    FDS st;
    memset(&st, 0, sizeof(FDS));

    if (order < 0) order = 0;
    if (order > MAX_TAYLOR_DEGREE) order = MAX_TAYLOR_DEGREE;

    st.order = order;

    if (order == 0) {
        st.radius = 0;
        st.step_h = 0.0;
        st.weights[0] = 1.0;
        return st;
    }

    // 차수가 높아질수록 분모 h^k 의 부동소수점 언더플로우를 막기 위해 보폭 h를 동적으로 키움
    // 현미경에서 생물을 관찰할때 같은 배율로 관찰하면 작은 생물은 안보이고, 큰 생물은 이게 생물인지 아닌지 구별 못하는거랑 같은 원리
    // (k=1일 때 ~1e-5, k=4일 때 ~0.01, k=10일 때 ~0.08, k=14일 때 ~0.14)
    st.step_h = pow(1e-15, 1.0 / (double)(order + 2));

    if (order % 2 == 0) {
        // [짝수 차수 k = 2m] 대칭 반경 R = k / 2, 총 k + 1개 점 사용
        // 공식: f^(k)(a) ≈ (1 / h^k) * Σ_{j=0}^{k} (-1)^j * C(k, j) * f(a + (k/2 - j)h)
      
        // e.g. k = 2 -> [1, -2, 1] k = 3 -> [1, -4, 6, -4, 1] 
        // 위의 가중치를 가운데 식 f(a)를 기준으로 h만큼 a를 옮긴 함숫값에다가 가중치를 곱함
        int R = order / 2;
        st.radius = R;

        double comb = 1.0; // 이항계수 C(order, j)
        for (int j = 0; j <= order; j++) {
            if (j > 0) comb = comb * (double)(order - j + 1) / (double)j;
            double sign = (j % 2 == 0) ? 1.0 : -1.0;
            // 배열 인덱스 idx = (R - j) + R = order - j
            st.weights[order - j] = sign * comb;
        }
    }
    else {
        // [홀수 차수 k = 2m + 1] 대칭성을 맞추기 위해 반경 R = (k + 1) / 2, 총 k + 2개 점 사용
        // 공식: (k-1)차 중앙 차분에 1차 중앙 차분 (f(x+h) - f(x-h)) / (2h) 를 합성(Convolution)
        // 홀수 차수는 공식을 적용할 수 없어서 어떤 식을 곱한 다음 짝수 차수로 만듦
        int m = order - 1; // 짝수 차수
        int R = (order + 1) / 2;
        st.radius = R;

        double base_comb[32] = { 0 };
        double comb = 1.0;
        for (int j = 0; j <= m; j++) {
            if (j > 0) comb = comb * (double)(m - j + 1) / (double)j;
            double sign = (j % 2 == 0) ? 1.0 : -1.0;
            base_comb[m - j] = sign * comb;
        }

        // 컨볼루션 합성: 짝수번 미분한 값에다가 처음 식을 곱해서 홀수번 미분한 식으로 만드는 과정 
        // 항의 개수는 홀수개가 되고, 파스칼의 삼각형을 따라갈텐데 좌우대칭 구조가 되어서 다루기 편해짐
        for (int i = 0; i <= m + 2; i++) {
            double right_term = (i <= m) ? base_comb[i] : 0.0;
            double left_term = (i >= 2 && i - 2 <= m) ? base_comb[i - 2] : 0.0;
            st.weights[i] = 0.5 * (right_term - left_term);
        }
    }

    return st;
}

// [빵틀 실행기] 진짜로 k번 미분한 값을 토해내는 함수
double apply_diff_stencil(const ASTNode* f, double a, const FDS* st) {
    if (f == NULL || st == NULL || isnan(a)) return NAN;
    if (st->order == 0) return eval_ast(f, a);

    double sum = 0.0;
    int R = st->radius;
    int total_pts = 2 * R + 1;

    for (int idx = 0; idx < total_pts; idx++) {
        if (fabs(st->weights[idx]) < 1e-18) continue; // 가중치가 0인 중앙점은 평가 생략
        int offset = idx - R; // -R ~ +R
        double y = eval_ast(f, a + offset * st->step_h);
        if (isnan(y)) return NAN;
        sum += st->weights[idx] * y;
    }

    return sum / pow(st->step_h, (double)st->order);
}

// [호너 평가기] 임의 차수 테일러 다항식 P_n(x) 고속 계산

/*※호너 평가기: 다항식을 상수 + 계수 * 뭐시깽이 형태로 고치는 마법
다항식을 그냥 계산하면 2000개의 포인터를 갖고있는 데이터 하나 집어넣을때마다 
pow함수를 겁나게 넣어야 하는데, 얘를 쓰면 곱셈 몇번 덧셈 몇번으로 마법같이 해결*/
double eval_taylor_poly(const TaylorResult* tr, double x) {
    double dx = x - tr->center_a;
    double sum = tr->coeffs[tr->degree];
    for (int k = tr->degree - 1; k >= 0; k--) {
        sum = sum * dx + tr->coeffs[k];
    }
    return sum;
}


//파생함수IX: 역함수의 기하적 투영 & 역함수 미분법 발동

// [PIP 파생함수 IX-1] 원함수 격자를 기반으로 역함수 대칭 곡선 좌표 (inv_x, inv_y) 및 도함수 역수 배열 생성
// 원함수 점 (x, f(x)) -> 역함수 점 (f(x), x) 로 투영
// 반환값: 구간 내 일대일대응(단조성) 여부 (1: 단조함수, 0: 극값 포함 비단조)
int generate_inverse_curve_arrays(
    const ASTNode* f,
    double x_min,
    double x_max,
    int count,
    double* out_orig_x,   // 원함수 x좌표 배열
    double* out_orig_y,   // 원함수 y = f(x) 배열
    double* out_inv_x,    // 역함수 x좌표 배열 (= f(x))
    double* out_inv_y,    // 역함수 y좌표 배열 (= x)
    double* out_inv_dy    // 역함수 도함수 배열 (= 1 / f'(x), PIP 서브캔버스용)
) {
    if (f == NULL || count < 2) return 0;

    double dx = (x_max - x_min) / (double)(count - 1);
    int pos_cnt = 0; // f'(x) > 0 인 격자 수
    int neg_cnt = 0; // f'(x) < 0 인 격자 수

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        double y = eval_ast(f, x);
        double dy = num_diff_1st(f, x);

        if (out_orig_x) out_orig_x[i] = x; 
        if (out_orig_y) out_orig_y[i] = y;

        // y = x 대칭 투영: (x, y) -> (y, x)
        if (out_inv_x) out_inv_x[i] = y;
        if (out_inv_y) out_inv_y[i] = x;

        // 역함수 미분법: (f^-1)'(f(x)) = 1 / f'(x)
        if (out_inv_dy) {
            if (isnan(dy) || fabs(dy) < ZERO_EPSILON) {
                out_inv_dy[i] = NAN; // 원함수 기울기가 0이면 역함수는 수직 접선(미분불가)
            } else {
                out_inv_dy[i] = 1.0 / dy;
            }
        }

        // 단조성 판정을 위한 도함수 부호 카운트
        if (!isnan(dy) && fabs(dy) >= ZERO_EPSILON) {
            if (dy > 0.0) pos_cnt++;
            else neg_cnt++;
        }
    }

    // 구간 전체에서 증가만 하거나 감소만 해야 일대일대응(역함수 존재) 성립
    if ((pos_cnt > 0 && neg_cnt == 0) || (neg_cnt > 0 && pos_cnt == 0)) {
        return 1;
    }
    return 0;
}

// [슬라이더 연동 IX-2] 특정 접점 t에서 원함수 접선과 역함수 대칭 접선 정보 동시 추출
IDI calc_inverse_derivative_at(const ASTNode* f, double t, double x_min, double x_max) {
    IDI info;
    memset(&info, 0, sizeof(IDI));

    if (f == NULL || isnan(t)) return info;

    info.t = t;
    info.ft = eval_ast(f, t);
    info.df_t = num_diff_1st(f, t);

    // 원함수 접선의 y절편
    if (!isnan(info.ft) && !isnan(info.df_t)) {
        info.orig_tan_y_int = info.ft - t * info.df_t;
    } else {
        info.orig_tan_y_int = NAN;
    }

    // 역함수 대칭점 (f(t), t)에서의 접선 기울기 = 1 / f'(t) 및 y절편 계산
    if (isnan(info.df_t) || fabs(info.df_t) < ZERO_EPSILON) {
        info.inv_df_ft = NAN;
        info.inv_tan_y_int = NAN;
    } else {
        info.inv_df_ft = 1.0 / info.df_t;
        // 기울기가 1/f'(t) 이고 점 (f(t), t)를 지나는 직선의 y절편: y - t = (1/f'(t))(x - f(t))
        info.inv_tan_y_int = t - info.ft * info.inv_df_ft;
    }

    // 구간 내 단조성 방향 판정 (샘플 200개 기준 빠른 체크)
    int pos = 0, neg = 0;
    double step = (x_max - x_min) / 200.0;
    for (int i = 0; i <= 200; i++) {
        double d = num_diff_1st(f, x_min + i * step);
        if (!isnan(d) && fabs(d) >= ZERO_EPSILON) {
            if (d > 0.0) pos++;
            else neg++;
        }
    }

    if (pos > 0 && neg == 0) {
        info.is_monotonic = 1;
        info.monotone_dir = 1;
    } else if (neg > 0 && pos == 0) {
        info.is_monotonic = 1;
        info.monotone_dir = -1;
    } else {
        info.is_monotonic = 0;
        info.monotone_dir = 0;
    }

    return info;
}

// 파생함수 X : 곡률(kappa)과 접촉원(Osculating Circle) 추적
// ※곡률: 운전하다 커브를 꺾을때 핸들의 꺾임 정도, 접촉원: 꺾인 상태로 한바퀴 돌때 나오는 원

// [슬라이더 연동 X-1] 특정 점 t에서의 곡률 κ(t), 곡률반지름 R(t), 접촉원 중심 (center_x, center_y) 계산
//특정 점 t에서 원 하나를 그리는 func
// 1. 곡률 κ(t) = |f''(t)| / (1 + f'(t)^2)^(3/2) : 2계 미분(휘어짐)을 기울기 길이로 보정
// 2. 변곡점 방어: f''(t) ≈ 0 이면 직선 구간이므로 반지름 R = 1/κ 가 무한대로 발산 -> NAN 처리
// 3. 접촉원 중심 (cx, cy): 접점 (t, f(t))에서 접선의 수직(법선) 안쪽으로 반지름 R만큼 이동한 좌표
OscCircle calc_osculating_circle_at(const ASTNode* f, double t) {
    OscCircle res;
    memset(&res, 0, sizeof(OscCircle));

    if (f == NULL || isnan(t)) return res;

    res.t = t;
    res.ft = eval_ast(f, t);
    double df = num_diff_1st(f, t);
    double d2f = num_diff_2nd(f, t);

    if (isnan(res.ft) || isnan(df) || isnan(d2f)) { //예외처리
        res.curvature = NAN;
        res.radius = NAN;
        res.center_x = NAN;
        res.center_y = NAN;
        return res;
    }

    // 1. 곡률 κ(t) = |f''(t)| / (1 + f'(t)^2)^(3/2)
    double norm_factor = 1.0 + df * df; // 1 + (f'(t))^2
    double denom = pow(norm_factor, 1.5);
    res.curvature = fabs(d2f) / denom;

    // 2. 변곡점(f''(t) ≈ 0)이면 직선과 같으므로 접촉원 반지름과 중심이 무한대로 발산(NAN 처리)
    if (fabs(d2f) < ZERO_EPSILON) {
        res.radius = NAN;
        res.center_x = NAN;
        res.center_y = NAN;
    }
    else {
        res.radius = 1.0 / res.curvature;
        // 법선 방향(오목한 안쪽)으로 R만큼 이동한 접촉원의 중심 좌표
        res.center_x = t - (df * norm_factor) / d2f;
        res.center_y = res.ft + norm_factor / d2f;
    }

    return res;
} 

// [PIP 파생함수 X-2] 실제 캔버스로 그리기 위한 구간 전체의 곡률 함수 κ(x) & 곡률반지름 R(x) 배열 생성
void generate_curvature_array(
    const ASTNode* f,
    double x_min,
    double x_max,
    int count,
    double* out_x,          // 가로축 x좌표 배열
    double* out_curvature,  // 곡률 κ(x) 배열 (PIP 서브캔버스용)
    double* out_radius      // 곡률반지름 R(x) 배열 (변곡점 부근은 NAN)
) {
    if (f == NULL || count < 2) return;

    double dx = (x_max - x_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double x = x_min + i * dx;
        if (out_x) out_x[i] = x;

        double df = num_diff_1st(f, x);
        double d2f = num_diff_2nd(f, x);

        if (isnan(df) || isnan(d2f)) {
            if (out_curvature) out_curvature[i] = NAN;
            if (out_radius)    out_radius[i] = NAN;
            continue;
        }

        double kappa = fabs(d2f) / pow(1.0 + df * df, 1.5);
        if (out_curvature) out_curvature[i] = kappa;

        if (out_radius) {
            out_radius[i] = (kappa < ZERO_EPSILON) ? NAN : (1.0 / kappa);
        }
    }
}