#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

// Token types
enum class TokenType
{
	NUMBER,
	IDENTIFIER,
	PLUS,
	MINUS,
	MULTIPLY,
	DIVIDE,
	ASSIGN,
	IF,
	THEN,
	ELSE,
	ENDIF,
	LPAREN,
	RPAREN,
	END
};

// Token structure
struct Token
{
	TokenType type;
	std::string value;
	int line;
	int column;
};

// AST Node structure
class ASTNode
{
public:
	virtual ~ASTNode() = default;
	virtual int evaluate() = 0;
};

// Number Node
class NumberNode : public ASTNode
{
private:
	int value;

public:
	NumberNode(int val) : value(val) {}
	int evaluate() override { return value; }
};

// Variable Node
class VariableNode : public ASTNode
{
private:
	std::string name;
	static std::map<std::string, int> variables;

public:
	VariableNode(const std::string &varName) : name(varName) {}
	int evaluate() override
	{
		if (variables.find(name) == variables.end())
		{
			throw std::runtime_error("Undefined variable: " + name);
		}
		return variables[name];
	}
	static void setVariable(const std::string &name, int value)
	{
		variables[name] = value;
	}
};

std::map<std::string, int> VariableNode::variables;

// Binary Operation Node
class BinaryOpNode : public ASTNode
{
private:
	std::shared_ptr<ASTNode> left;
	std::shared_ptr<ASTNode> right;
	TokenType op;

public:
	BinaryOpNode(std::shared_ptr<ASTNode> l, TokenType operation, std::shared_ptr<ASTNode> r)
		: left(l), op(operation), right(r) {}

	int evaluate() override
	{
		int leftVal = left->evaluate();
		int rightVal = right->evaluate();

		switch (op)
		{
		case TokenType::PLUS:
			return leftVal + rightVal;
		case TokenType::MINUS:
			return leftVal - rightVal;
		case TokenType::MULTIPLY:
			return leftVal * rightVal;
		case TokenType::DIVIDE:
			if (rightVal == 0)
				throw std::runtime_error("Division by zero");
			return leftVal / rightVal;
		default:
			throw std::runtime_error("Invalid operator");
		}
	}
};

// Assignment Node
class AssignmentNode : public ASTNode
{
private:
	std::string name;
	std::shared_ptr<ASTNode> value;

public:
	AssignmentNode(const std::string &varName, std::shared_ptr<ASTNode> val)
		: name(varName), value(val) {}

	int evaluate() override
	{
		int val = value->evaluate();
		VariableNode::setVariable(name, val);
		return val;
	}
};

// If Node
class IfNode : public ASTNode
{
private:
	std::shared_ptr<ASTNode> condition;
	std::shared_ptr<ASTNode> thenBranch;
	std::shared_ptr<ASTNode> elseBranch;

public:
	IfNode(std::shared_ptr<ASTNode> cond, std::shared_ptr<ASTNode> then, std::shared_ptr<ASTNode> else_)
		: condition(cond), thenBranch(then), elseBranch(else_) {}

	int evaluate() override
	{
		if (condition->evaluate() != 0)
		{
			return thenBranch->evaluate();
		}
		else if (elseBranch)
		{
			return elseBranch->evaluate();
		}
		return 0;
	}
};

// Lexer class
class Lexer
{
private:
	std::string input;
	size_t position;
	int line;
	int column;

	char current() const
	{
		return position < input.length() ? input[position] : '\0';
	}

	void advance()
	{
		if (current() == '\n')
		{
			line++;
			column = 1;
		}
		else
		{
			column++;
		}
		position++;
	}

	void skipWhitespace()
	{
		while (isspace(current()))
		{
			advance();
		}
	}

	Token number()
	{
		std::string result;
		while (isdigit(current()))
		{
			result += current();
			advance();
		}
		return {TokenType::NUMBER, result, line, column};
	}

	Token identifier()
	{
		std::string result;
		while (isalnum(current()) || current() == '_')
		{
			result += current();
			advance();
		}

		if (result == "if")
			return {TokenType::IF, result, line, column};
		if (result == "then")
			return {TokenType::THEN, result, line, column};
		if (result == "else")
			return {TokenType::ELSE, result, line, column};
		if (result == "endif")
			return {TokenType::ENDIF, result, line, column};

		return {TokenType::IDENTIFIER, result, line, column};
	}

public:
	Lexer(const std::string &text) : input(text), position(0), line(1), column(1) {}

	Token nextToken()
	{
		skipWhitespace();

		if (position >= input.length())
		{
			return {TokenType::END, "", line, column};
		}

		char c = current();

		if (isdigit(c))
		{
			return number();
		}

		if (isalpha(c))
		{
			return identifier();
		}

		advance();

		switch (c)
		{
		case '+':
			return {TokenType::PLUS, "+", line, column};
		case '-':
			return {TokenType::MINUS, "-", line, column};
		case '*':
			return {TokenType::MULTIPLY, "", line, column};
		case '/':
			return {TokenType::DIVIDE, "/", line, column};
		case '=':
			return {TokenType::ASSIGN, "=", line, column};
		case '(':
			return {TokenType::LPAREN, "(", line, column};
		case ')':
			return {TokenType::RPAREN, ")", line, column};
		default:
			throw std::runtime_error("Invalid character: " + std::string(1, c));
		}
	}
};

// Parser class
class Parser
{
private:
	Lexer lexer;
	Token currentToken;

	void eat(TokenType type)
	{
		if (currentToken.type == type)
		{
			currentToken = lexer.nextToken();
		}
		else
		{
			throw std::runtime_error("Unexpected token: " + currentToken.value);
		}
	}

	std::shared_ptr<ASTNode> factor()
	{
		Token token = currentToken;

		if (token.type == TokenType::NUMBER)
		{
			eat(TokenType::NUMBER);
			return std::make_shared<NumberNode>(std::stoi(token.value));
		}

		if (token.type == TokenType::IDENTIFIER)
		{
			std::string name = token.value;
			eat(TokenType::IDENTIFIER);
			return std::make_shared<VariableNode>(name);
		}

		if (token.type == TokenType::LPAREN)
		{
			eat(TokenType::LPAREN);
			auto node = expr();
			eat(TokenType::RPAREN);
			return node;
		}

		throw std::runtime_error("Invalid factor");
	}

	std::shared_ptr<ASTNode> term()
	{
		auto node = factor();

		while (currentToken.type == TokenType::MULTIPLY ||
			   currentToken.type == TokenType::DIVIDE)
		{
			Token token = currentToken;
			if (token.type == TokenType::MULTIPLY)
			{
				eat(TokenType::MULTIPLY);
			}
			else
			{
				eat(TokenType::DIVIDE);
			}
			node = std::make_shared<BinaryOpNode>(node, token.type, factor());
		}

		return node;
	}

	std::shared_ptr<ASTNode> expr()
	{
		auto node = term();

		while (currentToken.type == TokenType::PLUS ||
			   currentToken.type == TokenType::MINUS)
		{
			Token token = currentToken;
			if (token.type == TokenType::PLUS)
			{
				eat(TokenType::PLUS);
			}
			else
			{
				eat(TokenType::MINUS);
			}
			node = std::make_shared<BinaryOpNode>(node, token.type, term());
		}

		return node;
	}

	std::shared_ptr<ASTNode> statement()
	{
		if (currentToken.type == TokenType::IF)
		{
			return ifStatement();
		}

		if (currentToken.type == TokenType::IDENTIFIER)
		{
			std::string name = currentToken.value;
			eat(TokenType::IDENTIFIER);

			if (currentToken.type == TokenType::ASSIGN)
			{
				eat(TokenType::ASSIGN);
				auto value = expr();
				return std::make_shared<AssignmentNode>(name, value);
			}

			return std::make_shared<VariableNode>(name);
		}

		return expr();
	}

	std::shared_ptr<ASTNode> ifStatement()
	{
		eat(TokenType::IF);
		auto condition = expr();
		eat(TokenType::THEN);
		auto thenBranch = statement();

		std::shared_ptr<ASTNode> elseBranch = nullptr;
		if (currentToken.type == TokenType::ELSE)
		{
			eat(TokenType::ELSE);
			elseBranch = statement();
		}

		eat(TokenType::ENDIF);
		return std::make_shared<IfNode>(condition, thenBranch, elseBranch);
	}

public:
	Parser(const std::string &text) : lexer(text)
	{
		currentToken = lexer.nextToken();
	}

	std::shared_ptr<ASTNode> parse()
	{
		return statement();
	}

	int evaluate(std::shared_ptr<ASTNode> node)
	{
		return node->evaluate();
	}
};

int main()
{
	try
	{
		std::string input;
		std::cout << "Enter expression: ";
		std::getline(std::cin, input);

		Parser parser(input);
		auto ast = parser.parse();
		int result = parser.evaluate(ast);

		std::cout << "Result: " << result << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cout << "Error: " << e.what() << std::endl;
	}

	return 0;
}
