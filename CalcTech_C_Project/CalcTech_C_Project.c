#include "CalcTech공용두꺼비집.h"

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
    if (root != NULL) {
        free_ast(root);
    }
}

// 3. 문자열 수식 -> LaTeX 문자열 즉시 변환
CALCTECH_API void calctech_formula_to_latex(const char* expr, char* out_buf, int max_len) {
    if (expr == NULL || out_buf == NULL || max_len <= 0) return;

    out_buf[0] = '\0';
    ASTNode* root = calctech_parse_formula(expr);
    if (root != NULL) {
        ast_to_latex(root, out_buf, (size_t)max_len);
        free_ast(root);
    }
}

// 4. 통합 렌더링 데이터 패키지 일괄 생성 (샘플링 + 미적분 + 축 스케일링 + 특징점)
CALCTECH_API ViewportPackage* calctech_build_viewport(
    const ASTNode* f_root,
    const ASTNode* g_root,
    double x_min,
    double x_max,
    int canvas_width_px,
    int scale_target,
    int x_unit_mode,
    int include_origin_y
) {
    if (f_root == NULL || x_max <= x_min) return NULL;

    ViewportPackage* pkg = (ViewportPackage*)calloc(1, sizeof(ViewportPackage));
    if (pkg == NULL) return NULL;

    // 1) 해상도 연동 동적 샘플링 버퍼 할당 및 f(x) 추출
    double* temp_x = (double*)malloc(sizeof(double) * MAX_SAMPLE_COUNT);
    double* temp_y = (double*)malloc(sizeof(double) * MAX_SAMPLE_COUNT);
    if (!temp_x || !temp_y) {
        free(temp_x);
        free(temp_y);
        free(pkg);
        return NULL;
    }

    int n = sample_curve_adaptive(f_root, x_min, x_max, canvas_width_px, temp_x, temp_y, MAX_SAMPLE_COUNT);
    if (n < 2) {
        free(temp_x);
        free(temp_y);
        free(pkg);
        return NULL;
    }

    pkg->sample_count = n;
    pkg->dx = (x_max - x_min) / (double)(n - 1);

    pkg->x_arr = (double*)malloc(sizeof(double) * n);
    pkg->y_arr = (double*)malloc(sizeof(double) * n);
    pkg->dy_arr = (double*)malloc(sizeof(double) * n);
    pkg->d2y_arr = (double*)malloc(sizeof(double) * n);
    pkg->int_y_arr = (double*)malloc(sizeof(double) * n);
    pkg->g_y_arr = (g_root != NULL) ? (double*)malloc(sizeof(double) * n) : NULL;

    if (!pkg->x_arr || !pkg->y_arr || !pkg->dy_arr || !pkg->d2y_arr || !pkg->int_y_arr ||
        (g_root != NULL && !pkg->g_y_arr)) {
        free(temp_x);
        free(temp_y);
        calctech_free_viewport(pkg);
        return NULL;
    }

    memcpy(pkg->x_arr, temp_x, sizeof(double) * n);
    memcpy(pkg->y_arr, temp_y, sizeof(double) * n);
    free(temp_x);
    free(temp_y);

    // 2) 도함수, 이계도함수, 누적 정적분 및 보조함수 g(x) 채우기
    double running_area = 0.0;
    for (int i = 0; i < n; i++) {
        double x = pkg->x_arr[i];
        pkg->dy_arr[i] = num_diff_1st(f_root, x);
        pkg->d2y_arr[i] = num_diff_2nd(f_root, x);

        if (i == 0) {
            pkg->int_y_arr[0] = 0.0;
        }
        else {
            double x_prev = pkg->x_arr[i - 1];
            double y_prev = pkg->y_arr[i - 1];
            double y_curr = pkg->y_arr[i];
            double y_mid = eval_ast(f_root, 0.5 * (x_prev + x));
            double seg_dx = x - x_prev;

            if (!isnan(y_prev) && !isnan(y_mid) && !isnan(y_curr)) {
                running_area += (seg_dx / 6.0) * (y_prev + 4.0 * y_mid + y_curr);
            }
            pkg->int_y_arr[i] = running_area;
        }

        if (g_root != NULL) {
            pkg->g_y_arr[i] = eval_ast(g_root, x);
        }
    }

    // 3) 동적 축 스케일링 (점근선 마스킹 + Nice-Step 계산)
    ScaleOption opt;
    opt.scale_target = scale_target;
    opt.x_unit_mode = x_unit_mode;
    opt.include_origin_y = include_origin_y;
    opt.target_ticks = 8;

    pkg->axis = compute_dynamic_axis_scale(
        f_root, g_root, x_min, x_max,
        pkg->y_arr, pkg->g_y_arr, n, &opt
    );

    // 4) 심화 특징점(절편, 극점, 변곡점, 교점, 구멍) 추출
    pkg->keypoint_count = extract_graph_keypoints(
        f_root, g_root, x_min, x_max,
        pkg->keypoints, MAX_KEY_POINTS
    );

    return pkg;
}

// 5. 렌더링 데이터 패키지 동적 메모리 일괄 해제
CALCTECH_API void calctech_free_viewport(ViewportPackage* pkg) {
    if (pkg == NULL) return;

    free(pkg->x_arr);
    free(pkg->y_arr);
    free(pkg->dy_arr);
    free(pkg->d2y_arr);
    free(pkg->int_y_arr);
    free(pkg->g_y_arr);
    free(pkg);
}