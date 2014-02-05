package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"strings"
	"unicode"
	"unicode/utf8"
)

type stateFn func(*lexer) stateFn

type tokenType int

const (
	None tokenType = iota
	Value
	LBracket
	RBracket
	LBrace
	RBrace
	Colon
	Comma
)

type lexer struct {
	input     string
	start     int
	pos       int
	width     int
	token     tokenType
	tokenText string
}

func (l *lexer) run() (tokenType, string) {
	l.token = None
	l.tokenText = ""
	for state := lexWhitespace; state != nil; {
		state = state(l)
	}
	return l.token, l.tokenText
}

func (l *lexer) next() rune {
	if l.pos >= len(l.input) {
		l.width = 0
		return 0
	}
	r, w := utf8.DecodeRuneInString(l.input[l.pos:])
	l.width = w
	l.pos += l.width
	return r
}

func (l *lexer) peek() rune {
	r := l.next()
	l.pos -= l.width
	return r
}

func (l *lexer) skip() {
	l.start = l.pos
}

func (l *lexer) backup() {
	l.pos -= l.width
}

func (l *lexer) emit(token tokenType) {
	l.tokenText = l.input[l.start:l.pos]
	l.token = token
	l.start = l.pos
}

func (l *lexer) error(msg string) {
	fmt.Print("Lex Error: ", msg, "\n")
}

func (l *lexer) accept(valid string) bool {
	if strings.IndexRune(valid, l.next()) >= 0 {
		return true
	}
	l.backup()
	return false
}

func (l *lexer) acceptRun(valid string) bool {
	accept := false
	for strings.IndexRune(valid, l.next()) >= 0 {
		accept = true
	}
	l.backup()
	return accept
}

func lexWhitespace(l *lexer) stateFn {
	l.acceptRun(" \t\r\n")
	l.skip()
	return lexText
}

func lexText(l *lexer) stateFn {
	switch {
	case l.accept("["):
		l.emit(LBracket)
		return nil
	case l.accept("]"):
		l.emit(RBracket)
		return nil
	case l.accept("{"):
		l.emit(LBrace)
		return nil
	case l.accept("}"):
		l.emit(RBrace)
		return nil
	case l.accept(","):
		l.emit(Comma)
		return nil
	case l.accept(":"):
		l.emit(Colon)
		return nil
	case l.accept("\""):
		return lexString
	default:
		return lexName
	}
}

func lexString(l *lexer) stateFn {
	for {
		switch l.next() {
		case '"':
			l.emit(Value)
			return nil
		case '\r', '\n':
			l.error("Newline in string")
			return nil
		case 0:
			l.error("EOF in string")
			return nil
		}
	}
}

func lexName(l *lexer) stateFn {
	for {
		r := l.next()
		if r == 0 || unicode.IsSpace(r) || r == '{' || r == '}' || r == '[' || r == ']' || r == ':' || r == ',' {
			l.backup()
			if l.pos > l.start {
				l.emit(Value)
			}
			return nil
		}
	}
}

func lex(input string) {
	l := &lexer{
		input: input,
	}
	for {
		token, _ := l.run()
		if token == None {
			break
		}
	}
}

type parseFn func(*parser) parseFn

type parser struct {
	token           tokenType
	tokenText       string
	lexer           *lexer
	backupToken     tokenType
	backupTokenText string
}

func (p *parser) next() tokenType {
	if p.backupToken != None {
		p.token = p.backupToken
		p.tokenText = p.backupTokenText
		p.backupToken = None
		p.backupTokenText = ""
	} else {
		p.token, p.tokenText = p.lexer.run()
	}
	return p.token
}

func (p *parser) accept(t tokenType) bool {
	v := p.next()
	fmt.Print("v: ", p.tokenText, "\n")
	if v == t {
		return true
	}
	p.backup()
	return false
}

func (p *parser) backup() {
	p.backupToken = p.token
	p.backupTokenText = p.tokenText
	p.token = None
	p.tokenText = ""
}

func parse(input string) interface{} {
	p := &parser{
		lexer: &lexer{
			input: input,
		},
	}
	return parseValue(p)
}

func parseObject(p *parser) map[string]interface{} {
	result := map[string]interface{}{}
	p.accept(LBrace)
	first := true
	for {
		if p.accept(RBrace) {
			return result
		}
		if first || p.accept(Comma) {
			p.accept(Value)
			k := p.tokenText
			p.accept(Colon)
			v := parseValue(p)
			result[k] = v
		}
		first = false
	}
}

func parseArray(p *parser) []interface{} {
	result := []interface{}{}
	p.accept(LBracket)
	first := true
	for {
		if p.accept(RBracket) {
			return result
		}
		if first || p.accept(Comma) {
			result = append(result, parseValue(p))
		}
		first = false
	}
}

func stripQuotes(s string) string {
	if len(s) > 2 && s[0] == '"' && s[len(s)-1] == '"' && !strings.ContainsAny(s, "\\{}[] \t") {
		s = s[1 : len(s)-1]
	}
	return s
}

func parseValue(p *parser) interface{} {
	t := p.next()
	switch t {
	case Value:
		return stripQuotes(p.tokenText)
	case LBrace:
		return parseObject(p)
	case LBracket:
		return parseArray(p)
	}
	return nil
}

func indent(depth int) {
	for i := 0; i < depth; i++ {
		fmt.Print("\t")
	}
}

func printValue(v interface{}, depth int) {
	switch t := v.(type) {
	case string:
		fmt.Print(t)
	case map[string]interface{}:
		fmt.Print("{\n")
		l := 0
		for k, _ := range t {
			if len(k) > l {
				l = len(k)
			}
		}
		for k, v := range t {
			indent(depth + 1)
			fmt.Print(k)
			switch v.(type) {
			case map[string]interface{}, []interface{}:
				fmt.Print(" ")
			default:
				for i := 0; i < l-len(k)+1; i++ {
					fmt.Print(" ")
				}
			}
			printValue(v, depth+1)
			fmt.Print("\n")
		}
		indent(depth)
		fmt.Print("}")
	case []interface{}:
		fmt.Print("[\n")
		for _, e := range t {
			indent(depth + 1)
			printValue(e, depth+1)
			fmt.Print("\n")
		}
		indent(depth)
		fmt.Print("]")
	}
}

func main() {
	file, _ := os.Open(os.Args[1])
	data, _ := ioutil.ReadAll(file)
	r := parse(string(data))
	fmt.Print(r)
	printValue(r, 0)
	return
}
