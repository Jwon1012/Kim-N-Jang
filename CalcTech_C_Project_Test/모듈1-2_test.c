#include "모듈I.h"

// II. 쪼갠 리스트를 AST 형태로 출력
// 1단계에서 선언한 TokenType 및 TokenList 구조체를 공유.
// 재귀강하파서(파서 안에서 다른 파서를 부르고 필요하면 자기가 자기를 호출하는 파서)로 작성


// 노드 동적 생성 함수 (malloc)
ASTNode* create_ast_node(ASTNodeType type, const char* value) 
//ASTNode*(주소타입) 반환형: malloc을 통해 생성한 땅의 시작 주소값을 넘겨줘야 함
{

    //1. node라는 빵에 들어갈 땅 크기 정의 및 독점
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (node == NULL) return NULL;

    //2. 해당 땅에 정보 채우기 
    node->type = type; //해당 빵이 숫자빵인지 연산빵인지 태그를 붙여서 분류
    strncpy_s(node->value, sizeof(node->value), value, _TRUNCATE); //빵을 안전하게 복사해서 팍팍 채워넣기
    //_TRUNCATE함수는 소숫점 이하를 버리는 함수
    //3. 아직 옆에는 아무도 없으니 자식 node들을 초기화
    node->left = NULL;
    node->right = NULL;

    //4. 다중 인자 멤버 안전 초기화
    for (int i = 0; i < 4; i++)
    {
        node->args[i] = NULL;
    }
    return node;
}

// AST 트리 동적 메모리 해제 함수 (재귀 해제)
void free_ast(ASTNode* node) 
{
    if (node == NULL) return;
    free_ast(node->left);
    free_ast(node->right);

    //다중 인자 연산자의 자식 트리들도 전부 해제
    if (node->type == NODE_SPECIAL_OP)
    {
        for (int i = 0; i < node->arg_count; i++)
        {
            free_ast(node->args[i]);
        }
    }
    free(node);
}

// 파서용 도우미: 현재 토큰 엿보기
Token* peek(Parser* p) {
    if (p->current < p->list->count) {
        return &(p->list->tokens[p->current]);
    }
    return NULL;
}

// 파서용 도우미: 다음 토큰으로 이동하고 반환
Token* advance(Parser* p) {
    Token* t = peek(p);
    if (t != NULL) p->current++;
    return t;
}

// ★ AST Parser 함수 선언 (상호 호출을 위해 상단 선언)
ASTNode* parse_expression(Parser* p);
ASTNode* parse_term(Parser* p);
ASTNode* parse_power(Parser* p);
ASTNode* parse_factor(Parser* p);
// 아래에서 올라가는 Bottom-Up방식 채택
// [최고 우선순위] 숫자, 변수, 괄호 ( ) 처리
ASTNode* parse_factor(Parser* p) 
{
    Token* t = peek(p);
    if (t == NULL) return NULL;

    // (1) 여는 괄호 '('를 만난 경우: 괄호 안의 전체 수식을 재귀 호출로 최우선 파싱
    if (t->type == TOKEN_LPAREN) {
        advance(p); // '(' 넘기기
        ASTNode* node = parse_expression(p);
        Token* rparen = peek(p);
        if (rparen && rparen->type == TOKEN_RPAREN) {
            advance(p); // ')' 넘기기
        }
        return node;
    }

    // (2) 숫자 토큰
    if (t->type == TOKEN_NUMBER) {
        advance(p);
        return create_ast_node(NODE_NUMBER, t->text);
    }
    // (3) 변수 또는 예약어(함수/특수연산자) 처리
    if (t->type == TOKEN_VARIABLE || t->type == TOKEN_KEYWORD) {
        Token* id_token = advance(p); // 이름("x", "sin", "lim" 등) 꺼냄
        Token* next_t = peek(p);

        // 바로 뒤에 '('가 따라오면 -> 함수 또는 특수 연산자 호출!
        if (next_t != NULL && next_t->type == TOKEN_LPAREN) 
        {
            advance(p); // '(' 소비
            const KeywordSpec* spec = find_keyword(id_token->text);

            if (spec != NULL) {
                ASTNode* node = create_ast_node(spec->type, spec->name);

                // 괄호 닫힐 때까지 콤마(,)로 구분하며 인자 파싱
                while (peek(p) != NULL && peek(p)->type != TOKEN_RPAREN) {
                    if (node->arg_count < spec->expected_args) {
                        ASTNode* arg_expr = parse_expression(p);

                        // 단일 함수(sin 등)는 기존 left 포인터에도 연결
                        if (spec->type == NODE_FUNCTION) {
                            node->left = arg_expr;
                        }
                        node->args[node->arg_count++] = arg_expr;
                    }

                    // 콤마(',')가 있으면 넘기고 다음 인자 파싱, 없으면 루프 종료
                    if (peek(p) != NULL && peek(p)->type == TOKEN_COMMA) {
                        advance(p);
                    }
                    else {
                        break;
                    }
                }

                // 닫는 괄호 ')' 소비
                if (peek(p) != NULL && peek(p)->type == TOKEN_RPAREN) {
                    advance(p);
                }
                return node;
            }
        }

        // 괄호가 안 따라오면 일반 변수(x, y 등) 노드로 반환
        return create_ast_node(NODE_VARIABLE, id_token->text);
    }


    return NULL;
}

// 2. [우선순위 3] 거듭제곱 연산자 ( ^ ) 처리
ASTNode* parse_power(Parser* p) {
    ASTNode* left = parse_factor(p);

    Token* t = peek(p);
    while (t != NULL && t->type == TOKEN_OPERATOR && strcmp(t->text, "^") == 0) {
        advance(p); // '^' 넘기기
        ASTNode* right = parse_factor(p);

        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, "^");
        op_node->left = left;
        op_node->right = right;
        left = op_node; // 생성한 부모 노드를 다음 연산의 left로 갱신

        t = peek(p);
    }
    return left;
}

// 3. [우선순위 2] 곱셈 및 나눗셈 연산자 ( * , / ) 처리
ASTNode* parse_term(Parser* p) {
    ASTNode* left = parse_power(p);

    Token* t = peek(p);
    while (t != NULL && t->type == TOKEN_OPERATOR &&
        (strcmp(t->text, "*") == 0 || strcmp(t->text, "/") == 0)) {
        Token* op_token = advance(p); // '*' 또는 '/' 가져오기
        ASTNode* right = parse_power(p);

        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, op_token->text);
        op_node->left = left;
        op_node->right = right;
        left = op_node;

        t = peek(p);
    }
    return left;
}

// 4. [우선순위 1] 덧셈 및 뺄셈 연산자 ( + , - ) 처리 [파서의 진입점]
ASTNode* parse_expression(Parser* p) {
    ASTNode* left = parse_term(p);

    Token* t = peek(p);
    while (t != NULL && t->type == TOKEN_OPERATOR &&
        (strcmp(t->text, "+") == 0 || strcmp(t->text, "-") == 0)) {
        Token* op_token = advance(p); // '+' 또는 '-' 가져오기
        ASTNode* right = parse_term(p);

        ASTNode* op_node = create_ast_node(NODE_BINARY_OP, op_token->text);
        op_node->left = left;
        op_node->right = right;
        left = op_node;

        t = peek(p);
    }
    return left;
}

// AST 트리 가로형(Left-to-Right) 시각화 및 메인 테스트 코드
// 선을 그려주며 왼쪽->오른쪽으로 확장되는 가로형 AST 출력 함수
void print_ast_horizontal(ASTNode* node, char* prefix, int is_left) {
    if (node == NULL) return;

    printf("%s", prefix);

    // 가지 기호 출력 (마지막 자식인지 중간 자식인지 구분)
    printf("%s", is_left ? "├── " : "└── ");

    // 노트 타입별 라벨 출력
    if (node->type == NODE_NUMBER) 
    {
        printf("[Var: %s]\n", node->value);
    }
    else if (node->type == NODE_VARIABLE)
    {
        printf("[Var: %s]\n", node->value);
    }
    else if (node->type == NODE_FUNCTION)
    {
        printf("[Func: %s]\n", node->value);
    }
    else if (node->type == NODE_SPECIAL_OP)
    {
        printf("[SpecialOp: %s]\n", node->value);
    }
    else
    {
        printf("[BinaryOp: %s]\n", node->value);
    }
   
    // 다음 계층으로 넘겨줄 접두사(선 연결) 버퍼 준비
    char next_prefix[512];
    snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, is_left ? "│   " : "    ");

    if (node->type == NODE_SPECIAL_OP) {
        for (int i = 0; i < node->arg_count; i++) {
            int has_next = (i < node->arg_count - 1);
            print_ast_horizontal(node->args[i], next_prefix, has_next);
        }
    }
    // 3. 기존의 왼쪽/오른쪽 자식 방문 코드를 else 안으로 넣기
    else {
        int has_left = (node->left != NULL);
        int has_right = (node->right != NULL);

        if (has_left || has_right) {
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
void print_ast_tree(ASTNode* root) {
    if (root == NULL) {
        printf("(빈 트리)\n");
        return;
    }

    // 최상위 뿌리(Root) 노드 출력
    if (root->type == NODE_BINARY_OP) {
        printf("[Op: %s]\n", root->value);
    }
    else if (root->type == NODE_NUMBER) {
        printf("[Num: %s]\n", root->value);
    }
    else {
        printf("[Var: %s]\n", root->value);
    }

    char prefix[512] = "";
    int has_left = (root->left != NULL);
    int has_right = (root->right != NULL);

    if (has_left) {
        print_ast_horizontal(root->left, prefix, has_right ? 1 : 0);
    }
    if (has_right) {
        print_ast_horizontal(root->right, prefix, 0);
    }
}