#include "모듈I.h"

// 노드 동적 생성 함수 (malloc)
ASTNode* create_ast_node(ASTNodeType type, const char* value)
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