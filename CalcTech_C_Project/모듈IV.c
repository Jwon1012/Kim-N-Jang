#include "CalcTech공용두꺼비집.h"
#pragma region 분수함수 점근선 후보 검증 및 II차 자체 발굴 브릿지
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
