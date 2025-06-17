#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cctype>

using namespace std;

//-----------------//
//   Tokenizer     //
//-----------------//

enum class TokenType
{
	IDENT,
	NUMBER,
	IF,
	LBRACE,
	RBRACE,
	LPAREN,
	RPAREN,
	SEMICOLON,
	PLUS,
	MINUS,
	EQUAL,
	END
};

struct Token
{
	TokenType type;
	string text;
	int value; // for NUMBER tokens
};

class Lexer
{
public:
	Lexer(const string &input) : input(input), pos(0) {}

	Token getNextToken()
	{
		skipWhitespace();
		if (pos >= input.size())
			return {TokenType::END, "", 0};

		char currentChar = input[pos];

		// Single-character tokens.
		if (currentChar == '{')
		{
			pos++;
			return {TokenType::LBRACE, "{", 0};
		}
		if (currentChar == '}')
		{
			pos++;
			return {TokenType::RBRACE, "}", 0};
		}
		if (currentChar == '(')
		{
			pos++;
			return {TokenType::LPAREN, "(", 0};
		}
		if (currentChar == ')')
		{
			pos++;
			return {TokenType::RPAREN, ")", 0};
		}
		if (currentChar == ';')
		{
			pos++;
			return {TokenType::SEMICOLON, ";", 0};
		}
		if (currentChar == '+')
		{
			pos++;
			return {TokenType::PLUS, "+", 0};
		}
		if (currentChar == '-')
		{
			pos++;
			return {TokenType::MINUS, "-", 0};
		}
		if (currentChar == '=')
		{
			pos++;
			return {TokenType::EQUAL, "=", 0};
		}

		// Number.
		if (isdigit(currentChar))
		{
			int start = pos;
			while (pos < input.size() && isdigit(input[pos]))
				pos++;
			int num = stoi(input.substr(start, pos - start));
			return {TokenType::NUMBER, input.substr(start, pos - start), num};
		}

		// Identifier or reserved word "if".
		if (isalpha(currentChar) || currentChar == '_')
		{
			int start = pos;
			while (pos < input.size() && (isalnum(input[pos]) || input[pos] == '_'))
				pos++;
			string word = input.substr(start, pos - start);
			if (word == "if")
				return {TokenType::IF, word, 0};
			return {TokenType::IDENT, word, 0};
		}

		throw runtime_error(string("Unexpected character: ") + currentChar);
	}

	// Peek at the next token without consuming it.
	Token peekToken()
	{
		size_t tempPos = pos;
		Token token = getNextToken();
		pos = tempPos;
		return token;
	}

private:
	void skipWhitespace()
	{
		while (pos < input.size() && isspace(input[pos]))
			pos++;
	}
	string input;
	size_t pos;
};

//---------------------------//
//       AST Declarations    //
//---------------------------//

// Environment for variable storage.
using Env = unordered_map<string, int>;

// Base class for expressions.
struct Expr
{
	virtual ~Expr() {}
	virtual int evaluate(Env &env) const = 0;
};

struct NumberExpr : public Expr
{
	int value;
	NumberExpr(int value) : value(value) {}
	int evaluate(Env &env) const override
	{
		return value;
	}
};

struct VariableExpr : public Expr
{
	string name;
	VariableExpr(const string &name) : name(name) {}
	int evaluate(Env &env) const override
	{
		if (env.find(name) == env.end())
			throw runtime_error("Undefined variable: " + name);
		return env[name];
	}
};

struct BinaryExpr : public Expr
{
	char op; // '+' or '-'
	unique_ptr<Expr> left, right;
	BinaryExpr(char op, unique_ptr<Expr> left, unique_ptr<Expr> right)
		: op(op), left(move(left)), right(move(right)) {}
	int evaluate(Env &env) const override
	{
		int l = left->evaluate(env);
		int r = right->evaluate(env);
		if (op == '+')
			return l + r;
		if (op == '-')
			return l - r;
		throw runtime_error("Unknown operator");
	}
};

// Base class for statements.
struct Statement
{
	virtual ~Statement() {}
	virtual void execute(Env &env) const = 0;
};

struct AssignmentStmt : public Statement
{
	string var;
	unique_ptr<Expr> expr;
	AssignmentStmt(const string &var, unique_ptr<Expr> expr)
		: var(var), expr(move(expr)) {}
	void execute(Env &env) const override
	{
		env[var] = expr->evaluate(env);
	}
};

struct IfStmt : public Statement
{
	unique_ptr<Expr> condition;
	unique_ptr<Statement> stmt;
	IfStmt(unique_ptr<Expr> condition, unique_ptr<Statement> stmt)
		: condition(move(condition)), stmt(move(stmt)) {}
	void execute(Env &env) const override
	{
		if (condition->evaluate(env) != 0)
			stmt->execute(env);
	}
};

struct CompoundStmt : public Statement
{
	vector<unique_ptr<Statement>> statements;
	void execute(Env &env) const override
	{
		for (auto &stmt : statements)
			stmt->execute(env);
	}
};

//---------------------------//
//         Parser            //
//---------------------------//

class Parser
{
public:
	Parser(Lexer &lexer) : lexer(lexer)
	{
		current = lexer.getNextToken();
	}

	// Parse a program: Prog -> { Deyimler } END.
	unique_ptr<CompoundStmt> parseProgram()
	{
		auto program = make_unique<CompoundStmt>();
		eat(TokenType::LBRACE);
		while (current.type == TokenType::IDENT || current.type == TokenType::IF)
		{
			program->statements.push_back(parseStatement());
		}
		eat(TokenType::RBRACE);
		if (current.type != TokenType::END)
		{
			throw runtime_error("Expected end of input");
		}
		return program;
	}

private:
	Lexer &lexer;
	Token current;

	void eat(TokenType type)
	{
		if (current.type == type)
			current = lexer.getNextToken();
		else
			throw runtime_error("Unexpected token: " + current.text);
	}

	// Statement: Deyim -> id = Exp ; | if ( Exp ) Deyim
	unique_ptr<Statement> parseStatement()
	{
		if (current.type == TokenType::IDENT)
		{
			string varName = current.text;
			eat(TokenType::IDENT);
			eat(TokenType::EQUAL);
			auto expr = parseExpr();
			eat(TokenType::SEMICOLON);
			return make_unique<AssignmentStmt>(varName, move(expr));
		}
		else if (current.type == TokenType::IF)
		{
			eat(TokenType::IF);
			eat(TokenType::LPAREN);
			auto condition = parseExpr();
			eat(TokenType::RPAREN);
			auto stmt = parseStatement();
			return make_unique<IfStmt>(move(condition), move(stmt));
		}
		else
		{
			throw runtime_error("Unexpected token in statement: " + current.text);
		}
	}

	// Expression: Exp -> term expt
	unique_ptr<Expr> parseExpr()
	{
		auto left = parseTerm();
		return parseExpt(move(left));
	}

	// Expt -> + Exp | - Exp | epsilon
	unique_ptr<Expr> parseExpt(unique_ptr<Expr> left)
	{
		if (current.type == TokenType::PLUS || current.type == TokenType::MINUS)
		{
			char op = (current.type == TokenType::PLUS) ? '+' : '-';
			eat(current.type);
			auto right = parseExpr();
			auto bin = make_unique<BinaryExpr>(op, move(left), move(right));
			return bin;
		}
		// epsilon production: just return left
		return left;
	}

	// term -> NUMBER | IDENT
	unique_ptr<Expr> parseTerm()
	{
		if (current.type == TokenType::NUMBER)
		{
			int num = current.value;
			eat(TokenType::NUMBER);
			return make_unique<NumberExpr>(num);
		}
		else if (current.type == TokenType::IDENT)
		{
			string varName = current.text;
			eat(TokenType::IDENT);
			return make_unique<VariableExpr>(varName);
		}
		throw runtime_error("Expected NUMBER or IDENT in term, got: " + current.text);
	}
};

//---------------------------//
//           Main            //
//---------------------------//

int main(int argc, char *argv[])
{
	string input;
	if (argc >= 2)
	{
		input = argv[1];
	}
	else
	{
		cout << "Enter program:\n";
		getline(cin, input);
	}

	try
	{
		Lexer lexer(input);
		Parser parser(lexer);
		auto programAST = parser.parseProgram();

		Env env;
		programAST->execute(env);

		cout << "Final variables:\n";
		for (auto &kv : env)
		{
			cout << kv.first << " = " << kv.second << "\n";
		}
	}
	catch (const exception &ex)
	{
		cerr << "Error: " << ex.what() << "\n";
		return 1;
	}
	return 0;
}
