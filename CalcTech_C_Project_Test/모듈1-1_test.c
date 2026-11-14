#include "모듈I.h"

const KeywordSpec KEYWORD_TABLE[] = {
    // [0] 상수 키워드 (expected_args = 0, 자식 노드 없음)
    { "inf",       NODE_NUMBER,     0 }, // 무한대 (\infty)
    { "infty",     NODE_NUMBER,     0 }, // LaTeX 표기용 무한대
    { "pi",        NODE_NUMBER,     0 }, // 원주율 (\pi)

    // [1] 단일 인자 함수 (expected_args = 1)
    { "exp",       NODE_FUNCTION,   1 }, // Exponential
    { "ln",        NODE_FUNCTION,   1 }, // 자연로그
    { "log",       NODE_FUNCTION,   1 }, // 상용로그
    { "abs",       NODE_FUNCTION,   1 }, // 절댓값 (\left| x \right|)
    { "prime",     NODE_FUNCTION,   1 }, // 뉴턴/라그랑주 미분 (f'(x))
    { "sin",       NODE_FUNCTION,   1 },
    { "cos",       NODE_FUNCTION,   1 },
    { "tan",       NODE_FUNCTION,   1 },
    { "sec",       NODE_FUNCTION,   1 }, //삼각함수의 역수
    { "csc",       NODE_FUNCTION,   1 },
    { "cot",       NODE_FUNCTION,   1 },
    { "asin",      NODE_FUNCTION,   1 }, //역삼각함수
    { "acos",      NODE_FUNCTION,   1 },
    { "atan",      NODE_FUNCTION,   1 },
    { "asec",      NODE_FUNCTION,   1 },
    { "acsc",      NODE_FUNCTION,   1 },
    { "acot",      NODE_FUNCTION,   1 },
    { "sinh",      NODE_FUNCTION,   1 }, //쌍곡선함수
    { "cosh",      NODE_FUNCTION,   1 },
    { "tanh",      NODE_FUNCTION,   1 },
    { "sqrt",      NODE_FUNCTION,   1 }, //제곱근 (0.5제곱으로 작성해도 무관)

    // [2] 다중 인자 함수 및 대형 수학 연산자 (expected_args >= 2)
    { "frac",      NODE_SPECIAL_OP, 2 }, // args[0]: 분자, args[1]: 분모
    { "logbase",   NODE_SPECIAL_OP, 2 }, // args[0]: 밑, args[1]: 진수
    { "indef_int", NODE_SPECIAL_OP, 2 }, // args[0]: 피적분함수, args[1]: 적분변수 (부정적분)
    { "diff",      NODE_SPECIAL_OP, 3 }, // args[0]: 피미분식, args[1]: 변수, args[2]: 미분횟수 (라이프니츠)
    { "lim",       NODE_SPECIAL_OP, 3 }, // args[0]: 변수, args[1]: 목표값, args[2]: f(x)
    { "sum",       NODE_SPECIAL_OP, 4 }, // args[0]: 변수, args[1]: 시작값, args[2]: 끝값, args[3]: 일반항
    { "int",       NODE_SPECIAL_OP, 4 }  // args[0]: 아래끝, args[1]: 위끝, args[2]: 피적분함수, args[3]: 적분변수
};
const int KEYWORD_COUNT = sizeof(KEYWORD_TABLE) / sizeof(KEYWORD_TABLE[0]);

// 1. keyword 탐색 함수
const KeywordSpec* find_keyword(const char* text)
{
    for (int i = 0; i < KEYWORD_COUNT; i++)
    {
    if (strcmp(text, KEYWORD_TABLE[i].name) == 0) return &KEYWORD_TABLE[i];
    }
    return NULL;
}

// 2. 예약어 여부 검사 함수
int is_keyword(const char* str) {return find_keyword(str) != NULL;}

// 토큰 리스트 메모리 초기화
void init_token_list(TokenList* list) {
    list->capacity = 16; //리스트 용량
    list->count = 0;
    list->tokens = (Token*)malloc(sizeof(Token) * list->capacity);
//sizeof(Token): 토큰 방 1개 만드는데 필요한 땅 크기
//*list->capacity: 를 16개로 우선 설정
//malloc: 땅 16개를 우선 뚫어놨으니 시작 주소 가져가라는 의미 (memory + allocation)
//Token*: 해당 땅을 0번째 메모리부터 Token전용으로 독점하겠다는 의미
}

// 토큰 동적 추가 (용량 초과 시 메모리 2배 확장)
void add_token(TokenList* list, TokenType type, const char* text) 
{
    if (list->count >= list->capacity) 
    {
        list->capacity *= 2;
        Token* temp = (Token*)realloc(list->tokens, sizeof(Token) * list->capacity);
    //realloc: 땅이 꽉 차면 땅 개수를 list->capacity만큼 올리겠다는 의미
        if (temp == NULL) return;
        list->tokens = temp;
    }
    list->tokens[list->count].type = type;
    strncpy_s(list->tokens[list->count].text, sizeof(list->tokens[list->count].text), text, 31);
    list->tokens[list->count].text[31] = '\0';
    list->count++;
}

// 4. [핵심] 문자열 수식을 토큰 리스트로 분리하는 1단계 Lexer 함수
TokenList tokenize(const char* expr) {
    TokenList list;
    init_token_list(&list);
    const char* p = expr; //expr의 0번째 주소를 p에게 할당

    while (*p != '\0') {
        // [규칙 A] 공백 문자는 건너뛰기
        if (isspace((unsigned char)*p)) { p++; continue;}

        // [규칙 B] 숫자 읽기 (정수 및 소수점 처리 - 연산자 우선순위 괄호 보완)
        if (isdigit((unsigned char)*p) || *p == '.') 
        {
            char buf[256] = { 0 };
            int idx = 0;
            while ((isdigit((unsigned char)*p) || *p == '.') && idx < 255) buf[idx] = *p++;
            add_token(&list, TOKEN_NUMBER, buf);
            continue;
        }

        // [규칙 C] 변수 및 함수 이름 읽기
        if (isalpha((unsigned char)*p)) 
        {
            char buf[256] = { 0 };
            int idx = 0;
            while (isalpha((unsigned char)*p) && idx < 255) buf[idx++] = *p++; // <-- idx++ 가 아니라 그냥 idx!
            buf[idx] = '\0';
            buf[idx] = '\0';

            // 예약어면 반드시 TOKEN_KEYWORD로 등록!
            if (is_keyword(buf)) add_token(&list, TOKEN_KEYWORD, buf);
            else 
            {
                for (int i = 0; i < idx; i++) 
                {
                    char single_var[2] = { buf[i], '\0' };
                    add_token(&list, TOKEN_VARIABLE, single_var);
                }
            }
            continue;
        }

        // [규칙 D] 사칙연산자 및 거듭제곱 기호 처리
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^') 
        {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_OPERATOR, buf);
            continue;
        }

        // [규칙 E] 여는 괄호 '('
        if (*p == '(') 
        {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_LPAREN, buf);
            continue;
        }

        // [규칙 F] 닫는 괄호 ')'
        if (*p == ')') 
        {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_RPAREN, buf);
            continue;
        }

        // [규칙 G - 추가] 다중 인자 구분용 쉼표 ','
        if (*p == ',') 
        {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_COMMA, buf);
            continue;
        }

        // 처리 불가능한 문자는 포인터를 한 칸 넘김
        p++;
    }

    // 토큰 리스트의 끝을 알리는 EOF 토큰 추가
    add_token(&list, TOKEN_EOF, "EOF");
    return list;
}

// 5. 토큰 리스트 출력 (마지막 EOF 제외하고 출력)
void print_tokens(const TokenList* list) 
{
    printf("[");
    int print_count = list->count;
    if (print_count > 0 && list->tokens[print_count - 1].type == TOKEN_EOF) print_count--;
    for (int i = 0; i < print_count; i++) 
    {
        printf("'%s'", list->tokens[i].text);
        if (i < print_count - 1) printf(", ");
    }
    printf("]\n");
}

// 6. 동적 메모리 해제
void free_token_list(TokenList* list) 
{
    if (list->tokens != NULL) { free(list->tokens);list->tokens = NULL; }
    list->count = 0; list->capacity = 0;
}