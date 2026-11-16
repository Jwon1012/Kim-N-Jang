#include "CalcTech공용두꺼비집.h"
//모듈 IIB: 파생함수 10가지 기능 규현

//파생함수I: 두 함수 사이의 넓이함수

// 접선 정보 구조체 생성 및 접선의 방정식 계산: g_t(x) = f'(t)(x - t) + f(t)
TangentInfo calc_tangent_info(const ASTNode* f, double t) {
    TangentInfo info;
    info.t = t;
    info.ft = eval_ast(f, t);
    info.slope = num_diff_1st(f, t);

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
// 곡선과 접선의 교점 자동 탐색기 (스캔 + 이분법 + 접점 t 강제 삽입)
int find_curve_tangent_intersections(
    const ASTNode* f,
    TangentInfo tan_info,
    double x_min,
    double x_max,
    double* out_roots,
    int max_roots
) {
    if (f == NULL || out_roots == NULL || max_roots <= 0) return 0;

    double temp_roots[MAX_INTERSECTIONS];
    int count = 0;

    // (1) 접점 t 자체가 화면 범위 안에 있다면 확정 교점으로 먼저 등록
    if (tan_info.t >= x_min && tan_info.t <= x_max) {
        temp_roots[count++] = tan_info.t;
    }

    // (2) 구간을 1000등분해 부호가 바뀌는 교점 후보 탐색 후 이분법(Bisection) 정밀 추적
    int scan_steps = 1000;
    double dx = (x_max - x_min) / (double)scan_steps;

    for (int i = 0; i < scan_steps && count < MAX_INTERSECTIONS; i++) {
        double a = x_min + i * dx;
        double b = a + dx;

        double ha = eval_ast(f, a) - eval_tangent(tan_info, a);
        double hb = eval_ast(f, b) - eval_tangent(tan_info, b);

        if (isnan(ha) || isnan(hb)) continue;

        // 부호가 반대인 경우 이분법 40회 반복 (정밀도 약 10^-12)
        if (ha * hb <= 0.0) {
            double left = a, right = b;
            double h_left = ha;

            for (int iter = 0; iter < 40; iter++) {
                double mid = 0.5 * (left + right);
                double h_mid = eval_ast(f, mid) - eval_tangent(tan_info, mid);
                if (fabs(h_mid) < ZERO_EPSILON) {
                    left = right = mid;
                    break;
                }
                if (h_left * h_mid <= 0.0) {
                    right = mid;
                }
                else {
                    left = mid;
                    h_left = h_mid;
                }
            }

            double root = 0.5 * (left + right);

            // 이미 등록된 교점(특히 접점 t)과 1e-4 이내로 겹치면 중복 제외
            int is_duplicate = 0;
            for (int k = 0; k < count; k++) {
                if (fabs(temp_roots[k] - root) < 1e-4) {
                    is_duplicate = 1;
                    break;
                }
            }

            if (!is_duplicate) {
                temp_roots[count++] = root;
            }
        }
    }

    // (3) 교점 오름차순 정렬 (삽입 정렬)
    for (int i = 1; i < count; i++) {
        double key = temp_roots[i];
        int j = i - 1;
        while (j >= 0 && temp_roots[j] > key) {
            temp_roots[j + 1] = temp_roots[j];
            j--;
        }
        temp_roots[j + 1] = key;
    }

    int final_count = (count < max_roots) ? count : max_roots;
    for (int i = 0; i < final_count; i++) {
        out_roots[i] = temp_roots[i];
    }
    return final_count;
}

// 3. 심프슨 1/3 법칙 기반 두 그래프 사이의 넓이 적분: int_a^b |f(x) - g(x)| dx
double integrate_abs_diff_two_funcs(const ASTNode* f, const ASTNode* g, double a, double b, int steps) {
    if (f == NULL || g == NULL || isnan(a) || isnan(b)) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (b - a) / (double)steps;
    double sum = fabs(eval_ast(f, a) - eval_ast(g, a)) + fabs(eval_ast(f, b) - eval_ast(g, b));

    for (int i = 1; i < steps; i++) {
        double xi = a + i * dx;
        double diff = fabs(eval_ast(f, xi) - eval_ast(g, xi));
        if (isnan(diff)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * diff;
    }

    return sum * (dx / 3.0);
}

double integrate_abs_diff_tangent(const ASTNode* f, TangentInfo tan_info, double a, double b, int steps) {
    if (f == NULL || isnan(a) || isnan(b)) return NAN;
    if (fabs(b - a) < 1e-15) return 0.0;

    if (steps < 2) steps = 2;
    if (steps % 2 != 0) steps++;

    double dx = (b - a) / (double)steps;
    double sum = fabs(eval_ast(f, a) - eval_tangent(tan_info, a))
        + fabs(eval_ast(f, b) - eval_tangent(tan_info, b));

    for (int i = 1; i < steps; i++) {
        double xi = a + i * dx;
        double diff = fabs(eval_ast(f, xi) - eval_tangent(tan_info, xi));
        if (isnan(diff)) return NAN;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * diff;
    }

    return sum * (dx / 3.0);
}

// ============================================================================
// 4. [PIP 파생함수 공장] 접점 t 변화에 따른 선택 영역 넓이 함수 S(t) 배열 생성
// ============================================================================
void generate_tangent_area_S(
    const ASTNode* f,
    double t_min,
    double t_max,
    double x_min,
    double x_max,
    int region_idx,
    int count,
    double* out_t,
    double* out_S
) {
    if (f == NULL || count < 2) return;

    double dt = (t_max - t_min) / (double)(count - 1);

    for (int i = 0; i < count; i++) {
        double t = t_min + i * dt;
        if (out_t) out_t[i] = t;

        if (out_S) {
            TangentInfo tan_info = calc_tangent_info(f, t);

            // 현재 t에서의 교점 찾기
            double roots[MAX_INTERSECTIONS];
            int root_cnt = find_curve_tangent_intersections(f, tan_info, x_min, x_max, roots, MAX_INTERSECTIONS);

            // 전체 경계점 배열 조립: [x_min, root_0, root_1, ..., x_max]
            double bounds[MAX_INTERSECTIONS + 2];
            int b_cnt = 0;
            bounds[b_cnt++] = x_min;
            for (int r = 0; r < root_cnt; r++) {
                if (roots[r] > x_min + 1e-6 && roots[r] < x_max - 1e-6) {
                    bounds[b_cnt++] = roots[r];
                }
            }
            bounds[b_cnt++] = x_max;

            // 사용자가 선택한 region_idx 구역이 존재하면 해당 구간 넓이 계산
            if (region_idx >= 0 && region_idx < b_cnt - 1) {
                out_S[i] = integrate_abs_diff_tangent(f, tan_info, bounds[region_idx], bounds[region_idx + 1], 200);
            }
            else {
                out_S[i] = NAN; // 해당 t 위치에서 선택한 구역이 사라진 경우 빈칸 처리
            }
        }
    }
}