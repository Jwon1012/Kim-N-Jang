#include "CalcTech공용두꺼비집.h"
#pragma region 첫번째 모듈 원스톱 파싱 및 LaTeX 변환 래퍼

CALCTECH_API ASTNode* calctech_parse_formula(const char* expr) {
    if (expr == NULL || expr[0] == '\0') return NULL;

    TokenList tokens = tokenize(expr);
    Parser parser = { &tokens, 0 };
    ASTNode* root = parse_expression(&parser);

    free_token_list(&tokens);
    return root;
}

CALCTECH_API void calctech_free_formula(ASTNode* root) {
    if (root != NULL) free_ast(root); }

CALCTECH_API int calctech_formula_to_latex(const char* expr, char* out_buf, int max_len) {
    if (expr == NULL || out_buf == NULL || max_len <= 0) return -1;

    out_buf[0] = '\0';
    ASTNode* root = calctech_parse_formula(expr);
    if (root == NULL) return -2;

    ast_to_latex(root, out_buf, (size_t)max_len);
    free_ast(root);
    return 0;
}

#pragma endregion

#pragma region II. 모듈 IV 육십_fps 메인/서브 캔버스 데이터 패킹 브릿지

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

#pragma region III. 모듈 IV 점근선 후보 검증 및 II차 자체 발굴 브릿지

// 모듈 V(Python)가 전달한 1차 후보(수직 x값 + 비수직 식 문자열)를 모듈 III 엔진으로 검증하고,
// 놓친 초월함수 점근선까지 2차 발굴해 최종 GAsymp 배열로 반환하는 DLL 진입점
CALCTECH_API int calctech_analyze_asymptotes(
    const ASTNode* f_root,                 // 주함수 f(x) AST
    double x_min,                          // 현재 뷰포트 X 시작점
    double x_max,                          // 현재 뷰포트 X 끝점
    int canvas_width_px,                   // 캔버스 가로 픽셀 수
    const double* cand_vert_xs,            // [모듈V 1차 후보] 분모=0 수직 점근선 후보 x 배열 (없으면 NULL)
    int num_vert_cands,                    // 수직 후보 개수
    const char* const* cand_rhs_exprs,     // [모듈V 1차 후보] 비수직 점근선 우변 식 배열 (예: "2*x - 1", 없으면 NULL)
    const ASType* cand_types,              // 각 비수직 후보의 점근선 종류 배열 (없으면 NULL)
    int num_nonvert_cands,                 // 비수직 후보 개수
    GAsymp* out_asymptotes,                // [출력] 최종 확정/리포트 점근선 배열
    int max_asymptotes                     // 출력 버퍼 최대 크기
) {
    if (f_root == NULL || out_asymptotes == NULL || max_asymptotes <= 0 || x_max <= x_min) {
        return 0;
    }

    // 1. 모듈 V가 넘긴 비수직 후보 문자열들을 임시 AST로 변환
    ASTNode** temp_g_roots = NULL; //포인터를 여러개 담는 동적 배열의 시작점
    char** temp_eq_strs = NULL;

    if (cand_rhs_exprs != NULL && num_nonvert_cands > 0) {
        temp_g_roots = (ASTNode**)calloc((size_t)num_nonvert_cands, sizeof(ASTNode*)); //malloc 동적 배열 ver.
        temp_eq_strs = (char**)calloc((size_t)num_nonvert_cands, sizeof(char*)); //대신 얘는 땅을 뚫자마자 모든 칸을 0으로 청소

        for (int i = 0; i < num_nonvert_cands; i++) {
            if (cand_rhs_exprs[i] == NULL || cand_rhs_exprs[i][0] == '\0') continue;

            temp_g_roots[i] = calctech_parse_formula(cand_rhs_exprs[i]);
            temp_eq_strs[i] = (char*)malloc(96);
            if (temp_eq_strs[i] != NULL) {
                snprintf(temp_eq_strs[i], 96, "y = %s", cand_rhs_exprs[i]);
            }
        }
    }

    // 2. 모듈 III 통합 엔진 호출:
    //- 현재 화면 내 수직 점근선 1차 스캔
    //- 모듈 V 비수직 후보식 잔차 r(x) = |f(x) - g(x)| 정밀 검증
    //- 초월함수(arctan, exp 등) 수평 점근선 2차 자체 발굴
    int total = extract_asymptote_equations(
        f_root,
        x_min, x_max,
        canvas_width_px,
        (const ASTNode* const*)temp_g_roots,
        cand_types,
        (const char* const*)temp_eq_strs,
        num_nonvert_cands,
        out_asymptotes,
        max_asymptotes
    );

    // 3. 임시 파싱한 후보 AST 및 문자열 메모리 즉시 해제
    if (temp_g_roots != NULL) {
        for (int i = 0; i < num_nonvert_cands; i++) {
            if (temp_g_roots[i] != NULL) free_ast(temp_g_roots[i]);
            if (temp_eq_strs != NULL && temp_eq_strs[i] != NULL) free(temp_eq_strs[i]);
        }
        free(temp_g_roots);
        free(temp_eq_strs);
    }

    // 4. 모듈 V가 별도로 넘긴 수직 후보(분모=0 지점) 정밀 검증 및 병합
    //    (해상도 격자 사이에 숨은 좁은 수직 점근선 보완 + Hole 불연속점 필터링)
    if (cand_vert_xs != NULL && num_vert_cands > 0) {
        for (int i = 0; i < num_vert_cands && total < max_asymptotes; i++) {
            double xv = cand_vert_xs[i];
            if (isnan(xv) || isinf(xv) || xv < x_min || xv > x_max) continue;

            // 좌·우 아주 가까운 지점(1e-5)에서 발산 여부 확인 (Hole 제거)
            double yl = fabs(eval_ast(f_root, xv - 1e-5));
            double yr = fabs(eval_ast(f_root, xv + 1e-5));
            bool div_l = (!isnan(yl) && (isinf(yl) || yl > 1e3));
            bool div_r = (!isnan(yr) && (isinf(yr) || yr > 1e3));
            if (!div_l && !div_r) continue;

            // 이미 등록된 수직 점근선과 중복인지 확인
            bool dup = false;
            for (int k = 0; k < total; k++) {
                if (out_asymptotes[k].type == ASYMP_VERTICAL && fabs(out_asymptotes[k].value - xv) < 1e-4) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;

            char val_str[64];
            format_symbolic_or_decimal(xv, val_str, sizeof(val_str));

            out_asymptotes[total].type = ASYMP_VERTICAL;
            out_asymptotes[total].value = xv;
            out_asymptotes[total].direction = (div_l && div_r) ? 0 : (div_l ? -1 : +1);
            out_asymptotes[total].is_confirmed = true;
            out_asymptotes[total].final_residual = NAN;
            snprintf(out_asymptotes[total].equation, sizeof(out_asymptotes[total].equation), "x = %s", val_str);
            total++;
        }
    }

    return total;
}

#pragma endregion