#include "모듈I.h"

// II. 쪼갠 리스트를 AST 형태로 출력
// 1단계에서 선언한 TokenType 및 TokenList 구조체를 공유.
// 재귀강하파서(파서 안에서 다른 파서를 부르고 필요하면 자기가 자기를 호출하는 파서)로 작성
// 1. AST 노드 Type
typedef enum {
    NODE_NUMBER,    // 숫자 (예: 2, 1)
    NODE_VARIABLE,  // 변수 및 예약어 (예: x, sin)
    NODE_BINARY_OP  // 이항 연산자 (+, -, *, /, ^)
} ASTNodeType;

// 2. AST 단일 노드 구조체
typedef struct ASTNode {
    ASTNodeType type;
    char value[32];          // 값 또는 연산자 텍스트 ("2", "x", "+", "/" 등)
    struct ASTNode* left;    // 왼쪽 자식 노드 포인터
    struct ASTNode* right;   // 오른쪽 자식 노드 포인터
} ASTNode;

// 3. 파서 위치 관리 구조체
typedef struct {
    TokenList* list; // 1단계에서 쪼갠 토큰 리스트
    int current;     // 현재 읽고 있는 토큰 위치 (인덱스)
} Parser;

// 노드 동적 생성 함수 (malloc)
ASTNode* create_ast_node(ASTNodeType type, const char* value) 
//ASTNode*(주소타입) 반환형: malloc을 통해 생성한 땅의 시작 주소값을 넘겨줘야 함
{

    //1. node라는 빵에 들어갈 땅 크기 정의 및 독점
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (node == NULL) return NULL;

    //2. 해당 땅에 정보 채우기 
    node->type = type; //해당 빵이 숫자빵인지 연산빵인지 태그를 붙여서 분류
    strncpy_s(node->value, sizeof(node->value), value, 31); //빵을 안전하게 복사해서 팍팍 채워넣다가
    node->value[31] = '\0'; //공간 다 차면 공백으로 마무리

    //3. 아직 옆에는 아무도 없으니 자식 node들을 초기화
    node->left = NULL;
    node->right = NULL;

    //
    return node;
}

// AST 트리 동적 메모리 해제 함수 (재귀 해제)
void free_ast(ASTNode* node) {
    if (node == NULL) return;
    free_ast(node->left);
    free_ast(node->right);
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
// 
// [최고 우선순위] 숫자, 변수, 괄호 ( ) 처리
ASTNode* parse_factor(Parser* p) {
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

    // (3) 변수 및 예약어 토큰
    if (t->type == TOKEN_VARIABLE) {
        advance(p);
        return create_ast_node(NODE_VARIABLE, t->text);
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

    // 노드 타입별 텍스트 출력
    if (node->type == NODE_BINARY_OP) {
        printf("[Op: %s]\n", node->value);
    }
    else if (node->type == NODE_NUMBER) {
        printf("[Num: %s]\n", node->value);
    }
    else {
        printf("[Var: %s]\n", node->value);
    }

    // 다음 계층으로 넘겨줄 접두사(선 연결) 버퍼 준비
    char new_prefix[512];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_left ? "│   " : "    ");

    int has_left = (node->left != NULL);
    int has_right = (node->right != NULL);

    // 왼쪽 자식 먼저 방문 (위쪽 가지)
    if (has_left) {
        print_ast_horizontal(node->left, new_prefix, has_right ? 1 : 0);
    }

    // 오른쪽 자식 방문 (아래쪽 가지)
    if (has_right) {
        print_ast_horizontal(node->right, new_prefix, 0);
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

// [통합 테스트 메인 함수]
int main(void) {
    char input_expr[256];

    printf(" [CalcTech Core Engine] Module 1-1 & 1-2 Integrated Test\n");

    while (1) {
        printf("\n종료하시려면 'exit'을 입력하세요.\n");
        printf("수식 입력 (예시: (2*x) / (1 + x^2)) > ");

        // 1. 키보드로 한 줄 입력받기 (실패 시 또는 EOF 시 종료)
        if (fgets(input_expr, sizeof(input_expr), stdin) == NULL) {
            break;
        }

        // 2. 입력받은 문자열 끝의 엔터('\n') 제거
        input_expr[strcspn(input_expr, "\r\n")] = '\0';

        // 3. 'exit' 입력 시 무한 루프 탈출 (strcmp는 두 문자열이 같으면 0 반환)
        if (strcmp(input_expr, "'exit'") == 0) {
            printf("엔진 테스트를 종료합니다.\n");
            break;
        }

        // 4. 빈 문자열(그냥 엔터만 친 경우) 예외 처리
        if (strlen(input_expr) == 0) {
            continue;
        }

        // --- 1단계 Lexer 실행 ---
        TokenList list = tokenize(input_expr);
        printf("\n[1단계 Lexer 결과]\n");
        print_tokens(&list);

        // --- 2단계 AST Parser 실행 ---
        Parser parser = { &list, 0 };
        ASTNode* root = parse_expression(&parser);

        printf("\n[2단계 AST Parser 트리 출력 (가로 확장형)]\n");
        print_ast_tree(root);

        // --- 메모리 해제 (매 루프마다 깔끔하게 싹 지워줘야 메모리 누수가 안 남!) ---
        free_ast(root);
        free_token_list(&list);
    }

    return 0;
}