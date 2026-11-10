#include "모듈I.h"

//I. 수식을 토큰 리스트 단위로 쪼개어서 출력
// 0. 예약어 종류 정의
const char* KEYWORDS[] = {
    "sin", "cos", "tan", "sec","csc","cot","asin", "acos","atan","asec","acsc","acot","sinh","cosh","tanh","log", "ln", "sqrt",
    "lim", "sum", "int", "infty", "laplace", "exp",
    "pi", "theta"
};
const int KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

// 2. 예약어 여부 검사 함수
int is_keyword(const char* str) {
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(str, KEYWORDS[i]) == 0) {
            return 1; // 예약어 맞음
        }
    }
    return 0; // 일반 변수
}

// 토큰 리스트 메모리 초기화
void init_token_list(TokenList* list) {
    list->capacity = 16;
    list->count = 0;
    list->tokens = (Token*)malloc(sizeof(Token) * list->capacity);
    //sizeof(Token): 토큰 방 1개 만드는데 필요한 땅 크기
    //*list->capacity: 를 16개로 우선 설정
    //malloc: 땅 16개를 우선 뚫어놨으니 시작 주소 가져가라는 의미 (memory + allocation)
    //Token*: 해당 땅을 0번째 메모리부터 Token전용으로 독점하겠다는 의미
}

// 토큰 동적 추가 (용량 초과 시 메모리 2배 확장)
void add_token(TokenList* list, TokenType type, const char* text) {
    if (list->count >= list->capacity) {
        list->capacity *= 2; 
        list->tokens = (Token*)realloc(list->tokens, sizeof(Token) * list->capacity);
    }
    list->tokens[list->count].type = type; //방금 잘라낸 토큰의 종류 저장
    strncpy_s(list->tokens[list->count].text, sizeof(list->tokens[list->count].text), text, 31); //글자를 토큰 빵틀의 text 재료 안에 안전히 저장
    list->tokens[list->count].text[31] = '\0'; // 문자열 안전 널 종결
    list->count++;
}

// 4. [핵심] 문자열 수식을 토큰 리스트로 분리하는 1단계 Lexer 함수
TokenList tokenize(const char* expr) {
    TokenList list;
    init_token_list(&list);

    const char* p = expr; // 문자열 시작 주소를 포인터 p에 할당

    while (*p != '\0') {
        // [규칙 A] 공백 문자는 건너뛰기 (포인터만 1바이트 직진)
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        // [규칙 B] 숫자 읽기 (정수 및 소수점 처리)
        // ex) "2x" 입력 시 '2'를 먼저 자르고 포인터를 'x' 위치로 옮김
        if (isdigit((unsigned char)*p) || *p == '.' ) {
            char buf[256] = { 0 };
            int idx = 0;
            while (isdigit((unsigned char)*p) || *p == '.' && idx < 255) {
                buf[idx++] = *p++; // 현재 글자 넣고 포인터 1칸 직진 (*p++)
            }
            add_token(&list, TOKEN_NUMBER, buf); //만약 3.14같은 소수점을 입력한다면 buf에는 총 4칸이 저장
            continue;
        }

        // [규칙 C] 변수 및 함수 이름 읽기 (x, sin, cos 등)
        if (isalpha((unsigned char)*p)) { //주어진 정수가 알파벳 문자인지 확인
            char buf[256] = { 0 };
            int idx = 0;
            while (isalpha((unsigned char)*p) && idx < 255) {
                buf[idx++] = *p++; // 현재 글자 넣고 포인터 1칸 직진 (*p++)
            }
            buf[idx] = '\0';
            //정의한 예약어 목록(sin,cos,tan,...)에 있는지 검사 
            //여기 안에서 or조건을 통해 구현할 수는 있지만 그렇다고 코드가 극단적으로 길어지게 할 수는 X
            //컴퓨터는 lim을 극한인지, 'l''i''m'인지 판단할 능력이 없음.
            if (is_keyword(buf)){
                add_token(&list, TOKEN_VARIABLE, buf); // "lim"등으로 통쨰로 등록
            }
            else
            {//예약어가 아니면 'a','b','c'로 분리
                for (int i = 0; i < idx; i++) 
                {
                    char single_var[2] = { buf[i], '\0' };
                    add_token(&list, TOKEN_VARIABLE, single_var);
                }
            }

            continue;
        }

        // [규칙 D] 사칙연산자 및 거듭제곱 기호 처리
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^') {
            char buf[2] = { *p++, '\0' }; // 현재 문자를 넣고 주소 1칸 이동 (*p++)
            add_token(&list, TOKEN_OPERATOR, buf);
            continue;
        }

        // [규칙 E] 여는 괄호 '('
        if (*p == '(') {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_LPAREN, buf);
            continue;
        }

        // [규칙 F] 닫는 괄호 ')'
        if (*p == ')') {
            char buf[2] = { *p++, '\0' };
            add_token(&list, TOKEN_RPAREN, buf);
            continue;
        }

        // 처리 불가능한 문자는 포인터를 한 칸 넘김
        p++;
    }

    return list;
}

// 5. 토큰 리스트 예쁘게 출력하기 [ '2', 'x' ]
void print_tokens(const TokenList* list) {
    printf("[");
    for (int i = 0; i < list->count; i++) {
        printf("'%s'", list->tokens[i].text);
        if (i < list->count - 1) printf(", ");
    }
    printf("]\n");
}

// 6. 동적 메모리 해제
void free_token_list(TokenList* list) {
    if (list->tokens != NULL) {
        free(list->tokens);
        list->tokens = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

// 7. 메인 테스트 함수 (디버깅용)
int main_I(void) {
    char input_expr[256];

    printf(" [CalcTech Core Engine] Module 1 - Lexer Test\n");
    printf(" 종료하시려면 '\"exit\"'을 입력하세요.\n");

    while (1) {
        printf("수식 입력 (ex: 2x, 3*x^2 + sin(x)) > ");

        // 키보드로 공백 포함 한 줄 입력받기
        if (fgets(input_expr, sizeof(input_expr), stdin) == NULL) {
            break;
        }

        // 엔터키(\n) 제거
        input_expr[strcspn(input_expr, "\r\n")] = '\0';

        // 종료 조건 검사
        if (strcmp(input_expr, "\"exit\"")==0) {
            printf("Lexer 테스트를 종료합니다.\n");
            break;
        }

        // 입력값이 비어있으면 건너뛰기
        if (strlen(input_expr) == 0) {
            continue;
        }

        // 토큰화 수행 및 결과 출력
        TokenList list = tokenize(input_expr);

        printf(" -> 분리된 토큰 개수 : %d개\n", list.count);
        printf(" -> 토큰 리스트 결과 : ");
        print_tokens(&list);
        printf("\n");

        // 동적 메모리 해제
        free_token_list(&list);
    }

    return 0;
}