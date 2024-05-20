#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#define LINE_BUF_SIZE 256

int heighestIndex;
const char *type = "uint_fast64_t";
const char *typePrintMacro = "PRIuFAST64";
char *file = "a";
char *name = "program";

enum InstructionType {
	undefinedType = 0,
	assignment,
	loopInstruction,
	whileInstruction,
	ifInstructionStart,
	ifInstructionEnd
};

enum Operation {
	undefinedOperation = 0,
	constant,
	variable,
	plus,
	minus,
	times,
	dividedBy,
	modulo,
	equal,
	notEqual,
	greater,
	greaterEqual,
	less,
	lessEqual
};

typedef struct Program {
	enum InstructionType instructionType;
	enum Operation operation;
	int treatCAsVariable;
	uint64_t i, j, c;
	struct Program *innerProgram;
	struct Program *nextProgram;
} Program;

typedef struct ProgramStack {
	Program *program;
	struct ProgramStack *next;
} ProgramStack;

typedef struct LineReader {
	char *inputFileName;
	FILE *input;
	char *buf;
	int size;
	int line;
	int segment;
	int position;
	int endOfLine;
} LineReader;

typedef struct ParserOptions {
	char *inputFileName;
	int extensionWhile;
	int extensionOperations;
	int extensionAssignment;
	int noWhitespace;
	int extensionIf;
	int extensionIfExtended;
	int extensionWhileExtended;
} ParserOptions;

typedef struct WriteOptions {
	char *outputFileName;
	char *functionName;
	int extensionHeader;
} WriteOptions;

void error(char *message) {
	fprintf(stderr, "loop: error: %s\n", message);
	exit(EXIT_FAILURE);
}

void help() {
	char *message =
		"Usage: ./loop [options] file\n"
		"Options:\n"
		"  --help             -h           Display this information.\n"
		"  --version          -v           Display version information.\n"
		"  --output <file>    -o <file>    Place the output into <file>. (Default: \"%s\")\n"
		"  --name <name>      -n <name>    Name the function that gets generated <name>. (Default: \"%s\")\n"
		"  --header           -H           Also generate and include a header file.\n"
		"  --operations       -O           Also accept multiplication, division, and modulo.\n"
		"  --assignment       -a           Also accept various different assignments.\n"
		"  --if               -i           Also accept basic IF programs.\n"
		"  --ifExtended       -I           Also accept various different IF programs.\n"
		"  --while            -w           Also accept basic WHILE programs.\n"
		"  --whileExtended    -W           Also accept various different WHILE programs.\n"
		"  --noWhitespace     -N           Also accept programs with missing whitespace.\n"
		"  --klausur          -k           The same as -O -a -I.\n";
	printf(message, file, name);
}

void version() {
	char *message =
		"LOOP to C transpiler 1.0\n"
		"Kai Hallmann, 2021\n";
	printf(message);
}

void changeWhitespaceToSpaces(char *string) {
	if (string == NULL) return;
	for (int i = 0; i < strlen(string); ++i)
		if (strchr("\t\r\n", string[i]))
			string[i] = ' ';
}

void parserError(LineReader *lineReader, char *message) {
	if (lineReader->inputFileName == NULL) error("Input file name not set");
	fprintf(stderr, "%s:%d:%d: error: %s\n", lineReader->inputFileName, lineReader->line, lineReader->segment * (lineReader->size - 1) + lineReader->position, message);
	changeWhitespaceToSpaces(lineReader->buf);
	fprintf(stderr, "%s\n", lineReader->buf);
	for (int i = 1; i < lineReader->position; ++i)
		fprintf(stderr, " ");
	fprintf(stderr, "^");
	exit(EXIT_FAILURE);
}

Program *newProgram() {
	Program *program = (Program *) calloc(1, sizeof(Program));
}

void freeProgram(Program *program) {
	if (program == NULL) return;
	freeProgram(program->innerProgram);
	freeProgram(program->nextProgram);
	free(program);
}

ProgramStack *newProgramStack() {
	return NULL;
}

void push(ProgramStack **programStack, Program *program) {
	ProgramStack *newProgramStack = (ProgramStack *) malloc(sizeof(ProgramStack));
	newProgramStack->program = program;
	newProgramStack->next = *programStack;
	*programStack = newProgramStack;
}

Program *pop(ProgramStack **programStack) {
	if (programStack == NULL || *programStack == NULL) return NULL;
	ProgramStack *oldProgramStack = *programStack;
	Program *program = oldProgramStack->program;
	*programStack = oldProgramStack->next;
	free(oldProgramStack);
	return program;
}

void freeProgramStack(ProgramStack *programStack) {
	if (programStack == NULL) return;
	freeProgramStack(programStack->next);
	free(programStack);
}

LineReader *newLineReader(char *inputFileName, int size) {
	LineReader *lineReader = malloc(sizeof(LineReader));
	lineReader->inputFileName = inputFileName;
	lineReader->input = fopen(inputFileName, "r");
	if (lineReader->input == NULL) error(strerror(errno));
	lineReader->buf = malloc(size);
	lineReader->size = size;
	lineReader->line = 0;
	lineReader->segment = 0;
	lineReader->position = 0;
	lineReader->endOfLine = 1;
	return lineReader;
}

void freeLineReader(LineReader *lineReader) {
	if (lineReader == NULL) return;
	free(lineReader->buf);
	fclose(lineReader->input);
	free(lineReader);
}


void adjustOutputFileName(char **outputFileName) {
	if (outputFileName == NULL || *outputFileName == NULL) error("Unexpected output file name null pointer");
	int length = strlen(*outputFileName);
	char *name = malloc(length + 2);
	strcpy(name, *outputFileName);
	if (length < 2 || name[length - 2] != '.' || name[length - 1] != 'c')
		strcat(name, ".c");
	*outputFileName = name;
}

void handleArguments(int argc, char **argv, ParserOptions *parserOptions, WriteOptions *writeOptions) {
	struct option longOptions[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"output", required_argument, NULL, 'o'},
		{"while", no_argument, NULL, 'w'},
		{"header", no_argument, NULL, 'H'},
		{"name", required_argument, NULL, 'n'},
		{"operations", no_argument, NULL, 'O'},
		{"assignment", no_argument, NULL, 'a'},
		{"noWhitespace", no_argument, NULL, 'N'},
		{"if", no_argument, NULL, 'i'},
		{"ifExtended", no_argument, NULL, 'I'},
		{"whileExtended", no_argument, NULL, 'W'},
		{"klausur", no_argument, NULL, 'k'},
		{NULL, 0, NULL, 0}
	};
	while (1) {
		int index = 0;
		int c = getopt_long(argc, argv, "hvo:wn:HOaNiIWk", longOptions, &index);
		if (c == EOF) break;
		switch (c) {
		case 'h':
			help();
			exit(EXIT_SUCCESS);
		case 'v':
			version();
			exit(EXIT_SUCCESS);
		case 'o':
			writeOptions->outputFileName = optarg;
			break;
		case 'w':
			parserOptions->extensionWhile = 1;
			break;
		case 'H':
			writeOptions->extensionHeader = 1;
			break;
		case 'n':
			writeOptions->functionName = optarg;
			break;
		case 'O':
			parserOptions->extensionOperations = 1;
			break;
		case 'a':
			parserOptions->extensionAssignment = 1;
			break;
		case 'N':
			parserOptions->noWhitespace = 1;
			break;
		case 'i':
			parserOptions->extensionIf = 1;
			break;
		case 'I':
			parserOptions->extensionIf = 1;
			parserOptions->extensionIfExtended = 1;
			break;
		case 'W':
			parserOptions->extensionWhile = 1;
			parserOptions->extensionWhileExtended = 1;
			break;
		case 'k':
			parserOptions->extensionOperations = 1;
			parserOptions->extensionAssignment = 1;
			parserOptions->extensionIf = 1;
			parserOptions->extensionIfExtended = 1;
			break;
		case '?':
			break;
		default:
			error("Unknown argument parser error");
		}
	}
	if (optind >= argc) error("No input file");
	if (optind < argc - 1) error("Too many input files");
	parserOptions->inputFileName = argv[optind];
	adjustOutputFileName(&(writeOptions->outputFileName));
}

char getChar(LineReader *lineReader) {
	while (1) {
		if (lineReader->position >= lineReader->size) error("Invalid line reader buffer position");
		char c = lineReader->buf[lineReader->position++];
		if (c != '\0') return c;
		char *res = fgets(lineReader->buf, lineReader->size, lineReader->input);
		if (res == NULL) return EOF;
		if (lineReader->endOfLine) {
			lineReader->line++;
			lineReader->segment = 0;
			lineReader->position = 0;
			lineReader->endOfLine = 0;
		} else {
			lineReader->segment++;
			lineReader->position = 0;
		}
		int len = strlen(res);
		if (len > 0 && res[len - 1] == '\n')
			lineReader->endOfLine = 1;
	}
	return EOF;
}

int consumeWhitespace(LineReader *lineReader, int minimum, ParserOptions *parserOptions) {
	int count = 0;
	char c;
	while (c = getChar(lineReader), strchr(" \t\r\n", c))
		++count;
	if (count < minimum && !parserOptions->noWhitespace) {
		if (c == EOF) parserError(lineReader, "Unexpected end of file");
		else parserError(lineReader, "Expected whitespace");
	}
	lineReader->position--;
	return count;
}

int parseNumber(LineReader *lineReader) {
	int count = 0, x = 0;
	char c;
	while (c = getChar(lineReader), c >= '0' && c <= '9') {
		x *= 10;
		x += c - '0';
		++count;
	}
	if (count < 1) {
		if (c == EOF)
			parserError(lineReader, "Unexpected end of file");
		else
			parserError(lineReader, "Expected number");
	}
	lineReader->position--;
	return x;
}

void consumeString(LineReader *lineReader, char *string) {
	if (string == NULL) return;
	for (int i = 0; i < strlen(string); ++i) {
		char c = getChar(lineReader);
		if (c != string[i]) {
			if (c == EOF) parserError(lineReader, "Unexpected end of file");
			char buf[20];
			sprintf(buf, "Expected '%c'", string[i]);
			parserError(lineReader, buf);
		}
	}
}

void parseAssignment(Program *program, LineReader *lineReader, ParserOptions *parserOptions, int *count) {
	char c;
	program->instructionType = assignment;
	program->i = parseNumber(lineReader);
	if (program->i > heighestIndex)
		heighestIndex = program->i;
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, ":=");
	consumeWhitespace(lineReader, 1, parserOptions);
	if (parserOptions->extensionAssignment) {
		c = getChar(lineReader);
		lineReader->position--;
		if (c >= '0' && c <= '9') {
			program->operation = constant;
			program->c = parseNumber(lineReader);
			*count = consumeWhitespace(lineReader, 0, parserOptions);
			return;
		}
	}
	consumeString(lineReader, "x");
	program->j = parseNumber(lineReader);
	if (program->j > heighestIndex)
		heighestIndex = program->j;
	if (parserOptions->extensionAssignment) {
		*count = consumeWhitespace(lineReader, 0, parserOptions);
		c = getChar(lineReader);
		lineReader->position--;
		if (c == ';' || c == 'E' || c == EOF) {
			if (!parserOptions->noWhitespace && c == 'E' && *count == 0) parserError(lineReader, "Expected whitespace");
			program->operation = variable;
			return;
		}
	} else
		consumeWhitespace(lineReader, 1, parserOptions);
	c = getChar(lineReader);
	if (c == '+') {
		program->operation = plus;
	} else if (c == '-') {
		program->operation = minus;
	} else if (parserOptions->extensionOperations && c == '*') {
		program->operation = times;
	} else if (parserOptions->extensionOperations && c == 'D') {
		consumeString(lineReader, "IV");
		program->operation = dividedBy;
	} else if (parserOptions->extensionOperations && c == 'M') {
		consumeString(lineReader, "OD");
		program->operation = modulo;
	} else if (c == EOF) {
		parserError(lineReader, "Unexpected end of file");
	} else {
		if (parserOptions->extensionOperations) parserError(lineReader, "Expected '+', '-', '*', \"DIV\", or \"MOD\"");	
		else parserError(lineReader, "Expected '+' or '-'");	
	}
	consumeWhitespace(lineReader, 1, parserOptions);
	if (parserOptions->extensionAssignment) {
		c = getChar(lineReader);
		if (c == 'x')
			program->treatCAsVariable = 1;
		else if (c >= '0' && c <= '9')
			lineReader->position--;
		else parserError(lineReader, "Expected a variable or number");
	}
	program->c = parseNumber(lineReader);
	*count = consumeWhitespace(lineReader, 0, parserOptions);
}

void parseLoop(Program *program, LineReader *lineReader, ParserOptions *parserOptions) {
	program->instructionType = loopInstruction;
	consumeString(lineReader, "OOP");
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "x");
	program->i = parseNumber(lineReader);
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "DO");
	consumeWhitespace(lineReader, 1, parserOptions);
}

void parseWhile(Program *program, LineReader *lineReader, ParserOptions *parserOptions) {
	program->instructionType = whileInstruction;
	consumeString(lineReader, "HILE");
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "x");
	program->i = parseNumber(lineReader);
	consumeWhitespace(lineReader, 1, parserOptions);
	char c = getChar(lineReader);
	if (c == '!') {
		consumeString(lineReader, "=");
		program->operation = notEqual;
	} else {
		if (parserOptions->extensionWhileExtended) {
			if (c == '=') {
				program->operation = equal;
			} else if (c == '>') {
				c = getChar(lineReader);
				if (c == '=') {
					program->operation = greaterEqual;
				} else {
					lineReader->position--;
					program->operation = greater;
				}
			} else if (c == '<') {
				c = getChar(lineReader);
				if (c == '=') {
					program->operation = lessEqual;
				} else {
					lineReader->position--;
					program->operation = less;
				}
			} else parserError(lineReader, "Expected \"=\", \"!=\", \">\", \">=\", \"<\", or \"<=\"");
		} else parserError(lineReader, "Expected \"!=\"");
	}
	consumeWhitespace(lineReader, 1, parserOptions);
	if (parserOptions->extensionWhileExtended) {
		c = getChar(lineReader);
		if (c == 'x') {
			program->treatCAsVariable = 1;
		} else {
			lineReader->position--;
		}
		program->c = parseNumber(lineReader);
	} else {
		consumeString(lineReader, "0");
	}
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "DO");
	consumeWhitespace(lineReader, 1, parserOptions);
}

void parseIf(Program *program, LineReader *lineReader, ParserOptions *parserOptions) {
	program->instructionType = ifInstructionStart;
	consumeString(lineReader, "F");
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "x");
	program->i = parseNumber(lineReader);
	consumeWhitespace(lineReader, 1, parserOptions);
	char c = getChar(lineReader);
	if (c == '=') {
		program->operation = equal;
	} else {
		if (parserOptions->extensionIfExtended) {
			if (c == '!') {
				consumeString(lineReader, "=");
				program->operation = notEqual;
			} else if (c == '>') {
				c = getChar(lineReader);
				if (c == '=') {
					program->operation = greaterEqual;
				} else {
					lineReader->position--;
					program->operation = greater;
				}
			} else if (c == '<') {
				c = getChar(lineReader);
				if (c == '=') {
					program->operation = lessEqual;
				} else {
					lineReader->position--;
					program->operation = less;
				}
			} else parserError(lineReader, "Expected \"=\", \"!=\", \">\", \">=\", \"<\", or \"<=\"");
		} else parserError(lineReader, "Expected '='");
	}
	consumeWhitespace(lineReader, 1, parserOptions);
	if (parserOptions->extensionIfExtended) {
		c = getChar(lineReader);
		if (c == 'x') {
			program->treatCAsVariable = 1;
		} else {
			lineReader->position--;
		}
		program->c = parseNumber(lineReader);
	} else {
		consumeString(lineReader, "0");
	}
	consumeWhitespace(lineReader, 1, parserOptions);
	consumeString(lineReader, "THEN");
	consumeWhitespace(lineReader, 1, parserOptions);
}

Program *parse(ParserOptions *parserOptions) {
	heighestIndex = 0;
	Program *ret = newProgram();
	Program *program = ret;
	ProgramStack *programStack = newProgramStack();
	LineReader *lineReader = newLineReader(parserOptions->inputFileName, LINE_BUF_SIZE);
	int count;
	
	while (1) {
		consumeWhitespace(lineReader, 0, parserOptions);
		char c = getChar(lineReader);
		if (c == EOF) parserError(lineReader, "Unexpected end of file");
		else if (c == 'x')
			parseAssignment(program, lineReader, parserOptions, &count);
		else if (c == 'L') {
			parseLoop(program, lineReader, parserOptions);
			program->innerProgram = newProgram();
			push(&programStack, program);
			program = program->innerProgram;
			continue;
		} else if (c == 'W' && parserOptions->extensionWhile) {
			parseWhile(program, lineReader, parserOptions);
			program->innerProgram = newProgram();
			push(&programStack, program);
			program = program->innerProgram;
			continue;
		} else if (c == 'I' && parserOptions->extensionIf) {
			parseIf(program, lineReader, parserOptions);
			program->innerProgram = newProgram();
			push(&programStack, program);
			program = program->innerProgram;
			continue;
		} else parserError(lineReader, "Expected beginning of instruction");
		
		end_of_instruction:
		c = getChar(lineReader);
		if (c == ';') {
			program->nextProgram = newProgram();
			program = program->nextProgram;
		} else if (c == 'E') {
			if (count == 0 && !parserOptions->noWhitespace) parserError(lineReader, "Expected whitespace");
			c = getChar(lineReader);
			if (c == 'N') {
				consumeString(lineReader, "D");
				program = pop(&programStack);
				if (program == NULL) parserError(lineReader, "Unexpected END token");
				count = consumeWhitespace(lineReader, 0, parserOptions);
				goto end_of_instruction;
			} else if (parserOptions->extensionIfExtended) {
				if (c == 'L') {
					consumeString(lineReader, "SE");
					program = pop(&programStack);
					if (program->instructionType != ifInstructionStart) parserError(lineReader, "Unexpected ELSE token");
					consumeWhitespace(lineReader, 1, parserOptions);
					program->nextProgram = newProgram();
					program = program->nextProgram;
					program->instructionType = ifInstructionEnd;
					program->innerProgram = newProgram();
					push(&programStack, program);
					program = program->innerProgram;
					continue;
				} else parserError(lineReader, "Expected 'N' or 'L'");
			} else parserError(lineReader, "Expected 'N'");
		} else if (c == EOF) {
			if (pop(&programStack) != NULL) parserError(lineReader, "Unexpected end of file");
			break;
		} else {
			if (pop(&programStack) == NULL) parserError(lineReader, "Expected ';' or end of file");
			else parserError(lineReader, "Expected ';' or \"END\"");
		}
	}
	
	freeLineReader(lineReader);
	freeProgramStack(programStack);
	return ret;
}

void writeIncludes(FILE *output) {
	const char *includes =
		"#include <stdlib.h>\n"
		"#include <stdio.h>\n"
		"#include <string.h>\n"
		"#include <inttypes.h>\n";
	fprintf(output, includes);
}

void writeStart(FILE *output, char *functionName) {
	const char *start =
		"\n"
		"%s %s(%s argc, %s *argv) {\n"
		"\t%s *x = calloc(%d, sizeof(%s));\n"
		"\t%s n = argc < %d ? argc : %d;\n"
		"\tmemcpy(x + 1, argv, n * sizeof(%s));\n";
	fprintf(output, start, type, functionName, type, type, type, heighestIndex + 1, type, type, heighestIndex, heighestIndex, type);
}

void writeEnd(FILE *output, char *functionName) {
	const char *end =
		"\t\n"
		"\t\n"
		"\t%s ret = x[0];\n"
		"\tfree(x);\n"
		"\treturn ret;\n"
		"}\n"
		"\n"
		"int main(int argc, char **argv) {\n"
		"\t%s *arr = malloc((argc - 1) * sizeof(%s));\n"
		"\tfor (int i = 0; i < argc - 1; ++i) {\n"
		"\t\tarr[i] = atoi(argv[i + 1]);\n"
		"\t}\n"
		"\t%s res = %s(argc - 1, arr);\n"
		"\tfree(arr);\n"
		"\tprintf(\"%%\" %s \"\\n\", res);\n"
		"\treturn 0;\n"
		"}";
	fprintf(output, end, type, type, type, type, functionName, typePrintMacro);
}

void writeIndentation(int indentation, FILE *output) {
	fputc('\n', output);
	for (int i = 0; i < indentation; ++i)
		fputc('\t', output);
}

void writeAssignment(Program *program, FILE *output) {
	switch (program->operation) {
	case constant:
		fprintf(output, "x[%d] = %d;", program->i, program->c);
		break;
	case variable:
		fprintf(output, "x[%d] = x[%d];", program->i, program->j);
		break;
	case plus:
		fprintf(output, program->treatCAsVariable ? "x[%d] = x[%d] + x[%d];" : "x[%d] = x[%d] + %d;", program->i, program->j, program->c);
		break;
	case minus:
		fprintf(output, program->treatCAsVariable ? "x[%d] = x[%d] > x[%d] ? x[%d] - x[%d] : 0;" : "x[%d] = x[%d] > %d ? x[%d] - %d : 0;", program->i, program->j, program->c, program->j, program->c);
		break;
	case times:
		fprintf(output, program->treatCAsVariable ? "x[%d] = x[%d] * x[%d];" : "x[%d] = x[%d] * %d;", program->i, program->j, program->c);
		break;
	case dividedBy:
		fprintf(output, program->treatCAsVariable ? "x[%d] = x[%d] / x[%d];" : "x[%d] = x[%d] / %d;", program->i, program->j, program->c);
		break;
	case modulo:
		fprintf(output, program->treatCAsVariable ? "x[%d] = x[%d] %% x[%d];" : "x[%d] = x[%d] %% %d;", program->i, program->j, program->c);
		break;
	default:
		error("Encountered assignment with undefined operation");
	}
}

void writeLoop(Program *program, FILE *output) {
	fprintf(output, "for (int i = x[%d]; i; --i) {", program->i);
}

void writeWhile(Program *program, FILE *output) {
	char *relation;
	switch (program->operation) {
	case equal:
		relation = "==";
		break;
	case notEqual:
		relation = "!=";
		break;
	case greater:
		relation = ">";
		break;
	case greaterEqual:
		relation = ">=";
		break;
	case less:
		relation = "<";
		break;
	case lessEqual:
		relation = "<=";
		break;
	default:
		error("Encountered WHILE with undefined relation");
	}
	fprintf(output, program->treatCAsVariable ? "while (x[%d] %s x[%d]) {" : "while (x[%d] %s %d) {", program->i, relation, program->c);
}

void writeIfStart(Program *program, FILE *output) {
	char *relation;
	switch (program->operation) {
	case equal:
		relation = "==";
		break;
	case notEqual:
		relation = "!=";
		break;
	case greater:
		relation = ">";
		break;
	case greaterEqual:
		relation = ">=";
		break;
	case less:
		relation = "<";
		break;
	case lessEqual:
		relation = "<=";
		break;
	default:
		error("Encountered IF with undefined relation");
	}
	fprintf(output, program->treatCAsVariable ? "if (x[%d] %s x[%d]) {" : "if (x[%d] %s %d) {", program->i, relation, program->c);
}

void writeIfEnd(Program *program, FILE *output) {
	fprintf(output, "else {");
}

void writeInstruction(Program *program, FILE *output) {
	switch (program->instructionType) {
	case assignment:
		writeAssignment(program, output);
		break;
	case loopInstruction:
		writeLoop(program, output);
		break;
	case whileInstruction:
		writeWhile(program, output);
		break;
	case ifInstructionStart:
		writeIfStart(program, output);
		break;
	case ifInstructionEnd:
		writeIfEnd(program, output);
		break;
	default:
		error("Encountered Instruction of undefined type");
	}
}

void writeLoopEnd(FILE *output) {
	fprintf(output, "}");
}

void writeHeader(WriteOptions *writeOptions, FILE *output) {
	char *name = writeOptions->outputFileName;
	int length = strlen(name);
	name[length - 1] = 'h';
	fprintf(output, "#include \"%s\"\n", name);
	output = fopen(name, "w");
	if (output == NULL) error(strerror(errno));
	name[length - 1] = 'c';
	char *functionName = writeOptions->functionName;
	
	const char *headerText =
		"#ifndef LOOP_%s_H\n"
		"#define LOOP_%s_H\n"
		"\n"
		"%s %s(%s argc, %s *argv);\n"
		"\n"
		"#endif";
	fprintf(output, headerText, functionName, functionName, type, functionName, type, type);
	
	fclose(output);
}

void writeProgram(Program *program, WriteOptions *writeOptions) {
	if (program == NULL) error("Encountered empty program");
	FILE *output = fopen(writeOptions->outputFileName, "w");
	if (output == NULL) error(strerror(errno));
	writeIncludes(output);
	if (writeOptions->extensionHeader)
		writeHeader(writeOptions, output);
	writeStart(output, writeOptions->functionName);
	int indentation = 1;
	ProgramStack *programStack = newProgramStack();
	
	while (1) {
		if (program->instructionType == ifInstructionEnd)
			fprintf(output, " ");
		else
			writeIndentation(indentation, output);
		writeInstruction(program, output);
		if (program->innerProgram != NULL) {
			push(&programStack, program);
			program = program->innerProgram;
			++indentation;
		} else {
			while (program->nextProgram == NULL) {
				program = pop(&programStack);
				if (program == NULL) break;
				--indentation;
				writeIndentation(indentation, output);
				writeLoopEnd(output);
			}
			if (program == NULL) break;
			program = program->nextProgram;
		}
	}
	
	freeProgramStack(programStack);
	writeEnd(output, writeOptions->functionName);
	fclose(output);
}

int main(int argc, char **argv) {
	ParserOptions parserOptions = {NULL, 0, 0, 0, 0, 0, 0, 0};
	WriteOptions writeOptions = {file, name, 0};
	handleArguments(argc, argv, &parserOptions, &writeOptions);
	Program *program = parse(&parserOptions);
	writeProgram(program, &writeOptions);
	freeProgram(program);
	free(writeOptions.outputFileName);
	return EXIT_SUCCESS;
}