#pragma once
#define _USE_MATH_DEFINES
#define DEFAULT_SAMPLE_COUNT 2000
//수치 미분 전용 스텝 크기 (절단 오차와 반올림 오차의 최적 균형점)
#define DIFF_STEP_H1  1e-5   // 1계 도함수용 (eps^(1/3))
#define DIFF_STEP_H2  1e-4   // 2계 도함수용 (eps^(1/4))

//교점 탐색 및 0 판정 허용 오차 (부동소수점 노이즈 회피)
#define ZERO_EPSILON  1e-9   // |f(x) - g(x)| < 1e-9 이면 교점으로 인정
#define MAX_CRITICAL_POINTS 64
#define MAX_REGIONS 128

// 경계선 종류 식별 코드 (for 파생함수 IA)
#define BOUND_FUNC   1  // 원함수 y = f(x)
#define BOUND_TAN    2  // 접선   y = g_t(x)
#define BOUND_X_AXIS 3  // x축    y = 0

// 정적분 정의 함수 패턴 식별 코드 (for 파생함수 IB)
#define DEF_INT_FIXED_START  1  //g(x) = ∫_a^x f(t) dt  (시작점 a 고정)
#define DEF_INT_SLIDING_WIN  2  // g(x) = ∫_x^(x+h) f(t) dt (폭 h 구간 이동)
#define DEF_INT_SYMMETRIC    3  //g(x) = ∫_(a-x)^(a+x) f(t) dt (a 기준 대칭 확장)

//테일러 급수 최대 미분 횟수 (for 파생함수 VIII)
#define MAX_TAYLOR_DEGREE 14

//공통 표준 라이브러리 모음
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <complex.h>

//모듈 I: 사용자가 입력한 수식을 컴퓨터가 인식 가능한 형태로 변환하는 모듈

// 빵틀 정의
// [빵틀 1] 토큰의 종류 (숫자, 변수, 연산자, 괄호 등 분류)

typedef enum {
    TOKEN_NUMBER,   //숫자 (2,3.14)
    TOKEN_VARIABLE,  //변수 (x,y,z)
    TOKEN_KEYWORD, //예약어 (sin,cos,ln,lim)
    TOKEN_OPERATOR,  //사칙연산 + 제곱연산
    TOKEN_LPAREN,  // (
    TOKEN_RPAREN,   // )
    TOKEN_COMMA, // for 다중 인자 연산자 (lim,int,sigma,...)
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

// [빵틀 2] 단일 토큰 정보 (토큰의 타입 + 실제 문자열 텍스트)
typedef struct {
    TokenType type; // 토큰 종류
    char text[32];  // 쪼개진 글자를 저장하는 버퍼 (최대 31자 + 널문자)
} Token;

// [빵틀 3] 토큰들을 담는 동적 배열 (토큰 리스트)
typedef struct {
    Token* tokens;  // 토큰 배열의 시작 메모리 주소
    int count;      // 현재 저장된 토큰 개수
    int capacity;   // 동적으로 할당된 메모리 최대 용량
} TokenList;

// [빵틀 4] AST 노드 Type
typedef enum {
    NODE_NUMBER,    // 숫자 (예: 2, 1)
    NODE_VARIABLE,  // 일반변수 (x,y,z,...)
    NODE_FUNCTION, //단일 인자 함수 (exp,ln,sin,cos,tan,...)
    NODE_SPECIAL_OP, //다중 인자 함수 (log, integral, sigma, limit, ...)
    NODE_BINARY_OP  // 이항 연산자 (+, -, *, /, ^)
} ASTNodeType;

// [빵틀 5] 예약어 관리 빵틀 (밀가루고 뭐고 없는 쇠통 빵틀)
typedef struct {
    const char* name; //예약어 text
    ASTNodeType type; //단일 인자 / 다중 인자
    int expected_args; //요구되는 인자 개수
} KeywordSpec;

// [빵틀 6] AST 단일 노드 구조체
typedef struct ASTNode {
    ASTNodeType type;
    char value[32];          // 값 또는 연산자 텍스트 ("2", "x", "+", "/" 등)
    struct ASTNode* left;    // 왼쪽 자식 노드 포인터
    struct ASTNode* right;   // 오른쪽 자식 노드 포인터
    struct ASTNode* args[4]; //다중 인자 저장용 포인터 배열
    int arg_count; //실제 들어온 인자 개수
} ASTNode;

// "어딘가(.c 파일)에 KEYWORD_TABLE과 KEYWORD_COUNT가 정의되어 있으니 갖다 쓰셈"
extern const KeywordSpec KEYWORD_TABLE[]; // {"함수명", 노드타입, 인자개수}
extern const int KEYWORD_COUNT;

// [빵틀 6] 파서 위치 관리 구조체
typedef struct {
    TokenList* list; // 1단계에서 쪼갠 토큰 리스트
    int current;     // 현재 읽고 있는 토큰 위치 (인덱스)
} Parser;

// [빵틀 7] 특정 접점 t에서의 접선 정보 패키징
typedef struct {
    double t;            // 접점의 x좌표
    double ft;           // 접점의 y좌표 f(t)
    double slope;        // 접선의 기울기 f'(t)
    double x_intercept;  // 접선의 x절편 (기울기가 0이면 NAN)
    double y_intercept;  // 접선의 y절편
} TangentInfo;

// [빵틀 8] 분할된 개별 영역 정보 (파이썬 호버링 & 넓이 표시용)
typedef struct {
    int id;          // 영역 고유 번호 (0, 1, 2...)
    double a;        // 왼쪽 x 경계
    double b;        // 오른쪽 x 경계
    int top_type;    // 천장 경계 종류 (BOUND_FUNC / BOUND_TAN / BOUND_X_AXIS)
    int bot_type;    // 바닥 경계 종류 (BOUND_FUNC / BOUND_TAN / BOUND_X_AXIS)
    double area;     // 심프슨 법칙으로 계산된 해당 조각의 넓이
    int is_closed;   // 1이면 교점/축으로 완전히 닫힌 영역, 0이면 화면 끝(x_min/x_max)에 닿은 열린 영역
} TangentRegion;

// [빵틀 9] N축의 원리를 반영하여 얻은 실근을 담는 빵틀
typedef struct {
  double t_val; // 겉함수 y = f(t)의 실근좌표
  double x_val; // 속함수 g(x) = t의 실근좌표
} CompositeRootPath;

// [빵틀 10] 접선의 기울기를 계속 뽑아내는 빵틀
 typedef struct {
     double t;          // 현재 기준 x좌표
     double ft;         // 원함수 높이 f(t)
     double dft;        // 1차 접선 기울기 f'(t) == PIP 1의 함숫값
     double d2ft;       // 2차 접선 기울기 f''(t) == PIP 2의 함숫값
     double tan1_y_int; // 원함수 접선의 y절편: f(t) - t * f'(t)
     double tan2_y_int; // 도함수 접선의 y절편: f'(t) - t * f''(t)
 } CascadeDerivativePoint;

 //별명등록 ∵너무 길어..
 typedef CascadeDerivativePoint CDP;

// [빵틀 11] 라그랑주 MVT 전용 빵틀
 typedef struct {
     double a, b;            // 구간 양 끝점
     double fa, fb;          // 양 끝점 함숫값
     double secant_slope;    // 할선 평균변화율: (f(b) - f(a)) / (b - a)
     double secant_y_int;    // 할선 y절편: f(a) - a * secant_slope
     double max_abs_deriv;   // 구간 내 |f'(x)| 최댓값 M (립시츠/오차 한계 상수)
     int c_count;            // 발견된 평균값 정리 만족점 c의 개수
     double c_vals[32];      // 만족점 c의 x좌표 배열
     double fc_vals[32];     // 만족점에서의 함숫값 f(c) 배열
     double tan_y_ints[32];  // 각 c에서의 평행 접선 y절편 배열
 } MVTResult;

 // [빵틀 12] 코시 MVT 전용 빵틀
 typedef struct {
     double a, b;            // 매개변수 t의 시작/끝 구간 [a, b]
     double ga, fa;          // 출발점 좌표 (g(a), f(a))
     double gb, fb;          // 도착점 좌표 (g(b), f(b))
     double chord_dx;        // 전체 이동 벡터 x성분: g(b) - g(a)
     double chord_dy;        // 전체 이동 벡터 y성분: f(b) - f(a)
     int c_count;            // 두 벡터가 평행해지는 순간 c의 총개수
     double c_vals[32];      // 만족 매개변수 t = c 목록
     double gc_vals[32];     // 평행 접점의 x좌표 g(c)
     double fc_vals[32];     // 평행 접점의 y좌표 f(c)
     double dg_vals[32];     // 그 순간의 속도 벡터 x성분 g'(c) (파이썬 화살표 그리기용)
     double df_vals[32];     // 그 순간의 속도 벡터 y성분 f'(c) (파이썬 화살표 그리기용)
 } CauchyMVTResult;
 typedef CauchyMVTResult Cauchy;

 // [빵틀 13] G.D. 전용 빵틀
 typedef struct {
     int step_count;          // 실제 이동한 총 스텝 수 (발산 시 조기 중단된 시점까지)
     int status;              // 0: 극소점 수렴 성공 | 1: 최대 스텝 도달(진동 중) | 2: 발산(폭주)
     double x_hist[128];      // 매 스텝 공의 x좌표 (x_0, x_1, ..., x_n)
     double y_hist[128];      // 매 스텝 공의 높이 f(x_n)
     double grad_hist[128];   // 출발점 발밑의 순간변화율(접선 기울기) f'(x_n)
     double secant_hist[128]; // 실제 점프한 구간 [x_n, x_{n+1}]의 평균변화율(할선 기울기)
 } GDTraceResult;

 // [빵틀 14] 곡선의 길이(호의 길이) 통합 분석 결과 빵틀
 typedef struct {
     double a, b;              // 구간 양 끝점
     double total_length;      // 전체 곡선 길이 L = ∫_a^b sqrt(1 + f'(x)^2) dx
     double chord_length;      // 양 끝점을 잇는 직선 현의 길이 sqrt((b-a)^2 + (f(b)-f(a))^2)
     double tortuosity;        // 굴곡도(우회율) = total_length / chord_length (직선이면 1.0)
     int div_count;            // 등분할 점 개수 (최대 64)
     double eq_x[64];          // 호의 길이를 등분할하는 점들의 x좌표
     double eq_y[64];          // 호의 길이를 등분할하는 점들의 y좌표 f(x)
 } ArcLengthResult;
 typedef ArcLengthResult ALR;

 // [빵틀 15] 뉴턴-랩슨 전용 빵틀
 typedef struct {
     int iter_count;          // 실제 수행한 반복 횟수 (최대 63)
     int status;              // 0: 근 수렴 성공 | 1: 최대 반복 도달(진동/느린 수렴) | 2: 발산(접선 수평 f'(x)≈0 또는 폭주)
     double root;             // 최종 수렴한 근 (수렴 실패 시 마지막 x값)
     double x_hist[64];       // 각 스텝의 x좌표 (x_0, x_1, ..., x_n)
     double hx_hist[64];      // 차이 함수 높이 h(x_n) = f(x_n) - g(x_n)
     double fx_hist[64];      // 원함수 높이 f(x_n) (메인 캔버스 교점 표시용)
     double gx_hist[64];      // 비교함수 높이 g(x_n) (g가 NULL이면 0.0)
     double slope_hist[64];   // 각 스텝에서의 접선 기울기 h'(x_n)
 } NewtonTraceResult;
 typedef NewtonTraceResult NewtonTrace;

 // [빵틀 16] k계 도함수 중앙 차분 가중치 테이블 전용 빵틀
 typedef struct {
     int order;               // 미분 차수 k (0 ~ 14)
     int radius;              // 좌우 샘플링 반경 R (총 2R + 1개 점 사용)
     double step_h;           // 해당 차수에 최적화된 스텝 크기 h
     double weights[32];      // 중앙 차분 가중치 w_j (j = -R ~ +R 를 0 ~ 2R 인덱스에 매핑)
 } FiniteDiffStencil;
 typedef FiniteDiffStencil FDS;

 // [빵틀 17] 테일러 급수 근사 결과 빵틀 (최대 14차까지 확장)
 typedef struct {
     double center_a;                       // 전개 기준점 a
     int degree;                            // 근사 다항식 차수 n (0 ~ MAX_TAYLOR_DEGREE)
     double derivs[MAX_TAYLOR_DEGREE + 1];  // k계 도함수 값 f^(k)(a)
     double coeffs[MAX_TAYLOR_DEGREE + 1];  // 테일러 계수 c_k = f^(k)(a) / k!
     double max_error;                      // 구간 내 최대 잔여 오차 max |f(x) - P_n(x)|
 } TaylorResult;

 // [빵틀 18] 역함수 투영 및 역함수 미분법 연동 결과 빵틀
 typedef struct {
     int is_monotonic;        // 1: 구간 내 순증가/순감소 (역함수 단일 함수 존재) | 0: 극값 (도함수의 부호 변화)존재 
     int monotone_dir;        // 1: 순증가, -1: 순감소, 0: 비단조 (변곡점 존재 또는 상수함수)
     double t;                // 원함수 기준 x좌표 t
     double ft;               // 원함수 y좌표 f(t) == 역함수의 입력 x좌표
     double df_t;             // 원함수 접선 기울기 f'(t)
     double inv_df_ft;        // 역함수 대칭점 (f(t), t)에서의 접선 기울기 1 / f'(t) (수직 접선 시 NAN)
     double orig_tan_y_int;   // 원함수 접선의 y절편: f(t) - t * f'(t)
     double inv_tan_y_int;    // 역함수 접선의 y절편: t - f(t) * (1 / f'(t)) (수직 접선 시 NAN)
 } InverseDerivInfo;
 typedef InverseDerivInfo IDI;

 // [빵틀 19] 특정 접점 t에서의 곡률 및 접촉원(Osculating Circle) 정보 빵틀
 typedef struct {
     double t;                // 기준 접점의 x좌표
     double ft;               // 기준 접점의 y좌표 f(t)
     double curvature;        // 곡률 κ(t) = |f''(t)| / (1 + f'(t)^2)^(3/2)
     double radius;           // 곡률반지름 R(t) = 1 / κ(t) (변곡점 등 직선 구간은 NAN)
     double center_x;         // 접촉원 중심의 x좌표 (변곡점 구간은 NAN)
     double center_y;         // 접촉원 중심의 y좌표 (변곡점 구간은 NAN)
 } OsculatingCircleInfo;
 typedef OsculatingCircleInfo OscCircle;

#ifdef __cplusplus
extern "C" {
#endif

//외부 공개 함수

//키워드 탐색 함수
const KeywordSpec* find_keyword(const char* text);
// 토큰 리스트 메모리 초기화 (기본 용량: 16개)
void init_token_list(TokenList* list);

// 토큰 리스트에 새로운 토큰 1개 동적 추가 (용량 초과 시 2배 자동 확장 via malloc)
void add_token(TokenList* list, TokenType type, const char* text);

// ★ 입력받은 수식 문자열을 분석하여 토큰 리스트로 반환
TokenList tokenize(const char* expr);

// 토큰 리스트의 내용물을 [ '2', '*', 'x' ] 형태로 콘솔에 출력
void print_tokens(const TokenList* list);

// 사용이 끝난 토큰 리스트의 동적 할당 메모리 해제
void free_token_list(TokenList* list);

/* --- 노드 생성 / 해제 및 파서 도우미 선언-- */
ASTNode* create_ast_node(ASTNodeType type, const char* value);
void free_ast(ASTNode* node);
Token* peek(Parser* p);
Token* advance(Parser* p);

/* --- 재귀 강하 파싱 함수 선언-- - */
ASTNode* parse_expression(Parser* p);
ASTNode* parse_term(Parser* p);
ASTNode* parse_power(Parser* p);
ASTNode* parse_factor(Parser* p);

/* --- 트리 시각화 출력 함수 선언-- */
void print_ast_horizontal(ASTNode* node, char* prefix, int is_left);
void print_ast_tree(ASTNode* root);

/*버퍼 오버플로우를 막으면서 안전하게 문자열을 이어 붙이는 함수*/
void append_str(char* buf, size_t max_len, const char* str);
// ★AST 트리를 재귀적으로 순회하며 LaTeX 문자열 조립
void ast_to_latex(const ASTNode* node, char* out_buf, size_t max_len);

//모듈II: 모듈I에서 생성한 수식의 그래프를 수학적으로 다루는 툴 모음집

// IIA: C언어로 미분적분 구현하는 코드

// [1] 사용자가 입력한 수식을 진짜로 계산하는 함수
double eval_ast(const ASTNode* node, double x);
// [2] 단일 점 수치 미분 (1계, 2계 중앙 차분법)
double num_diff_1st(const ASTNode* root, double x);
double num_diff_2nd(const ASTNode* root, double x);
// [3] 단일 구간 정적분 (심프슨 1/3 법칙)
double num_integrate(const ASTNode* root, double a, double b, int steps);
// [4] 계산된 값을 2000개의 배열에다가 파바박 집어넣는 함수
void sample_calculus_arrays(
    const ASTNode* root, //모듈I이 만든 수식 트리
    double x_min, //x축 시작점
    double x_max, //x축 끝점
    int count, //샘플링 개수
    double* out_x, //출력 버퍼 I: x좌표 arr
    double* out_y, // 버퍼 II: x에 대한 함숫값 배열
    double* out_dy, // 버퍼 III: 1계도함수 배열
    double* out_d2y, //버퍼 IV: 2계도함수 배열
    double* out_int_y); //버퍼 V: 누적 정적분 배열

//IIB: 파생함수 10가지 구현하는 코드

/*파생함수 I: 심프슨 the GOAT */

//  IA: 곡선과 접선으로 둘러쌓인 넓이
//  접선 정보 구조체 생성 및 접선의 방정식 계산: g_t(x) = f'(t)(x - t) + f(t)
TangentInfo calc_tangent_info(const ASTNode* f, double t);

//곡선과 접선이 이루는 영역의 4가지 유형의 위아래를 막아주는 재료
double eval_boundary(int bound_type, const ASTNode* f, TangentInfo tan_info, double x);

// 범용 심프슨 1/3 적분기: 어떤 경계 조합이든 |top(x) - bot(x)| 적분
double integrate_between_bounds(
    const ASTNode* f,
    TangentInfo tan_info,
    double a,
    double b,
    int top_type,
    int bot_type,
    int steps
);

// [내부 도우미] 중복 없이 핵심 x좌표 추가 & 이분법 근 탐색
  void add_unique_point(double* arr, int* count, double val, double x_min, double x_max);

// 교점 탐색기
// mode == 0: 곡선과 접선의 교점 탐색, mode == 1: 곡선과 x축의 교점 탐색 탐색
  void scan_roots(const ASTNode* f, TangentInfo tan_info, int mode,
    double x_min, double x_max, double* pts, int* count);

// 평면 자동 분할기: 4가지 유형의 모든 영역을 자동 추출
int partition_tangent_regions(
    const ASTNode* f,
    TangentInfo tan_info,
    double x_min,
    double x_max,
    TangentRegion* out_regions,
    int max_regions
);

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
);

//파생함수 IB: 구간 누적 & 이동 적분

// 기존에 있던 함수에는 절댓값 옵션이 존재하지 않았음
  double simpson_piece(const ASTNode* f, double x1, double x2, int steps, int is_abs);

//g(x)의 그래프를 PIP에 그리기 위한 밑작업 함수
void generate_def_integral_array(
    const ASTNode* f,
    double x_min,
    double x_max,
    int mode, //패턴 1(시작점 고정 a~x), 패턴 2(구간 이동 x~x+h), 패턴 3(대칭 구간 a-x~a+x), 
    //위아래끝이 또다른 함수의 경우는 IC에서 다룰 예정
    double param, //패턴 1, 3에서는 기준점 'a', 패턴 2에서는 구간 폭 'h'
    int is_abs, //0이면 순수 정적분(부호 유지), 1이면 기하학적 넓이(| f(t) | )
    int count,
    double* out_x,
    double* out_g
);

//파생함수 IC: 특이함수 정적분& 이상적분
// 제2종: 점근선 같은 구간을 피한 이상적분
  double improper_integral_II(const ASTNode* f, double a, double b, int steps);

// 제1종: a~x를 정적분한 후 a를 극한으로 보내는 적분
// - 변수 치환 x = a + u / (1 - u) 를 통해 [a, ∞) 구간을 유한 구간 [0, 1)로 압축하여 적분
double improper_integral_I(const ASTNode* f, double a, int steps);

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
);

/*파생함수 II : 접점 t 이동에 따른 접선의 절편 & 축 삼각형 넓이 변화*/
void generate_tangent_intercepts_array(
    const ASTNode* f,
    double t_min,
    double t_max,
    int count,
    double* out_t,
    double* out_x_int, //접선의 x절편 X(t) = t - f(t) / f'(t) 배열 (수평 접선 구간은 NAN)
    double* out_y_int, //접선의 y절편 Y(t) = f(t) - t*f'(t) 배열
    double* out_tri_area //접선과 x축, y축이 이루는 직각삼각형 넓이 0.5 * |X(t) * Y(t)| 배열
);

/*파생함수 III: 합성함수*/

//N축 기반 합성함수 개형 추론
void generate_composite_n_axis_arrays(
    const ASTNode* f, // 겉함수 f(t)
    const ASTNode* g, // 속함수 g(x)
    double x_min,
    double x_max,
    int count,
    double* out_x, //out_x   : 입력 정의역 x 배열
    double* out_t, //out_t   : 속함수 치역 t = g(x) 배열 (N축 공유 축 좌표)
    double* out_fog //out_fog : 최종 합성함수 y = f(g(x)) 배열 (PIP 캔버스용)
);

//합성 방정식의 실근 역추적 & 합성 부등식의 범위 스캐닝

// [내부 도우미] 일반 함수방정식 h(u) = target 의 실근을 함수의 정의역 에서 모두 탐색
 int scan_equation_roots(
    const ASTNode* func,
    double target, //f(g(x)) = k에서 g(x)를 t로 치환하여 g(x) = t라는 방정식에서는 t가 target, f(t) = k에선 k가 target
    double u_min,
    double u_max,
    double* out_roots, //실근을 담는 바구니
    int max_roots
);

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
 );

// 합성부등식 f(g(x)) [op] k 해집합 마스크 생성
// 마스크: 조건을 만족시키는 칸은 1(통과), 그렇지 않은 칸은 0(빠꾸)로 분류하는 채점표 배열
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
);

/*파생함수 IV: 원함수 -> 도함수 -> 이계도함수 서브캔버스 연동*/
// [내부 도우미] 2계 도함수 f''(x) 수치 계산 (중심차분)
double num_diff_2nd(const ASTNode* f, double x);

// 슬라이더 이동 시 현재 위치 t에서의 [원함수 접선] & [도함수 접선] 정보 일괄 추출
CDP calc_cascade_derivative_at(const ASTNode* f, double t);

// PIP 1(f') 및 PIP 2(f'') 배경 곡선 배열 생성
void generate_cascade_derivative_arrays(
    const ASTNode* f,
    double x_min,
    double x_max,
    int count,
    double* out_x,
    double* out_df,
    double* out_d2f
);

//파생함수 V: MVT의 기하적 구현

// 함수의 최대속도 찾는 라그랑주 MVT
MVTResult calc_lagrange_mvt(const ASTNode* f, double a, double b);
// 두 함수의 변화율을 비유하는 코시 MVT, 극한 넣으면 로피탈 정리 뚝딱
Cauchy calc_cauchy_mvt(const ASTNode* f, const ASTNode* g, double a, double b);
// 안개가 자욱한 야산에서 안전하게 하산하려는 G.D.
GDTraceResult trace_gradient_descent(
    const ASTNode* f,
    double x0,          // 공을 떨어뜨릴 초기 위치
    double lr,          // 학습률 eta (보폭 크기)
    int max_steps       // 최대 점프 횟수 (최대 127)
);


//파생함수 VI: 곡선의 길이
// 호의 길이 추적

// [내부 도우미] 미소 길이 요소 ds/dx = sqrt(1 + (f'(x))^2) 계산
double eval_arc_speed(const ASTNode* f, double x);
// 단일 구간 [a, b]의 정확한 곡선 전체 길이 L 계산 (심프슨 1/3 법칙)
double calc_arc_length(const ASTNode* f, double a, double b, int steps);
// [PIP 파생함수] 시작점 a 기준 누적 곡선 길이 s(x) 및 직선 거리 d(x) 배열 생성
void generate_arc_length_arrays(
    const ASTNode* f,
    double a,           // 길이 측정 시작 기준점 x = a
    double x_min,
    double x_max,
    int count,
    double* out_x,      // 가로축 x좌표 배열
    double* out_s,      // 누적 곡선 길이 s(x) 배열
    double* out_chord   // 시작점 (a, f(a))와 현재 점 (x, f(x)) 사이의 직선 거리 배열
);

//곡선의 길이를 N등분하는 좌표 추출 및 굴곡도(Tortuosity) 분석
ALR calc_arc_length_partition(const ASTNode* f, double a, double b, int num_divisions);

// 매개변수 곡선 (g(t), f(t)) 의 순간 속력 v(t) 및 누적 이동 거리 s(t) 추적;



// [내부 도우미] 매개변수 곡선 순간 속력 v(t) = sqrt((g'(t))^2 + (f'(t))^2)
double eval_param_speed(const ASTNode* f, const ASTNode* g, double t);
// 단일 매개변수 구간 [t1, t2]의 곡선 궤적 길이 계산 (심프슨 1/3 법칙)
double calc_param_arc_length(const ASTNode* f, const ASTNode* g, double t1, double t2, int steps);
// [PIP 파생함수] 매개변수 t에 따른 궤적 좌표 (x(t), y(t)), 순간 속력 v(t), 누적 이동 거리 s(t) 생성
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
);

//파생함수 VII: 뉴턴-랩슨법으로 교점 탐색 & 접선 궤적 추적

// [내부 도우미] 차함수 h(x) = f(x) - g(x) 계산 (g가 NULL이면 단일 함수 f(x) = 0 모드)
double eval_diff_func(const ASTNode* f, const ASTNode* g, double x);
// [내부 도우미] 차함수의 도함수 h'(x) = f'(x) - g'(x) 수치 계산
double eval_diff_deriv(const ASTNode* f, const ASTNode* g, double x);

NewtonTrace trace_newton_raphson(
    const ASTNode* f,
    const ASTNode* g,   // NULL 허용 (단일 방정식 f(x)=0 모드)
    double x0,          // 사용자가 슬라이더로 지정하는 초기 추정값
    int max_iter        // 최대 반복 횟수 (최대 63)
);

//파생함수 VIII: 테일러 급수로 초월함수 근사 (최대 14회) & 잔여 오차 추적
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
);

// [빵틀 조립기] 테일러 급수의 표현한도를 정하는 함수, 이를테면 1로 설정하면 0차 + 1차, 10으로 설정하면 상수부터 10차까지 깡그리 더해서 표현
/*1. 다항식 (x-1)^k을 전개했을때 나오는 계수들을 모아서 가중치로 재활용
    2. 그 가중치들을 기준점 주변의 함숫값에 각각 더해서 h^(-k)곱
    3. k번 미분한 식 출력*/
FDS build_central_diff_stencil(int order);

// [빵틀 실행기] 진짜로 k번 미분한 값을 토해내는 함수
double apply_diff_stencil(const ASTNode* f, double a, const FDS* st);
// [호너 평가기] 임의 차수 테일러 다항식 P_n(x) 고속 계산

/*※호너 평가기: 다항식을 상수 + 계수 * 뭐시깽이 형태로 고치는 마법
다항식을 그냥 계산하면 2000개의 포인터를 갖고있는 데이터 하나 집어넣을때마다
pow함수를 겁나게 넣어야 하는데, 얘를 쓰면 곱셈 몇번 덧셈 몇번으로 마법같이 해결*/
double eval_taylor_poly(const TaylorResult* tr, double x);

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
);

// [슬라이더 연동 IX-2] 특정 접점 t에서 원함수 접선과 역함수 대칭 접선 정보 동시 추출
IDI calc_inverse_derivative_at(const ASTNode* f, double t, double x_min, double x_max);

// 파생함수 X : 곡률(kappa)과 접촉원(Osculating Circle) 추적
// [슬라이더 연동 X-1] 특정 점 t에서의 곡률 κ(t), 곡률반지름 R(t), 접촉원 중심 (center_x, center_y) 계산
OscCircle calc_osculating_circle_at(const ASTNode* f, double t);

// [PIP 파생함수 X-2] 구간 전체에 대한 곡률 함수 κ(x) 및 곡률반지름 R(x) 배열 생성
void generate_curvature_array(
    const ASTNode* f,
    double x_min,
    double x_max,
    int count,
    double* out_x,          // 가로축 x좌표 배열
    double* out_curvature,  // 곡률 κ(x) 배열 (PIP 서브캔버스용)
    double* out_radius      // 곡률반지름 R(x) 배열 (변곡점 부근은 NAN)
);

#ifdef __cplusplus
}
#endif
