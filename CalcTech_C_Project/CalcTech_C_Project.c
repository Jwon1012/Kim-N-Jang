#include "CalcTech공용두꺼비집.h"

#pragma region AST & LaTeX변환
// 1. 문자열 수식 -> AST 트리 변환 (토큰 메모리 자동 정리 포함)
CALCTECH_API ASTNode* calctech_parse_formula(const char* expr) {
    if (expr == NULL || expr[0] == '\0') return NULL;

    TokenList tokens = tokenize(expr);
    Parser parser = { &tokens, 0 };
    ASTNode* root = parse_expression(&parser);

    free_token_list(&tokens);
    return root;
}

// 2. AST 트리 메모리 해제
CALCTECH_API void calctech_free_formula(ASTNode* root) {
    if (root != NULL) free_ast(root);}

// 3. 문자열 수식 -> LaTeX 문자열 즉시 변환
CALCTECH_API int calctech_formula_to_latex(const char* expr, char* out_buf, int max_len) {
    if (expr == NULL || out_buf == NULL || max_len <= 0) return;

    out_buf[0] = '\0';
    ASTNode* root = calctech_parse_formula(expr);
    if (root != NULL) {
        ast_to_latex(root, out_buf, (size_t)max_len);
        free_ast(root);
    }
}
#pragma endregion

#pragma region 모듈 IV 육십_fps 메인/서브 캔버스 데이터 패킹 브릿지

// 파이썬이 전달한 버퍼 포인터(NULL 허용)에 맞춰 곡선 및 미적분 시계열 데이터를 채우는 내부 도우미
// 박스에다가 내용물을 집어넣는 함수
static void fill_canvas_series(
    const ASTNode* f_root,
    const ASTNode* g_root,
    double x_min,
    double dx,
    int n,
    double* out_x,
    double* out_main_y,
    double* out_g_y,
    double* out_sub_dy,
    double* out_sub_d2y,
    double* out_sub_int
) {
    double running_area = 0.0;

    for (int i = 0; i < n; i++) {
        double x = x_min + dx * (double)i;
        bool is_hole = false;
        double y = eval_with_singularity_guard(f_root, x, &is_hole);

        out_x[i] = x;
        out_main_y[i] = is_hole ? NAN : y;

        if (out_g_y != NULL) {
            if (g_root != NULL) {
                bool g_hole = false;
                double gy = eval_with_singularity_guard(g_root, x, &g_hole);
                out_g_y[i] = g_hole ? NAN : gy;
            }
            else {
                out_g_y[i] = NAN;
            }
        }

        if (out_sub_dy != NULL) {
            out_sub_dy[i] = num_diff_1st(f_root, x);
        }

        if (out_sub_d2y != NULL) {
            out_sub_d2y[i] = num_diff_2nd(f_root, x);
        }

        if (out_sub_int != NULL) {
            if (i == 0) {
                out_sub_int[0] = 0.0;
            }
            else {
                double x_prev = out_x[i - 1];
                double y_prev = out_main_y[i - 1];
                double y_curr = out_main_y[i];
                double y_mid = eval_ast(f_root, 0.5 * (x_prev + x));

                // 불연속 발산 구간은 누적 면적 폭주를 막기 위해 적분 누적에서 제외
                if (!isnan(y_prev) && !isnan(y_mid) && !isnan(y_curr)) {
                    running_area += (dx / 6.0) * (y_prev + 4.0 * y_mid + y_curr);
                }
                out_sub_int[i] = running_area;
            }
        }
    }
}

// 사용하지 않는 g_root 또는 출력 배열 포인터에는 NULL(Python None) 전달 가능
CALCTECH_API int calctech_fill_dual_canvas(
    const ASTNode* f_root,                       // 주함수 f(x) AST (필수)
    const ASTNode* g_root,                      // 보조함수 g(x) AST (없으면 NULL)
    double x_min,                                    // 현재 뷰포트 X 시작점
    double x_max,                                   // 현재 뷰포트 X 끝점
    int n,                                                 // Python NumPy 배열 길이 (샘플 수)
    const ScaleOption* scale_opt,             // 축 스케일링 옵션 (NULL이면 기본값 적용)
    double* out_x,                                   // [출력] 공통 X좌표 배열
    double* out_main_y,                          // [출력] 메인 캔버스 f(x) 배열
    double* out_g_y,                               // [출력] 메인 캔버스 g(x) 배열 (NULL 허용)
    double* out_sub_dy,                           // [출력] 서브 캔버스 1계도함수 f'(x) 배열 (NULL 허용)
    double* out_sub_d2y,                        // [출력] 서브 캔버스 2계도함수 f''(x) 배열 (NULL 허용)
    double* out_sub_int,                         // [출력] 서브 캔버스 누적 정적분 배열 (NULL 허용)
    DCM* out_meta                                // [출력] 그래프 요약 송장 (필수)
) {
    // 1. 필수 입력 및 출력 버퍼 검증
    if (f_root == NULL || out_x == NULL || out_main_y == NULL || out_meta == NULL) {
        return -1;
    }
    if (n < 2 || x_max <= x_min) {
        return -2;
    }

    memset(out_meta, 0, sizeof(DualCanvasMeta));

    // 2. X 격자 및 메인/서브 캔버스 시계열 데이터 채우기
    double dx = (x_max - x_min) / (double)(n - 1);
    fill_canvas_series(
        f_root, g_root, x_min, dx, n,
        out_x, out_main_y, out_g_y, out_sub_dy, out_sub_d2y, out_sub_int
    );

    out_meta->valid_count = n;
    out_meta->dx = dx;

    // 3. 메인 캔버스(f, g) 동적 축 스케일링
    ScaleOption opt;
    if (scale_opt != NULL) {
        opt = *scale_opt;
    }
    else {
        opt.scale_target = SCALE_AUTO_UNION;
        opt.x_unit_mode = X_UNIT_AUTO;
        opt.include_origin_y = 1;
        opt.target_ticks = 8;
    }

    out_meta->main_axis = compute_dynamic_axis_scale(
        f_root, g_root, x_min, x_max,
        out_main_y, out_g_y, n, &opt
    );

    // 4. 서브 캔버스(도함수/적분 중 활성화된 버퍼 기준) 축 스케일링
    double* sub_primary = (out_sub_dy != NULL) ? out_sub_dy :
        (out_sub_int != NULL) ? out_sub_int : out_sub_d2y;
    if (sub_primary != NULL) {
        ScaleOption sub_opt = opt;
        sub_opt.scale_target = SCALE_FOLLOW_F;
        out_meta->sub_axis = compute_dynamic_axis_scale(
            NULL, NULL, x_min, x_max,
            sub_primary, NULL, n, &sub_opt
        );
    }
    else {
        out_meta->sub_axis = out_meta->main_axis;
    }

    // 5. 일반 특징점(절편, 극점, 변곡점, 교점, 구멍) 및 주기 추출
    out_meta->keypoint_count = extract_graph_keypoints(
        f_root, g_root, x_min, x_max,
        out_meta->keypoints, MAX_KEY_POINTS
    );

    out_meta->period = detect_function_period(
        f_root, 20.0,
        out_meta->period_label, sizeof(out_meta->period_label)
    );

    return 0;
}
#pragma endregion
