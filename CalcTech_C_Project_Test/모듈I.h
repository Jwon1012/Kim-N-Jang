#pragma once

//모듈 I: 사용자가 입력한 수식을 컴퓨터가 인식 가능한 형태로 변환하는 모듈

 // 1. 공통 표준 라이브러리 모음
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

// [빵틀 5] 예약어 관리 빵틀
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
static void append_str(char* buf, size_t max_len, const char* str);
// ★AST 트리를 재귀적으로 순회하며 LaTeX 문자열 조립
void ast_to_latex(const ASTNode* node, char* out_buf, size_t max_len);