#include "CalcTech공용두꺼비집.h"

//모듈I: 사용자가 입력한 수식을 AST로 변환하기
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
    {"gamma", NODE_FUNCTION,    1}, //팩토리얼 함수

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
int is_keyword(const char* str) { return find_keyword(str) != NULL; }

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
        if (isspace((unsigned char)*p)) { p++; continue; }

        // [규칙 B] 숫자 읽기 (정수 및 소수점 처리 - 연산자 우선순위 괄호 보완)
        if (isdigit((unsigned char)*p) || *p == '.')
        {
            char buf[256] = { 0 };
            int idx = 0;
            while ((isdigit((unsigned char)*p) || *p == '.') && idx < 255) buf[idx++] = *p++;
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
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^' || *p == '=')
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

// 노드 동적 생성 함수 (malloc)
ASTNode * create_ast_node(ASTNodeType type, const char* value)
{
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (node == NULL) return NULL;

    node->type = type;
    strncpy_s(node->value, sizeof(node->value), value, _TRUNCATE);

    node->left = NULL; node->right = NULL;
    node->arg_count = 0; // [수정] 반드시 0으로 초기화! (미초기화 시 쓰레기 값으로 크래시 발생)

    for (int i = 0; i < 4; i++) node->args[i] = NULL;
    return node;
}

// AST 트리 동적 메모리 해제 함수 (재귀 해제)
void free_ast(ASTNode* node)
{
    if (node == NULL) return;
    free_ast(node->left); free_ast(node->right);

    if (node->type == NODE_SPECIAL_OP)
    {
        for (int i = 0; i < node->arg_count; i++) free_ast(node->args[i]);
    }
    free(node);
}

Token* peek(Parser* p)
{
    if (p->current < p->list->count)
    {
        return &(p->list->tokens[p->current]);
    }
    return NULL;
}

Token* advance(Parser* p)
{
    Token* t = peek(p);
    if (t != NULL) p->current++;
    return t;
}

// [최고 우선순위] 숫자, 변수, 괄호, 함수, 단항 부호 처리
ASTNode* parse_factor(Parser* p)
{
    Token* t = peek(p);
    if (t == NULL || t->type == TOKEN_EOF) return NULL;

    // (0) 단항 부호 (+x, -x) 처리
    if (t->type == TOKEN_OPERATOR && (strcmp(t->text, "-") == 0 || strcmp(t->text, "+") == 0))
    {
        Token* op = advance(p);
        ASTNode* operand = parse_factor(p);
        if (strcmp(op->text, "+") == 0)
        {
            return operand; // +x는 x와 동일
        }
        ASTNode* neg_node = create_ast_node(NODE_BINARY_OP, "-");
        neg_node->left = NULL; neg_node->right = operand;
        // 단항 마이너스 표시 (left가 NULL)
        return neg_node;
    }

    // (1) 여는 괄호 '(' 처리
    if (t->type == TOKEN_LPAREN)
    {
        advance(p); // '(' 소비
        ASTNode* node = parse_expression(p);
        if (peek(p) && peek(p)->type == TOKEN_RPAREN)
        {
            advance(p); // ')' 소비
        }
        return node;
    }

    // (2) 숫자 토큰 처리
    if (t->type == TOKEN_NUMBER)
    {
        advance(p);
        return create_ast_node(NODE_NUMBER, t->text);
    }

    // (3) 일반 변수 처리
    if (t->type == TOKEN_VARIABLE)
    {
        Token* var_token = advance(p);
        return create_ast_node(NODE_VARIABLE, var_token->text);
    }

    // (4) 예약어(함수 / 다중 인자 특수 연산자) 처리
    if (t->type == TOKEN_KEYWORD)
    {
        Token* kw_token = advance(p);
        const KeywordSpec* spec = find_keyword(kw_token->text);
        if (spec == NULL)
        {
            printf("[Error] KEYWORD_TABLE에 등록되지 않은 예약어: %s\n", kw_token->text);
            return NULL;
        }

        ASTNode* node = create_ast_node(spec->type, spec->name);

        // 예약어 뒤에 '('가 없으면 괄호 없는 형태(예: sin x)인 경우 인자 1개만 파싱
        if (peek(p) == NULL || peek(p)->type != TOKEN_LPAREN)
        {
            if (spec->type == NODE_FUNCTION)
            {
                ASTNode* arg_expr = parse_factor(p);
                node->left = arg_expr;
                if (arg_expr != NULL) node->args[node->arg_count++] = arg_expr;
            }
            return node;
        }

        advance(p); // '(' 소비

        // 닫는 괄호 ')' 또는 EOF를 만날 때까지 인자 파싱
        while (peek(p) != NULL && peek(p)->type != TOKEN_RPAREN && peek(p)->type != TOKEN_EOF)
        {
            ASTNode* arg_expr = parse_expression(p);

            if (arg_expr == NULL)
            {
                advance(p);
                break;
            }

            if (node->arg_count < spec->expected_args && node->arg_count < 4)
            {
                if (spec->type == NODE_FUNCTION)
                {
                    node->left = arg_expr;
                }
                node->args[node->arg_count++] = arg_expr;
            }
            else free_ast(arg_expr);

            if (peek(p) != NULL && peek(p)->type == TOKEN_COMMA)
            {
                advance(p); // ',' 소비 후 다음 인자 계속 파싱
            }
            else break;
        }

        if (peek(p) != NULL && peek(p)->type == TOKEN_RPAREN)
        {
            advance(p); // ')' 소비
        }
        return node;
    }
    advance(p);
    return NULL;
}

// 2. [우선순위 3] 거듭제곱 연산자 ( ^ ) 처리 (우결합: 2^3^2 -> 2^(3^2))
ASTNode* parse_power(Parser* p)
{
    ASTNode* left = parse_factor(p);

    Token* t = peek(p);
    if (t != NULL && t->type == TOKEN_OPERATOR && strcmp(t->text, "^") == 0)
    {
        advance(p); // '^' 넘기기
        ASTNode* right = parse_power(p); // 우결합을 위해 자기 자신 재귀 호출
        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, "^");
        op_node->left = left; op_node->right = right; return op_node;
    }
    return left;
}

// 3. [우선순위 2] 곱셈 및 나눗셈 연산자 ( * , / ) 및 암묵적 곱셈 (2x, 2sin(x)) 처리
ASTNode* parse_term(Parser* p)
{
    ASTNode* left = parse_power(p);

    Token* t = peek(p);
    while (t != NULL)
    {
        // 명시적 '*' 또는 '/' 연산자
        if (t->type == TOKEN_OPERATOR && (strcmp(t->text, "*") == 0 || strcmp(t->text, "/") == 0))
        {
            Token* op_token = advance(p);
            ASTNode* right = parse_power(p);

            ASTNode* op_node = create_ast_node(NODE_BINARY_OP, op_token->text);
            op_node->left = left;
            op_node->right = right;
            left = op_node;
        }
        // 암묵적 곱셈: 숫자/변수/괄호 바로 뒤에 변수, 함수, 여는 괄호가 올 때 (예: 2x, 3sin(x))
        else if (t->type == TOKEN_VARIABLE || t->type == TOKEN_KEYWORD || t->type == TOKEN_LPAREN)
        {
            ASTNode* right = parse_power(p);
            ASTNode* op_node = create_ast_node(NODE_BINARY_OP, "*");
            op_node->left = left; op_node->right = right;
            left = op_node;
        }
        else break;
        t = peek(p);
    }
    return left;
}

// 4. [우선순위 1] 덧셈 및 뺄셈 연산자 ( + , - ) 처리 [파서의 진입점]
ASTNode* parse_expression(Parser* p)
{
    ASTNode* left = parse_term(p);

    Token* t = peek(p);
    while (t != NULL && t->type == TOKEN_OPERATOR &&
        (strcmp(t->text, "+") == 0 || strcmp(t->text, "-") == 0))
    {
        Token* op_token = advance(p);
        ASTNode* right = parse_term(p);
        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, op_token->text);
        op_node->left = left;
        op_node->right = right;
        left = op_node;
        t = peek(p);
    }

    if (t != NULL && t->type == TOKEN_OPERATOR && strcmp(t->text, "=") == 0)
    {
        Token* op_token = advance(p);
        ASTNode* right = parse_expression(p); // 우변 전체를 재귀로 파싱
        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, op_token->text);
        op_node->left = left;
        op_node->right = right;
        return op_node;
    }
    return left;
}

// 가로형 AST 출력 함수
void print_ast_horizontal(ASTNode* node, char* prefix, int is_left)
{
    if (node == NULL) return;

    printf("%s", prefix);
    printf("%s", is_left ? "├── " : "└── ");

    if (node->type == NODE_NUMBER) {
        printf("[Num: %s]\n", node->value); // [수정] Var -> Num
    }
    else if (node->type == NODE_VARIABLE) {
        printf("[Var: %s]\n", node->value);
    }
    else if (node->type == NODE_FUNCTION) {
        printf("[Func: %s]\n", node->value);
    }
    else if (node->type == NODE_SPECIAL_OP) {
        printf("[SpecialOp: %s]\n", node->value);
    }
    else {
        printf("[BinaryOp: %s]\n", node->value);
    }

    char next_prefix[512];
    snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, is_left ? "│   " : "    ");

    if (node->type == NODE_SPECIAL_OP)
    {
        for (int i = 0; i < node->arg_count; i++)
        {
            int has_next = (i < node->arg_count - 1);
            print_ast_horizontal(node->args[i], next_prefix, has_next);
        }
    }
    else
    {
        int has_left = (node->left != NULL);
        int has_right = (node->right != NULL);

        if (has_left || has_right)
        {
            if (has_left) {
                print_ast_horizontal(node->left, next_prefix, has_right);
            }
            if (has_right) {
                print_ast_horizontal(node->right, next_prefix, 0);
            }
        }
    }
}

// 최초 호출용 래퍼 함수
void print_ast_tree(ASTNode* root)
{
    if (root == NULL) {
        printf("(빈 트리)\n");
        return;
    }

    // [수정] 모든 루트 노드 타입(함수, 특수연산자 포함) 출력 지원
    if (root->type == NODE_BINARY_OP) {
        printf("[Op: %s]\n", root->value);
    }
    else if (root->type == NODE_NUMBER) {
        printf("[Num: %s]\n", root->value);
    }
    else if (root->type == NODE_FUNCTION) {
        printf("[Func: %s]\n", root->value);
    }
    else if (root->type == NODE_SPECIAL_OP) {
        printf("[SpecialOp: %s]\n", root->value);
    }
    else {
        printf("[Var: %s]\n", root->value);
    }

    char prefix[512] = "";

    // [수정] 루트가 다중 인자 연산자(lim, sum, int)일 때도 자식 출력
    if (root->type == NODE_SPECIAL_OP) {
        for (int i = 0; i < root->arg_count; i++) {
            int has_next = (i < root->arg_count - 1);
            print_ast_horizontal(root->args[i], prefix, has_next);
        }
    }
    else {
        int has_left = (root->left != NULL);
        int has_right = (root->right != NULL);

        if (has_left) {
            print_ast_horizontal(root->left, prefix, has_right ? 1 : 0);
        }
        if (has_right) {
            print_ast_horizontal(root->right, prefix, 0);
        }
    }
}

// [내부 도우미 함수] 버퍼 오버플로우를 막으면서 안전하게 문자열을 이어 붙이는 함수
extern void append_str(char* buf, size_t max_len, const char* str) 
{
    if (buf == NULL || str == NULL) return;
    strncat_s(buf, max_len, str, _TRUNCATE);
}

// ★ AST 트리를 재귀적으로 순회하며 LaTeX 문자열 조립
void ast_to_latex(const ASTNode* node, char* out_buf, size_t max_len) 
{
    if (node == NULL || out_buf == NULL) return;

    // 1. 숫자(NODE_NUMBER) 또는 변수(NODE_VARIABLE)
    if (node->type == NODE_NUMBER || node->type == NODE_VARIABLE) 
    {
        if (strcmp(node->value, "inf") == 0 || strcmp(node->value, "infty") == 0) append_str(out_buf, max_len, "\\infty");
        else if (strcmp(node->value, "pi") == 0)  append_str(out_buf, max_len, "\\pi");
        else append_str(out_buf, max_len, node->value);
        return;
    }

    // 2. 이항 연산자 (+, -, *, /, ^) 및 단항 음수 (-x)
    if (node->type == NODE_BINARY_OP) 
    {
        if (strcmp(node->value, "/") == 0) 
        {
            append_str(out_buf, max_len, "\\frac{");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "}{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "^") == 0) 
        {
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "^{");
            ast_to_latex(node->right, out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "*") == 0) 
        {
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, " \\cdot ");
            ast_to_latex(node->right, out_buf, max_len);
        }
        else 
        {
            // 단항 마이너스(-x)인 경우 left가 NULL
            if (node->left == NULL && strcmp(node->value, "-") == 0) 
            {
                append_str(out_buf, max_len, "-");
                ast_to_latex(node->right, out_buf, max_len);
            }
            else 
            {
                ast_to_latex(node->left, out_buf, max_len);
                append_str(out_buf, max_len, " ");
                append_str(out_buf, max_len, node->value);
                append_str(out_buf, max_len, " ");
                ast_to_latex(node->right, out_buf, max_len); 
            }
        }
        return;
    }

    // 3. 단일 인자 수학 함수 (sin, cos, ln, exp 등)
    if (node->type == NODE_FUNCTION)
    {
        if (strcmp(node->value, "sqrt") == 0) {
            append_str(out_buf, max_len, "\\sqrt{");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "}");
            return;
        }
        if (strcmp(node->value, "abs") == 0) { //절댓값도 일종의 함수
            append_str(out_buf, max_len, "\\left|");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "\\right|");
            return;
        }
        if (strcmp(node->value, "prime") == 0) { //prime은 뉴턴식 미분, 
            append_str(out_buf, max_len, "\\left("); //f'(x)로 쓸 수도 있지만 여러번 미분하는 케이스도 고려
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "\\right)'");
            return;
        }
        if (strcmp(node->value, "gamma") == 0) {
            append_str(out_buf, max_len, "\\left(");
            ast_to_latex(node->left, out_buf, max_len);
            append_str(out_buf, max_len, "-1\\right)!");
            return;
        }
        // 기존 일반 삼각·로그 함수 처리...
        append_str(out_buf, max_len, "\\");
        append_str(out_buf, max_len, node->value);
        append_str(out_buf, max_len, "\\left(");
        ast_to_latex(node->left, out_buf, max_len);
        append_str(out_buf, max_len, "\\right)");
        return;
    }

    // 4. 다중 인자 미적분 연산자 (lim, sum, int)
    if (node->type == NODE_SPECIAL_OP) 
    {
        if (strcmp(node->value, "lim") == 0 && node->arg_count == 3) 
        {
            append_str(out_buf, max_len, "\\lim_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, " \\to ");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
        }
        else if (strcmp(node->value, "sum") == 0 && node->arg_count == 4) 
        {
            append_str(out_buf, max_len, "\\sum_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "=");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        else if (strcmp(node->value, "int") == 0 && node->arg_count == 4) 
        {
            append_str(out_buf, max_len, "\\int_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}^{");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "} ");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, " \\, d");
            ast_to_latex(node->args[3], out_buf, max_len);
        }
        else if (strcmp(node->value, "frac") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\frac{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}{");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "}");
        }
        else if (strcmp(node->value, "logbase") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\log_{");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, "}\\left(");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "\\right)");
        }
        else if (strcmp(node->value, "indef_int") == 0 && node->arg_count == 2) 
        {
            append_str(out_buf, max_len, "\\int ");
            ast_to_latex(node->args[0], out_buf, max_len);
            append_str(out_buf, max_len, " \\, d");
            ast_to_latex(node->args[1], out_buf, max_len);
        }
        else if (strcmp(node->value, "diff") == 0 && node->arg_count == 3) 
        {
            // \frac{d^{차수}}{d변수^{차수}} 피미분식
            append_str(out_buf, max_len, "\\frac{d^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "}}{d");
            ast_to_latex(node->args[1], out_buf, max_len);
            append_str(out_buf, max_len, "^{");
            ast_to_latex(node->args[2], out_buf, max_len);
            append_str(out_buf, max_len, "}} ");
            ast_to_latex(node->args[0], out_buf, max_len);
        }
        return;
    }
}


