#pragma once
#define _USE_MATH_DEFINES
#define DEFAULT_SAMPLE_COUNT 2000
//수치 미분 전용 스텝 크기 (절단 오차와 반올림 오차의 최적 균형점)
#define DIFF_STEP_H1  1e-5   // 1계 도함수용 (eps^(1/3))
#define DIFF_STEP_H2  1e-4   // 2계 도함수용 (eps^(1/4))

//교점 탐색 및 0 판정 허용 오차 (부동소수점 노이즈 회피)
#define ZERO_EPSILON  1e-9   // |f(x) - g(x)| < 1e-9 이면 교점으로 인정
#define MAX_INTERSECTIONS 32
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

#ifdef __cplusplus
}
#endif

//모듈II: 모듈I에서 생성한 수식의 그래프를 수학적으로 다루는 툴 모음집

// IIA: C언어로 미분적분 구현하는 코드
// [1] 사용자가 입력한 수식을 진짜로 계산하는 함수
double eval_ast(const ASTNode* node, double x);
// [2] 단일 점 수치 미분 (1계, 2계 중앙 차분법)
double num_diff_1st(const ASTNode* root, double x);
double num_diff_2nd(const ASTNode* root, double x);
// [3] 단일 구간 정적분 (심프슨 1/3 법칙)
double num_integrate(const ASTNode* root, double a, double b, int steps);
// [4] 2000개 격자 일괄 샘플링 커널 (원함수, 1계도함수, 2계도함수, 누적정적분 배열 동시 생성)
void sample_calculus_arrays(
    const ASTNode* root, //모듈I이 만든 수식 트리
    double x_min, //x축 시작점
    double x_max, //x축 끝점
    int count, //샘플링 개수
    double* out_x, //출력 버퍼 I: x좌표 arr
    double* out_y, // 버퍼 II: x에 대한 함숫값 배열
    double* out_dy, // 버퍼 III: 1계도함수 배열
    double* out_d2y, //버퍼 IV: 2계도함수 배열
    double* out_int_y); //버퍼 V: 누정 정적분 배열

//IIB: 파생함수 10가지 구현하는 코드

// 파생함수I: 두 함수로 둘러쌓인 넓이함수
// [1] 접선 정보 계산 및 접선 함수 g_t(x) 평가
TangentInfo calc_tangent_info(const ASTNode* f, double t);
double eval_tangent(TangentInfo tan_info, double x);

// [2] 곡선 f(x)와 접선 g_t(x)의 교점 자동 탐색 (찾은 교점 개수 반환)
int find_curve_tangent_intersections(
    const ASTNode* f,
    TangentInfo tan_info,
    double x_min,
    double x_max,
    double* out_roots,
    int max_roots
);

// [3] 심프슨 1/3 법칙 기반 두 함수 사이의 넓이 계산
double integrate_abs_diff_two_funcs(const ASTNode* f, const ASTNode* g, double a, double b, int steps);
double integrate_abs_diff_tangent(const ASTNode* f, TangentInfo tan_info, double a, double b, int steps);

// [4] [PIP 메인 커널] 접점 t 이동에 따른 선택 영역의 넓이 변화 함수 S(t) 배열 생성
void generate_tangent_area_S(
    const ASTNode* f,
    double t_min,
    double t_max,
    double x_min,
    double x_max,
    int region_idx,   // 사용자가 클릭해 선택한 구역 번호 (0, 1, 2...)
    int count,        // 샘플링 개수 (기본 2000)
    double* out_t,    // 출력 버퍼 1: t 좌표 배열
    double* out_S     // 출력 버퍼 2: 넓이 S(t) 배열
);